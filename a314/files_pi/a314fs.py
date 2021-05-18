#!/usr/bin/python3
# -*- coding: utf-8 -*-

# Copyright (c) 2018-2021 Niklas Ekström

import select
import sys
import socket
import time
import os
import struct
import glob
import logging
import json

logging.basicConfig(format = '%(levelname)s, %(asctime)s, %(name)s, line %(lineno)d: %(message)s')
logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)

CONFIG_FILE_PATH = './a314fs.conf'

SHARED_DIRECTORY = '/home/pi/a314shared'
METAFILE_EXTENSION = ':a314'

with open(CONFIG_FILE_PATH, encoding='utf-8') as f:
    cfg = json.load(f)
    devs = cfg['devices']
    dev = devs['PI0']
    SHARED_DIRECTORY = dev['path']

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

ACTION_NIL              = 0
ACTION_GET_BLOCK        = 2
ACTION_SET_MAP          = 4
ACTION_DIE              = 5
ACTION_EVENT            = 6
ACTION_CURRENT_VOLUME   = 7
ACTION_LOCATE_OBJECT    = 8
ACTION_RENAME_DISK      = 9
ACTION_WRITE            = ord('W')
ACTION_READ             = ord('R')
ACTION_FREE_LOCK        = 15
ACTION_DELETE_OBJECT    = 16
ACTION_RENAME_OBJECT    = 17
ACTION_MORE_CACHE       = 18
ACTION_COPY_DIR         = 19
ACTION_WAIT_CHAR        = 20
ACTION_SET_PROTECT      = 21
ACTION_CREATE_DIR       = 22
ACTION_EXAMINE_OBJECT   = 23
ACTION_EXAMINE_NEXT     = 24
ACTION_DISK_INFO        = 25
ACTION_INFO             = 26
ACTION_FLUSH            = 27
ACTION_SET_COMMENT      = 28
ACTION_PARENT           = 29
ACTION_TIMER            = 30
ACTION_INHIBIT          = 31
ACTION_DISK_TYPE        = 32
ACTION_DISK_CHANGE      = 33
ACTION_SET_DATE         = 34
ACTION_SAME_LOCK        = 40
ACTION_SCREEN_MODE      = 994
ACTION_READ_RETURN      = 1001
ACTION_WRITE_RETURN     = 1002
ACTION_FINDUPDATE       = 1004
ACTION_FINDINPUT        = 1005
ACTION_FINDOUTPUT       = 1006
ACTION_END              = 1007
ACTION_SEEK             = 1008
ACTION_TRUNCATE         = 1022
ACTION_WRITE_PROTECT    = 1023
ACTION_EXAMINE_FH       = 1034
ACTION_UNSUPPORTED      = 65535

ERROR_NO_FREE_STORE             = 103
ERROR_TASK_TABLE_FULL           = 105
ERROR_LINE_TOO_LONG             = 120
ERROR_FILE_NOT_OBJECT           = 121
ERROR_INVALID_RESIDENT_LIBRARY  = 122
ERROR_NO_DEFAULT_DIR            = 201
ERROR_OBJECT_IN_USE             = 202
ERROR_OBJECT_EXISTS             = 203
ERROR_DIR_NOT_FOUND             = 204
ERROR_OBJECT_NOT_FOUND          = 205
ERROR_BAD_STREAM_NAME           = 206
ERROR_OBJECT_TOO_LARGE          = 207
ERROR_ACTION_NOT_KNOWN          = 209
ERROR_INVALID_COMPONENT_NAME    = 210
ERROR_INVALID_LOCK              = 211
ERROR_OBJECT_WRONG_TYPE         = 212
ERROR_DISK_NOT_VALIDATED        = 213
ERROR_DISK_WRITE_PROTECTED      = 214
ERROR_RENAME_ACROSS_DEVICES     = 215
ERROR_DIRECTORY_NOT_EMPTY       = 216
ERROR_TOO_MANY_LEVELS           = 217
ERROR_DEVICE_NOT_MOUNTED        = 218
ERROR_SEEK_ERROR                = 219
ERROR_COMMENT_TOO_BIG           = 220
ERROR_DISK_FULL                 = 221
ERROR_DELETE_PROTECTED          = 222
ERROR_WRITE_PROTECTED           = 223
ERROR_READ_PROTECTED            = 224
ERROR_NOT_A_DOS_DISK            = 225
ERROR_NO_DISK                   = 226
ERROR_NO_MORE_ENTRIES           = 232

