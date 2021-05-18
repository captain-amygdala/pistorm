#!/usr/bin/python3
# -*- coding: utf-8 -*-

# Copyright (c) 2018-2021 Niklas Ekström

import select
import sys
import socket
import threading
import time
import os
import struct
import pty
import signal
import termios
import fcntl
import logging
import json

logging.basicConfig(format = '%(levelname)s, %(asctime)s, %(name)s, line %(lineno)d: %(message)s')
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

FS_CFG_FILE = '/etc/opt/a314/a314fs.conf'
PICMD_CFG_FILE = '/etc/opt/a314/picmd.conf'

volume_paths = {}
search_path = ''
env_vars = {}
sgr_map = {}

def load_cfg():
    with open(FS_CFG_FILE, 'rt') as f:
        cfg = json.load(f)
        devs = cfg['devices']
        for _, dev in devs.items():
            volume_paths[dev['volume']] = dev['path']

    global search_path
    search_path = os.getenv('PATH')

    with open(PICMD_CFG_FILE, 'rt') as f:
        cfg = json.load(f)

        if 'paths' in cfg:
            search_path = ':'.join(cfg['paths']) + ':' + search_path
            os.environ['PATH'] = search_path

        if 'env_vars' in cfg:
            for key, val in cfg['env_vars'].items():
                env_vars[key] = val

        if 'sgr_map' in cfg:
            for key, val in cfg['sgr_map'].items():
                sgr_map[key] = str(val)

load_cfg()

MSG_REGISTER_REQ        = 1
MSG_REGISTER_RES        = 2
MSG_DEREGISTER_REQ      = 3
MSG_DEREGISTER_RES      = 4
MSG_READ_MEM_REQ        = 5
MSG_READ_MEM_RES        = 6
MSG_WRITE_MEM_REQ       = 7
MSG_WRITE_MEM_RES       = 8
MSG_CONNECT             = 9
MSG_CONNECT_RESPONSE    = 10
MSG_DATA                = 11
MSG_EOS                 = 12
MSG_RESET               = 13

def wait_for_msg():
    header = b''
    while len(header) < 9:
        data = drv.recv(9 - len(header))
        if not data:
            logger.error('Connection to a314d was closed, terminating.')
            exit(-1)
        header += data
    (plen, stream_id, ptype) = struct.unpack('=IIB', header)
    payload = b''
    while len(payload) < plen:
        data = drv.recv(plen - len(payload))
        if not data:
            logger.error('Connection to a314d was closed, terminating.')
            exit(-1)
        payload += data
    return (stream_id, ptype, payload)

def send_register_req(name):
    m = struct.pack('=IIB', len(name), 0, MSG_REGISTER_REQ) + name
    drv.sendall(m)

def send_read_mem_req(address, length):
    m = struct.pack('=IIBII', 8, 0, MSG_READ_MEM_REQ, address, length)
    drv.sendall(m)

def read_mem(address, length):
    send_read_mem_req(address, length)
    stream_id, ptype, payload = wait_for_msg()
    if ptype != MSG_READ_MEM_RES:
        logger.error('Expected MSG_READ_MEM_RES but got %s. Shutting down.', ptype)
        exit(-1)
    return payload

def send_write_mem_req(address, data):
    m = struct.pack('=IIBI', 4 + len(data), 0, MSG_WRITE_MEM_REQ, address) + data
    drv.sendall(m)

def write_mem(address, data):
    send_write_mem_req(address, data)
    stream_id, ptype, payload = wait_for_msg()
    if ptype != MSG_WRITE_MEM_RES:
        logger.error('Expected MSG_WRITE_MEM_RES but got %s. Shutting down.', ptype)
        exit(-1)

def send_connect_response(stream_id, result):
    m = struct.pack('=IIBB', 1, stream_id, MSG_CONNECT_RESPONSE, result)
    drv.sendall(m)

