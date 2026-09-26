#!/usr/bin/env python3
# Adds a non-Steam game shortcut to every Steam user's shortcuts.vdf (Steam must be closed).
# Usage: add-to-steam.py <name> <exe> <start dir> <launch options> <icon>
# After ArnoldSmith86/minecraft-splitscreen's add-to-steam.py (MIT).
import glob, os, re, sys, zlib, struct

name, exe, startdir, options, icon = sys.argv[1:6]
appid = (zlib.crc32((exe + name).encode()) & 0xFFFFFFFF) | 0x80000000

def entry(index):
    s = lambda k, v: b'\x01' + k + b'\x00' + v.encode() + b'\x00'
    return (b'\x00' + str(index).encode() + b'\x00'
            + b'\x02appid\x00' + struct.pack('<I', appid)
            + s(b'AppName', name) + s(b'Exe', '"%s"' % exe) + s(b'StartDir', '"%s"' % startdir)
            + s(b'icon', icon) + s(b'LaunchOptions', options)
            + b'\x08')

files = glob.glob(os.path.expanduser('~/.steam/steam/userdata/*/config'))
if not files:
    sys.exit('no Steam user found')
for config in files:
    path = os.path.join(config, 'shortcuts.vdf')
    data = open(path, 'rb').read() if os.path.exists(path) else b'\x00shortcuts\x00\x08\x08'
    if exe.encode() in data:
        continue
    if not data.endswith(b'\x08\x08'):
        sys.exit('unrecognised shortcuts.vdf: ' + path)
    indices = [int(i) for i in re.findall(rb'\x00(\d+)\x00\x02appid', data)]
    data = data[:-2] + entry(max(indices, default=-1) + 1) + b'\x08\x08'
    open(path, 'wb').write(data)
    print('added to', path)