SHARED_LOCK         = -2
EXCLUSIVE_LOCK      = -1

LOCK_DIFFERENT      = -1
LOCK_SAME           = 0
LOCK_SAME_VOLUME    = 1

MODE_OLDFILE        = 1005  # Open existing file read/write positioned at beginning of file.
MODE_NEWFILE        = 1006  # Open freshly created file (delete old file) read/write, exclusive lock.
MODE_READWRITE      = 1004  # Open old file w/shared lock, creates file if doesn't exist.

OFFSET_BEGINNING    = -1    # Relative to Begining Of File.
OFFSET_CURRENT      = 0     # Relative to Current file position.
OFFSET_END          = 1     # relative to End Of File.

ST_ROOT             = 1
ST_USERDIR          = 2
ST_SOFTLINK         = 3     # looks like dir, but may point to a file!
ST_LINKDIR          = 4     # hard link to dir
ST_FILE             = -3    # must be negative for FIB!
ST_LINKFILE         = -4    # hard link to file
ST_PIPEFILE         = -5

current_stream_id = 0

class ObjectLock(object):
    def __init__(self, key, mode, path):
        self.key = key
        self.mode = mode
        self.path = path
        self.entry_it = None

locks = {}

next_key = 1

def get_key():
    global next_key
    key = next_key
    next_key = 1 if next_key == 0x7fffffff else (next_key + 1)
    while key in locks:
        key += 1
    return key

def find_path(key, name):
    i = name.find(':')
    if i != -1:
        vol = name[:i].lower()
        if vol == '' or vol == 'pi0' or vol == 'pidisk':
            key = 0
        name = name[i + 1:]

    if key == 0:
        cp = ()
    else:
        cp = locks[key].path

    while name:
        i = name.find('/')
        if i == -1:
            comp = name
            name = ''
        else:
            comp = name[:i]
            name = name[i + 1:]

        if len(comp) == 0:
            if len(cp) == 0:
                return None
            cp = cp[:-1]
        else:
            p = '.' if len(cp) == 0 else '/'.join(cp)
            entries = os.listdir(p)
            found = False
            for e in entries:
                if comp.lower() == e.lower():
                    cp = cp + (e,)
                    found = True
                    break
            if not found:
                if len(name) == 0:
                    cp = cp + (comp,)
                else:
                    return None

    return cp

def read_metadata(path):
    protection = 0
    comment = ''

    if not os.path.isfile(path + METAFILE_EXTENSION):
        return (protection, comment)

    try:
        f = open(path + METAFILE_EXTENSION, 'r')
        for line in f:
            if line[0] == 'p':
                try:
                    protection = int(line[1:].strip())
                except ValueError:
                    pass
            elif line[0] == 'c':
                comment = line[1:].strip()[:79]
        f.close()
    except FileNotFoundError:
        pass
    return (protection, comment)

def write_metadata(path, protection=None, comment=None):
    p, c = read_metadata(path)

    if protection == None:
        protection = p
    if comment == None:
        comment = c

    if (p, c) == (protection, comment):
        return True

    try:
        f = open(path + METAFILE_EXTENSION, 'w')
        f.write('p' + str(protection) + '\n')
        f.write('c' + comment + '\n')
        f.close()
    except FileNotFoundError as e:
        logger.warning('Failed to write metadata for file %s: %s', path, e)
        return False
    return True

