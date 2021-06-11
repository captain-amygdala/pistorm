#!/usr/bin/python3
# -*- coding: utf-8 -*-

# Copyright (c) 2019 Niklas Ekström

import fcntl
import logging
import os
import select
import socket
import struct
import sys

fcntl.F_SETPIPE_SZ = 1031

logging.basicConfig(format = '%(levelname)s, %(asctime)s, %(name)s, line %(lineno)d: %(message)s')
logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)

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

current_stream_id = None
first_msg = True
raw_received = b''
is_empty = [True, True]

def process_msg_data(payload):
    global ptrs, first_msg, raw_received

    if first_msg:
        ptrs = struct.unpack('>II', payload)
        logger.debug('Received pointers %s', ptrs)
        first_msg = False
        return

    buf_index = payload[0]

    if len(raw_received) < 900*2:
        if not is_empty[buf_index]:
            data = b'\x00' * (900*2)
            send_write_mem_req(ptrs[buf_index], data)
            is_empty[buf_index] = True
    else:
        ldata = raw_received[0:900*2:2]
        rdata = raw_received[1:900*2:2]
        data = ldata + rdata
        raw_received = raw_received[900*2:]
        send_write_mem_req(ptrs[buf_index], data)
        is_empty[buf_index] = False

def process_drv_msg(stream_id, ptype, payload):
    global current_stream_id, first_msg

    if ptype == MSG_CONNECT:
        if payload == b'piaudio' and current_stream_id is None:
            logger.info('Amiga connected')
            current_stream_id = stream_id
            first_msg = True
            send_connect_response(stream_id, 0)
        else:
            send_connect_response(stream_id, 3)
    elif current_stream_id == stream_id:
        if ptype == MSG_DATA:
            process_msg_data(payload)
        elif ptype == MSG_EOS:
            pass
        elif ptype == MSG_RESET:
            current_stream_id = None
            logger.info('Amiga disconnected')

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

    send_register_req(b'piaudio')
    _, _, payload = wait_for_msg()
    if payload[0] != 1:
        logger.error('Unable to register piaudio with driver, shutting down')
        drv.close()
        done = True

rbuf = b''

PIPE_NAME = '/tmp/piaudio_pipe'

if not done:
    exists = True
    try:
        if (os.stat(PIPE_NAME).st_mode & 0o170000) != 0o10000:
            logger.error('A file that is not a named pipe exists at ' + PIPE_NAME)
            done = True
    except:
        exists = False

if not done and not exists:
    try:
        os.mkfifo(PIPE_NAME)
    except:
        logger.error('Unable to create named pipe at ' + PIPE_NAME)
        done = True

if not done:
    try:
        pipe_fd = os.open(PIPE_NAME, os.O_RDONLY | os.O_NONBLOCK)
        fcntl.fcntl(pipe_fd, fcntl.F_SETPIPE_SZ, 4096)
    except:
        logger.error('Unable to open named pipe at ' + PIPE_NAME)
        done = True

if not done:
    logger.info('piaudio service is running')

while not done:
    sel_fds = [drv]

    if idx == -1:
        sel_fds.append(sys.stdin)

    if len(raw_received) < 900*2:
        sel_fds.append(pipe_fd)

    rfd, wfd, xfd = select.select(sel_fds, [], [], 5.0)

    for fd in rfd:
        if fd == sys.stdin:
            line = sys.stdin.readline()
            if not line or line.startswith('quit'):
                if current_stream_id is not None:
                    send_reset(current_stream_id)
                drv.close()
                done = True
        elif fd == drv:
            buf = drv.recv(1024)
            if not buf:
                if current_stream_id is not None:
                    send_reset(current_stream_id)
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
        elif fd == pipe_fd:
            data = os.read(pipe_fd, 900*2)

            if len(data) == 0:
                os.close(pipe_fd)

                l = len(raw_received)
                c = l // (900*2)
                if c * 900*2 < l:
                    raw_received += b'\x00' * ((c + 1) * 900*2 - l)

                pipe_fd = os.open(PIPE_NAME, os.O_RDONLY | os.O_NONBLOCK)
                fcntl.fcntl(pipe_fd, fcntl.F_SETPIPE_SZ, 4096)
            else:
                raw_received += data