def send_data(stream_id, data):
    m = struct.pack('=IIB', len(data), stream_id, MSG_DATA) + data
    drv.sendall(m)

def send_eos(stream_id):
    m = struct.pack('=IIB', 0, stream_id, MSG_EOS)
    drv.sendall(m)

def send_reset(stream_id):
    m = struct.pack('=IIB', 0, stream_id, MSG_RESET)
    drv.sendall(m)

sessions = {}

class PiCmdSession(object):
    def __init__(self, stream_id):
        self.stream_id = stream_id
        self.pid = 0

        self.first_packet = True
        self.reset_after = None

        self.rasp_was_esc = False
        self.rasp_in_cs = False
        self.rasp_holding = ''

        self.amiga_in_cs = False
        self.amiga_holding = ''

    def process_amiga_ansi(self, data):
        data = data.decode('latin-1')
        out = ''
        for c in data:
            if not self.amiga_in_cs:
                if c == '\x9b':
                    self.amiga_in_cs = True
                    self.amiga_holding = '\x1b['
                else:
                    out += c
            else: # self.amiga_in_cs
                self.amiga_holding += c
                if c >= chr(0x40) and c <= chr(0x7e):
                    if c == 'r':
                        # Window Bounds Report
                        # ESC[1;1;rows;cols r
                        rows, cols = map(int, self.amiga_holding[6:-2].split(';'))
                        winsize = struct.pack('HHHH', rows, cols, 0, 0)
                        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, winsize)
                    elif c == '|':
                        # Input Event Report
                        # ESC[12;0;0;x;x;x;x;x|
                        # Window resized
                        send_data(self.stream_id, b'\x9b' + b'0 q')
                    else:
                        out += self.amiga_holding
                    self.amiga_holding = ''
                    self.amiga_in_cs = False
        if len(out) != 0:
            os.write(self.fd, out.encode('utf-8'))

    def process_msg_data(self, data):
        if self.first_packet:
            if len(data) != 8:
                send_reset(self.stream_id)
                del sessions[self.stream_id]
            else:
                address, length = struct.unpack('>II', data)
                buf = read_mem(address, length)

                ind = 0
                rows, cols = struct.unpack('>HH', buf[ind:ind+4])
                ind += 4

                component_count = buf[ind]
                ind += 1

                components = []
                for _ in range(component_count):
                    n = buf[ind]
                    ind += 1
                    components.append(buf[ind:ind+n].decode('latin-1'))
                    ind += n

                arg_count = buf[ind]
                ind += 1

                args = []
                for _ in range(arg_count):
                    n = buf[ind]
                    ind += 1
                    args.append(buf[ind:ind+n].decode('latin-1'))
                    ind += n

                if arg_count == 0:
                    args.append('bash')

                self.pid, self.fd = pty.fork()
                if self.pid == 0:
                    for key, val in env_vars.items():
                        os.putenv(key, val)
                    os.putenv('PATH', search_path)
                    os.putenv('TERM', 'ansi')
                    winsize = struct.pack('HHHH', rows, cols, 0, 0)
                    fcntl.ioctl(sys.stdin, termios.TIOCSWINSZ, winsize)
                    if component_count != 0 and components[0] in volume_paths:
                        path = volume_paths[components[0]]
                        os.chdir(os.path.join(path, *components[1:]))
                    else:
                        os.chdir(os.getenv('HOME', '/'))
                    os.execvp(args[0], args)

                self.first_packet = False

        elif self.pid:
            self.process_amiga_ansi(data)

    def close(self):
        if self.pid:
            os.kill(self.pid, signal.SIGTERM)
            self.pid = 0
            os.close(self.fd)
        del sessions[self.stream_id]

    def process_rasp_ansi(self, text):
        text = text.decode('utf-8')
        out = ''
        for c in text:
            if not self.rasp_in_cs:
                if not self.rasp_was_esc:
                    if c == '\x1b':
                        self.rasp_was_esc = True
                    else:
                        out += c
                else: # self.rasp_was_esc
                    if c == '[':
                        self.rasp_was_esc = False
                        self.rasp_in_cs = True
                        self.rasp_holding = '\x1b['
                    elif c == '\x1b':
                        out += '\x1b'
                    else:
                        out += '\x1b'
                        out += c
                        self.rasp_was_esc = False
            else: # self.rasp_in_cs
                self.rasp_holding += c
                if c >= chr(0x40) and c <= chr(0x7e):
                    if c == 'm':
                        # Select Graphic Rendition
                        # ESC[30;37m
                        attrs = self.rasp_holding[2:-1].split(';')
                        attrs = [sgr_map[a] if a in sgr_map else a for a in attrs]
                        out += '\x1b[' + (';'.join(attrs)) + 'm'
                    else:
                        out += self.rasp_holding
                    self.rasp_holding = ''
                    self.rasp_in_cs = False
        return out.encode('latin-1', 'replace')

    def handle_text(self):
        try:
            text = os.read(self.fd, 1024)
            text = self.process_rasp_ansi(text)
            while len(text) > 0:
                take = min(len(text), 252)
                send_data(self.stream_id, text[:take])
                text = text[take:]
        except:
            #os.close(self.fd)
            os.kill(self.pid, signal.SIGTERM)
            self.pid = 0
            send_eos(self.stream_id)
            self.reset_after = time.time() + 10

    def handle_timeout(self):
        if self.reset_after and self.reset_after < time.time():
            send_reset(self.stream_id)
            del sessions[self.stream_id]

    def fileno(self):
        return self.fd