def process_locate_object(key, mode, name):
    logger.debug('ACTION_LOCATE_OBJECT, key: %s, mode: %s, name: %s', key, mode, name)

    cp = find_path(key, name)

    if cp is None or not (len(cp) == 0 or os.path.exists('/'.join(cp))):
        return struct.pack('>HH', 0, ERROR_OBJECT_NOT_FOUND)

    # TODO: Must check if there is already a lock for this path,
    # and if so, if the locks are compatible.

    key = get_key()
    locks[key] = ObjectLock(key, mode, cp)
    return struct.pack('>HHI', 1, 0, key)

def process_free_lock(key):
    logger.debug('ACTION_FREE_LOCK, key: %s', key)
    if key in locks:
        del locks[key]
    return struct.pack('>HH', 1, 0)

def process_copy_dir(prev_key):
    logger.debug('ACTION_COPY_DIR, prev_key: %s', prev_key)
    ol = locks[prev_key]
    key = get_key()
    locks[key] = ObjectLock(key, ol.mode, ol.path)
    return struct.pack('>HHI', 1, 0, key)

def process_parent(prev_key):
    logger.debug('ACTION_PARENT, prev_key: %s', prev_key)
    ol = locks[prev_key]
    if len(ol.path) == 0:
        key = 0
    else:
        key = get_key()
        locks[key] = ObjectLock(key, SHARED_LOCK, ol.path[:-1])
    return struct.pack('>HHI', 1, 0, key)

def mtime_to_dmt(mtime):
    mtime = int(mtime)
    days = mtime // 86400
    left = mtime - days * 86400
    mins = left // 60
    secs = left - mins * 60
    ticks = secs * 50
    days -= 2922 # Days between 1970-01-01 and 1978-01-01
    days = max(0, days) # If days are before Amiga epoc
    return (days, mins, ticks)

def process_examine_object(key):
    logger.debug('ACTION_EXAMINE_OBJECT, key: %s', key)
    ol = locks[key]

    if len(ol.path) == 0:
        fn = 'PiDisk'
        path = '.'
    else:
        fn = ol.path[-1]
        path = '/'.join(ol.path)

    days, mins, ticks = mtime_to_dmt(os.path.getmtime(path))
    protection, comment = read_metadata(path)

    if os.path.isfile(path):
        size = os.path.getsize(path)
        type_ = ST_FILE
    else:
        size = 0
        type_ = ST_USERDIR
        ol.entry_it = os.scandir(path)

    size = min(size, 2 ** 31 - 1)
    fn = (chr(len(fn)) + fn).encode('latin-1', 'ignore')
    comment = (chr(len(comment)) + comment).encode('latin-1', 'ignore')
    return struct.pack('>HHHhIIIII', 1, 0, 666, type_, size, protection, days, mins, ticks) + fn + comment

def process_examine_next(key, disk_key):
    logger.debug('ACTION_EXAMINE_NEXT, key: %s, disk_key: %s', key, disk_key)
    ol = locks[key]

    if len(ol.path) == 0:
        path = '.'
    else:
        path = '/'.join(ol.path)

    if not os.path.isdir(path):
        return struct.pack('>HH', 0, ERROR_OBJECT_WRONG_TYPE)

    disk_key += 1

    entry = next(ol.entry_it, None)
    while entry and entry.name.endswith(METAFILE_EXTENSION):
        entry = next(ol.entry_it, None)

    if not entry:
        return struct.pack('>HH', 0, ERROR_NO_MORE_ENTRIES)

    fn = entry.name
    path = ('/'.join(ol.path + (fn,)))

    days, mins, ticks = mtime_to_dmt(entry.stat().st_mtime)
    protection, comment = read_metadata(path)

    if os.path.isfile(path):
        size = os.path.getsize(path)
        type_ = ST_FILE
    else:
        size = 0
        type_ = ST_USERDIR

    size = min(size, 2 ** 31 - 1)
    fn = (chr(len(fn)) + fn).encode('latin-1', 'ignore')
    comment = (chr(len(comment)) + comment).encode('latin-1', 'ignore')
    return struct.pack('>HHHhIIIII', 1, 0, disk_key, type_, size, protection, days, mins, ticks) + fn + comment

