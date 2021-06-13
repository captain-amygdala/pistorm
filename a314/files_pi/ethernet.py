#!/usr/bin/python3
# -*- coding: utf-8 -*-

# Copyright (c) 2020 Niklas Ekström

import logging
import os
import pytun
import select
import socket
import struct
import sys
import time

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
    _, ptype, payload = wait_for_msg()
    if ptype != MSG_READ_MEM_RES:
        logger.error('Expected MSG_READ_MEM_RES but got %s. Shutting down.', ptype)
        exit(-1)
    return payload

def send_write_mem_req(address, data):
    m = struct.pack('=IIBI', 4 + len(data), 0, MSG_WRITE_MEM_REQ, address) + data
    drv.sendall(m)

def write_mem(address, data):
    send_write_mem_req(address, data)
    _, ptype, _ = wait_for_msg()
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

### A314 communication routines above. Actual driver below.

current_stream_id = None
done = False
rbuf = b''

DEV_NAME = 'tap0'
SERVICE_NAME = b'ethernet'

READ_FRAME_REQ = 1
WRITE_FRAME_REQ = 2
READ_FRAME_RES = 3
WRITE_FRAME_RES = 4

mem_read_queue = []
mem_write_queue = []

# Can buffer as many frames as fit in memory.
# Maybe should have a limit on the number of buffers?
waiting_read_reqs = []
buffered_frames = []

DROP_START_SECS = 15.0

def process_tap_frame(frame):
    if current_stream_id is None:
        return

    global drop_start
    if drop_start:
        if time.time() < start_time + DROP_START_SECS:
            return
        drop_start = False

    if waiting_read_reqs:
        stream_id, address, length = waiting_read_reqs.pop(0)

        if length < len(frame):
            logger.error('Fatal error, read frame from TAP larger than buffer')

        mem_write_queue.append((stream_id, address, len(frame)))
        send_write_mem_req(address, frame)
    else:
        buffered_frames.append(frame)

def process_stream_data(stream_id, data):
    address, length, kind = struct.unpack('>IHH', data)
    if kind == WRITE_FRAME_REQ:
        mem_read_queue.append((stream_id, address, length))
        send_read_mem_req(address, length)
    elif kind == READ_FRAME_REQ:
        if buffered_frames:
            frame = buffered_frames.pop(0)

            if length < len(frame):
                logger.error('Fatal error, read frame from TAP larger than buffer')

            mem_write_queue.append((stream_id, address, len(frame)))
            send_write_mem_req(address, frame)
        else:
            waiting_read_reqs.append((stream_id, address, length))

def process_read_mem_res(frame):
    tap.write(frame)

    stream_id, address, length = mem_read_queue.pop(0)
    if stream_id == current_stream_id:
        send_data(stream_id, struct.pack('>IHH', address, length, WRITE_FRAME_RES))

def process_write_mem_res():
    stream_id, address, length = mem_write_queue.pop(0)
    if stream_id == current_stream_id:
        send_data(stream_id, struct.pack('>IHH', address, length, READ_FRAME_RES))

def process_drv_msg(stream_id, ptype, payload):
    global current_stream_id

    if ptype == MSG_CONNECT:
        if payload == SERVICE_NAME and current_stream_id is None:
            logger.info('Amiga connected')
            current_stream_id = stream_id
            send_connect_response(stream_id, 0)
        else:
            send_connect_response(stream_id, 3)
    elif ptype == MSG_READ_MEM_RES:
        process_read_mem_res(payload)
    elif ptype == MSG_WRITE_MEM_RES:
        process_write_mem_res()
    elif current_stream_id == stream_id:
        if ptype == MSG_DATA:
            process_stream_data(stream_id, payload)
        elif ptype == MSG_EOS:
            # EOS is not used.
            pass
        elif ptype == MSG_RESET:
            current_stream_id = None
            waiting_read_reqs.clear()
            buffered_frames.clear()
            logger.info('Amiga disconnected')

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

    send_register_req(SERVICE_NAME)
    _, _, payload = wait_for_msg()
    if payload[0] != 1:
        logger.error('Unable to register ethernet with driver, shutting down')
        drv.close()
        done = True

if not done:
    try:
        tap = pytun.TunTapDevice(name=DEV_NAME, flags=pytun.IFF_TAP | pytun.IFF_NO_PI)
    except:
        logger.error('Unable to open tap device at ' + DEV_NAME)
        done = True

    start_time = time.time()
    drop_start = True
        
if not done:
    logger.info('Ethernet service is running')

while not done:
    try:
        rl, _, _ = select.select([drv, tap], [], [], 10.0)
    except KeyboardInterrupt:
        rl = []
        if current_stream_id is not None:
            send_reset(current_stream_id)
        drv.close()
        done = True

    if drv in rl:
        buf = drv.recv(1600)
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

                payload = rbuf[9:9+plen]
                rbuf = rbuf[9+plen:]

                process_drv_msg(stream_id, ptype, payload)

    if tap in rl:
        frame = tap.read(1600)
        process_tap_frame(frame)

tap.close()
logger.info('Ethernet service stopped')