def process_drv_msg(stream_id, ptype, payload):
    if ptype == MSG_CONNECT:
        if payload == b'picmd':
            s = PiCmdSession(stream_id)
            sessions[stream_id] = s
            send_connect_response(stream_id, 0)
        else:
            send_connect_response(stream_id, 3)
    elif stream_id in sessions:
        s = sessions[stream_id]

        if ptype == MSG_DATA:
            s.process_msg_data(payload)
        elif ptype == MSG_EOS:
            if s.pid:
                send_eos(s.stream_id)
            s.close()
        elif ptype == MSG_RESET:
            s.close()

done = False

try:
    idx = sys.argv.index('-ondemand')
except ValueError:
    idx = -1

if idx != -1:
    fd = int(sys.argv[idx + 1])
    drv = socket.socket(fileno=fd)
else:
    drv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    drv.connect(('localhost', 7110))
    drv.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

    send_register_req(b'picmd')
    _, _, payload = wait_for_msg()
    if payload[0] != 1:
        logger.error('Unable to register picmd with driver, shutting down')
        drv.close()
        done = True

rbuf = b''

if not done:
    logger.info('picmd server is running')

while not done:
    sel_fds = [drv] + [s for s in sessions.values() if s.pid]
    if idx == -1:
        sel_fds.append(sys.stdin)
    rfd, wfd, xfd = select.select(sel_fds, [], [], 5.0)

    for fd in rfd:
        if fd == sys.stdin:
            line = sys.stdin.readline()
            if not line or line.startswith('quit'):
                for s in sessions.values():
                    s.close()
                drv.close()
                done = True
        elif fd == drv:
            buf = drv.recv(1024)
            if not buf:
                for s in sessions.values():
                    s.close()
                drv.close()
                done = True
            else:
                rbuf += buf
                while True:
                    if len(rbuf) < 9:
                        break

                    (plen, stream_id, ptype) = struct.unpack('=IIB', rbuf[:9])
                    if len(rbuf) < 9 + plen:
                        break

                    rbuf = rbuf[9:]
                    payload = rbuf[:plen]
                    rbuf = rbuf[plen:]

                    process_drv_msg(stream_id, ptype, payload)
        else:
            fd.handle_text()

    for s in sessions.values():
        s.handle_timeout()