def process_examine_fh(arg1):
    logger.debug('ACTION_EXAMINE_FH, arg1: %s', arg1)

    fn = open_file_handles[arg1].f.name
    path = os.path.realpath(fn)
    days, mins, ticks = mtime_to_dmt(os.path.getmtime(path))
    protection, comment = read_metadata(path)

    if os.path.isfile(path):
        size = os.path.getsize(path)
        type_ = ST_FILE
    else:
        size = 0
        type_ = ST_USERDIR

    size = min(size, 2 ** 31 - 1)
    fn = (chr(len(fn)) + fn).encode('latin-1', 'ignore')
    comment = (chr(len(comment)) + comment).encode('latin-1', 'ignore')
    return struct.pack('>HHHhIIIII', 1, 0, 666, type_, size, protection, days, mins, ticks) + fn + comment


next_fp = 1

open_file_handles = {}

def get_file_ptr():
    global next_fp
    fp = next_fp
    next_fp = 1 if next_fp == 0x7fffffff else next_fp + 1
    while fp in open_file_handles:
        fp += 1
    return fp

class OpenFileHandle(object):
    def __init__(self, fp, f, p):
        self.fp = fp
        self.f = f
        self.p = p

def process_findxxx(mode, key, name):
    if mode == ACTION_FINDINPUT:
        logger.debug('ACTION_FINDINPUT, key: %s, name: %s', key, name)
    elif mode == ACTION_FINDOUTPUT:
        logger.debug('ACTION_FINDOUTPUT, key: %s, name: %s', key, name)
    elif mode == ACTION_FINDUPDATE:
        logger.debug('ACTION_FINDUPDATE, key: %s, name: %s', key, name)

    cp = find_path(key, name)
    if cp is None:
        return struct.pack('>HH', 0, ERROR_OBJECT_NOT_FOUND)

    path = '/'.join(cp)
    if len(cp) == 0 or os.path.isdir(path):
        return struct.pack('>HH', 0, ERROR_OBJECT_WRONG_TYPE)

    # TODO: Must check if there already exists a non-compatible lock for this path.

    # TODO: This must be handled better. Especially error reporting.

    protection, _ = read_metadata(path)
    try:
        if mode == MODE_OLDFILE:
            f = open(path, 'r+b')
        elif mode == MODE_READWRITE:
            f = open(path, 'r+b')
            if protection & 16:
                protection = protection & 0b11101111
                write_metadata(path, protection=protection)
        elif mode == MODE_NEWFILE:
            if protection & 0x1:
                return struct.pack('>HH', 0, ERROR_DELETE_PROTECTED)
            elif protection & 0x4:
                return struct.pack('>HH', 0, ERROR_WRITE_PROTECTED)
            f = open(path, 'w+b')
            if protection & 16:
                protection = protection & 0b11101111
                write_metadata(path, protection=protection)
    except IOError:
        if mode == MODE_READWRITE:
            try:
                f = open(path, 'w+b')
                if protection & 16:
                    protection = protection & 0b11101111
                    write_metadata(path, protection=protection)
            except IOError:
                return struct.pack('>HH', 0, ERROR_OBJECT_NOT_FOUND)
        else:
            return struct.pack('>HH', 0, ERROR_OBJECT_NOT_FOUND)

    fp = get_file_ptr()
    ofh = OpenFileHandle(fp, f, protection)
    open_file_handles[fp] = ofh

    return struct.pack('>HHI', 1, 0, fp)

def process_read(arg1, address, length):
    logger.debug('ACTION_READ, arg1: %s, address: %s, length: %s', arg1, address, length)
    protection = open_file_handles[arg1].p
    if protection & 0x8:
        return struct.pack('>HH', 0, ERROR_READ_PROTECTED)
    f = open_file_handles[arg1].f
    data = f.read(length)
    if len(data) != 0:
        write_mem(address, data)
    return struct.pack('>HHI', 1, 0, len(data))

def process_write(arg1, address, length):
    logger.debug('ACTION_WRITE, arg1: %s, address: %s, length: %s', arg1, address, length)
    protection = open_file_handles[arg1].p
    if protection & 0x4:
        return struct.pack('>HH', 0, ERROR_WRITE_PROTECTED)
    data = read_mem(address, length)
    f = open_file_handles[arg1].f
    f.seek(0, 1)
    try:
        f.write(data)
    except IOError:
        return struct.pack('>HH', 0, ERROR_DISK_FULL)
    return struct.pack('>HHI', 1, 0, length)

def process_seek(arg1, new_pos, mode):
    logger.debug('ACTION_SEEK, arg1: %s, new_pos: %s, mode: %s', arg1, new_pos, mode)

    f = open_file_handles[arg1].f
    old_pos = f.tell()

    from_what = 0
    if mode == OFFSET_CURRENT:
        from_what = 1
    elif mode == OFFSET_END:
        from_what = 2

    f.seek(new_pos, from_what)

    return struct.pack('>HHi', 1, 0, old_pos)

def process_end(arg1):
    logger.debug('ACTION_END, arg1: %s', arg1)

    if arg1 in open_file_handles:
        f = open_file_handles.pop(arg1).f
        f.close()

    return struct.pack('>HH', 1, 0)

def process_delete_object(key, name):
    logger.debug('ACTION_DELETE_OBJECT, key: %s, name: %s', key, name)

    cp = find_path(key, name)
    if cp is None or len(cp) == 0:
        return struct.pack('>HH', 0, ERROR_OBJECT_NOT_FOUND)

    path = '/'.join(cp)
    is_dir = os.path.isdir(path)

    protection, _ = read_metadata(path)
    if protection & 0x1:
        return struct.pack('>HH', 0, ERROR_DELETE_PROTECTED)

    try:
        if is_dir:
            os.rmdir(path)
        else:
            os.remove(path)
        if os.path.isfile(path + METAFILE_EXTENSION):
            os.remove(path + METAFILE_EXTENSION)
    except:
        if is_dir:
            return struct.pack('>HH', 0, ERROR_DIRECTORY_NOT_EMPTY)
        else:
            return struct.pack('>HH', 0, ERROR_OBJECT_NOT_FOUND)

    return struct.pack('>HH', 1, 0)

def process_rename_object(key, name, target_dir, new_name):
    logger.debug('ACTION_RENAME_OBJECT, key: %s, name: %s, target_dir: %s, new_name: %s', key, name, target_dir, new_name)

    cp1 = find_path(key, name)
    if cp1 is None or len(cp1) == 0:
        return struct.pack('>HH', 0, ERROR_OBJECT_NOT_FOUND)

    from_path = '/'.join(cp1)
    if not os.path.exists(from_path):
        return struct.pack('>HH', 0, ERROR_OBJECT_NOT_FOUND)

    cp2 = find_path(target_dir, new_name)
    if cp2 is None or len(cp2) == 0:
        return struct.pack('>HH', 0, ERROR_OBJECT_NOT_FOUND)

    to_path = '/'.join(cp2)

    if from_path == to_path:
        return struct.pack('>HH', 1, 0)

    if os.path.exists(to_path):
        return struct.pack('>HH', 0, ERROR_OBJECT_EXISTS)

    try:
        os.rename(from_path, to_path)
        if os.path.isfile(from_path + METAFILE_EXTENSION):
            os.rename(from_path + METAFILE_EXTENSION, to_path + METAFILE_EXTENSION)
    except:
        return struct.pack('>HH', 0, ERROR_OBJECT_NOT_FOUND)

    return struct.pack('>HH', 1, 0)

def process_create_dir(key, name):
    logger.debug('ACTION_CREATE_DIR, key: %s, name: %s', key, name)

    cp = find_path(key, name)
    if cp is None or len(cp) == 0:
        return struct.pack('>HH', 0, ERROR_OBJECT_NOT_FOUND)

    try:
        path = '/'.join(cp)
        os.makedirs(path)
    except:
        return struct.pack('>HH', 0, ERROR_OBJECT_NOT_FOUND)

    key = get_key()
    locks[key] = ObjectLock(key, SHARED_LOCK, cp)
    return struct.pack('>HHI', 1, 0, key)

def process_set_protect(key, name, mask):
    logger.debug('ACTION_SET_PROTECT, key: %s, name: %s, mask: %s', key, name, mask)

    cp = find_path(key, name)
    if cp is None or len(cp) == 0:
        return struct.pack('>HH', 0, ERROR_OBJECT_NOT_FOUND)

    path = '/'.join(cp)
    if write_metadata(path, protection=mask):
        return struct.pack('>HH', 1, 0)
    else:
        return struct.pack('>HH', 0, ERROR_OBJECT_NOT_FOUND)

def process_set_comment(key, name, comment):
    logger.debug('ACTION_SET_COMMENT, key: %s, name: %s, comment: %s', key, name, comment)

    if len(comment) > 79:
        return struct.pack('>HH', 0, ERROR_COMMENT_TOO_BIG)

    cp = find_path(key, name)
    if cp is None or len(cp) == 0:
        return struct.pack('>HH', 0, ERROR_OBJECT_NOT_FOUND)

    path = '/'.join(cp)
    if write_metadata(path, comment=comment):
        return struct.pack('>HH', 1, 0)
    else:
        return struct.pack('>HH', 0, ERROR_OBJECT_NOT_FOUND)

def process_same_lock(key1, key2):
    logger.debug('ACTION_SAME_LOCK, key1: %s key2: %s', key1, key2)

    if not (key1 in locks and key2 in locks):
        return struct.pack('>HH', 0, LOCK_DIFFERENT)
    elif locks[key1].path == locks[key2].path:
        return struct.pack('>HH', 1, LOCK_SAME)
    else:
        return struct.pack('>HH', 0, LOCK_SAME_VOLUME)

def process_request(req):
    #logger.debug('len(req): %s, req: %s', len(req), list(req))

    (rtype,) = struct.unpack('>H', req[:2])

    if rtype == ACTION_LOCATE_OBJECT:
        key, mode, nlen = struct.unpack('>IHB', req[2:9])
        name = req[9:9+nlen].decode('latin-1')
        return process_locate_object(key, mode, name)
    elif rtype == ACTION_FREE_LOCK:
        (key,) = struct.unpack('>I', req[2:6])
        return process_free_lock(key)
    elif rtype == ACTION_COPY_DIR:
        (key,) = struct.unpack('>I', req[2:6])
        return process_copy_dir(key)
    elif rtype == ACTION_PARENT:
        (key,) = struct.unpack('>I', req[2:6])
        return process_parent(key)
    elif rtype == ACTION_EXAMINE_OBJECT:
        (key,) = struct.unpack('>I', req[2:6])
        return process_examine_object(key)
    elif rtype == ACTION_EXAMINE_NEXT:
        key, disk_key = struct.unpack('>IH', req[2:8])
        return process_examine_next(key, disk_key)
    elif rtype == ACTION_EXAMINE_FH:
        (arg1,) = struct.unpack('>I', req[2:6])
        return process_examine_fh(arg1)
    elif rtype == ACTION_FINDINPUT or rtype == ACTION_FINDOUTPUT or rtype == ACTION_FINDUPDATE:
        key, nlen = struct.unpack('>IB', req[2:7])
        name = req[7:7+nlen].decode('latin-1')
        return process_findxxx(rtype, key, name)
    elif rtype == ACTION_READ:
        arg1, address, length = struct.unpack('>III', req[2:14])
        return process_read(arg1, address, length)
    elif rtype == ACTION_WRITE:
        arg1, address, length = struct.unpack('>III', req[2:14])
        return process_write(arg1, address, length)
    elif rtype == ACTION_SEEK:
        arg1, new_pos, mode = struct.unpack('>Iii', req[2:14])
        return process_seek(arg1, new_pos, mode)
    elif rtype == ACTION_END:
        (arg1,) = struct.unpack('>I', req[2:6])
        return process_end(arg1)
    elif rtype == ACTION_DELETE_OBJECT:
        key, nlen = struct.unpack('>IB', req[2:7])
        name = req[7:7+nlen].decode('latin-1')
        return process_delete_object(key, name)
    elif rtype == ACTION_RENAME_OBJECT:
        key, target_dir, nlen, nnlen = struct.unpack('>IIBB', req[2:12])
        name = req[12:12+nlen].decode('latin-1')
        new_name = req[12+nlen:12+nlen+nnlen].decode('latin-1')
        return process_rename_object(key, name, target_dir, new_name)
    elif rtype == ACTION_CREATE_DIR:
        key, nlen = struct.unpack('>IB', req[2:7])
        name = req[7:7+nlen].decode('latin-1')
        return process_create_dir(key, name)
    elif rtype == ACTION_SET_PROTECT:
        key, mask, nlen = struct.unpack('>IIB', req[2:11])
        name = req[11:11+nlen].decode('latin-1')
        return process_set_protect(key, name, mask)
    elif rtype == ACTION_SET_COMMENT:
        key, nlen, clen = struct.unpack('>IBB', req[2:8])
        name = req[8:8+nlen].decode('latin-1')
        comment = req[8+nlen:8+nlen+clen].decode('latin-1')
        return process_set_comment(key, name, comment)
    elif rtype == ACTION_SAME_LOCK:
        key1, key2 = struct.unpack('>II', req[2:10])
        return process_same_lock(key1, key2)
    elif rtype == ACTION_UNSUPPORTED:
        (dp_Type,) = struct.unpack('>H', req[2:4])
        logger.warning('Unsupported action %d (Amiga/a314fs)', dp_Type)
        return struct.pack('>HH', 0, ERROR_ACTION_NOT_KNOWN)
    else:
        logger.warning('Unsupported action %d (a314d/a314fs)', rtype)
        return struct.pack('>HH', 0, ERROR_ACTION_NOT_KNOWN)

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

    send_register_req(b'a314fs')
    stream_id, ptype, payload = wait_for_msg()
    if payload[0] != 1:
        logger.error('Unable to register service a314fs, shutting down')
        drv.close()
        done = True

if not done:
    os.chdir(SHARED_DIRECTORY)
    logger.info('a314fs is running, shared directory: %s', SHARED_DIRECTORY)

while not done:
    stream_id, ptype, payload = wait_for_msg()

    if ptype == MSG_CONNECT:
        if payload == b'a314fs':
            if current_stream_id is not None:
                send_reset(current_stream_id)
            current_stream_id = stream_id
            send_connect_response(stream_id, 0)
        else:
            send_connect_response(stream_id, 3)
    elif ptype == MSG_DATA:
        address, length = struct.unpack('>II', payload)
        #logger.debug('address: %s, length: %s', address, length)
        req = read_mem(address + 2, length - 2)
        res = process_request(req)
        write_mem(address + 2, res)
        #write_mem(address, b'\xff\xff')
        send_data(stream_id, b'\xff\xff')
    elif ptype == MSG_EOS:
        if stream_id == current_stream_id:
            logger.debug('Got EOS, stream closed')
            send_eos(stream_id)
            current_stream_id = None
    elif ptype == MSG_RESET:
        if stream_id == current_stream_id:
            logger.debug('Got RESET, stream closed')
            current_stream_id = None
