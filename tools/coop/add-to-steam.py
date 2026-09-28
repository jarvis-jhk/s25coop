#!/usr/bin/env python3
# Adds a non-Steam game shortcut to every Steam user's shortcuts.vdf (Steam must be closed), and gives it
# Settlers II artwork from SteamGridDB, so it looks like the real game in the library and in Game Mode.
#   add-to-steam.py <name> <exe> <start dir> <launch options>
#   add-to-steam.py --artwork-only <name> <exe>   only the pictures, for an entry that exists (Steam may run)
# After ArnoldSmith86/minecraft-splitscreen's add-to-steam.py (MIT).
import glob, os, re, sys, zlib, struct, urllib.request

# The Settlers 2: Gold Edition on SteamGridDB (game 5247477). Steam's file names in config/grid:
# <appid>p = library cover 600x900, <appid> = wide capsule 920x430, _hero = header, _logo, _icon.
ARTWORK = {
    'p.png': 'https://cdn2.steamgriddb.com/grid/c6fff3fd2b60ebf71b3ea44eb4e1036a.png',
    '.jpg': 'https://cdn2.steamgriddb.com/grid/87dab7322c069fc3f8c0d162435b9a4d.jpg',
    '_hero.png': 'https://cdn2.steamgriddb.com/hero/d395294d0a48ec6b7a8f0ac142bed046.png',
    '_logo.png': 'https://cdn2.steamgriddb.com/logo/22defa97e505f07bf06fc69736fef23d.png',
    '_icon.png': 'https://cdn2.steamgriddb.com/icon/a22ede5d703532f281f393a5459571fd.png',
}

artwork_only = sys.argv[1:2] == ['--artwork-only']
args = sys.argv[2:] if artwork_only else sys.argv[1:]
name, exe = args[0], args[1]
startdir, options = (args[2], args[3]) if not artwork_only else ('', '')


def entry(index, appid, icon):
    s = lambda k, v: b'\x01' + k + b'\x00' + v.encode() + b'\x00'
    return (b'\x00' + str(index).encode() + b'\x00'
            + b'\x02appid\x00' + struct.pack('<I', appid)
            + s(b'AppName', name) + s(b'Exe', '"%s"' % exe) + s(b'StartDir', '"%s"' % startdir)
            + s(b'icon', icon) + s(b'LaunchOptions', options)
            + b'\x08')


def existing_appid(data):
    """appid of the shortcut that already starts exe, or None"""
    pos = data.find(exe.encode())
    if pos < 0:
        return None
    key = data.rfind(b'\x02appid\x00', 0, pos)
    return struct.unpack_from('<I', data, key + 7)[0] if key >= 0 else None


def update_entry(data, appid, icon):
    """Gives the existing shortcut of exe the current name and the Settlers II icon"""
    start = data.rfind(b'\x02appid\x00', 0, data.find(exe.encode()))
    end = data.find(b'\x02appid\x00', start + 1)
    end = len(data) if end < 0 else end
    part = data[start:end]
    for key, value in ((rb'AppName', name), (rb'icon', icon)):
        part = re.sub(rb'(?i)\x01' + key + rb'\x00[^\x00]*\x00', lambda m: m.group(0)[:len(key) + 2] + value.encode() + b'\x00', part, count=1)
    return data[:start] + part + data[end:]


def fetch_artwork(grid, appid):
    os.makedirs(grid, exist_ok=True)
    for suffix, url in ARTWORK.items():
        path = os.path.join(grid, '%d%s' % (appid, suffix))
        if os.path.exists(path):
            continue
        try:
            req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0 s25coop-installer'})
            with urllib.request.urlopen(req, timeout=20) as resp:
                data = resp.read()
            with open(path + '.part', 'wb') as out:
                out.write(data)
            os.replace(path + '.part', path)
            print('artwork', path)
        except Exception as e:  # pictures are nice to have; a missing one must not fail the install
            print('artwork failed: %s: %s' % (url, e), file=sys.stderr)


configs = glob.glob(os.path.expanduser('~/.steam/steam/userdata/*/config'))
if not configs:
    sys.exit('no Steam user found')
for config in configs:
    path = os.path.join(config, 'shortcuts.vdf')
    grid = os.path.join(config, 'grid')
    data = open(path, 'rb').read() if os.path.exists(path) else b'\x00shortcuts\x00\x08\x08'
    appid = existing_appid(data)
    if appid is not None and not artwork_only:
        updated = update_entry(data, appid, os.path.join(grid, '%d_icon.png' % appid))
        if updated != data:
            open(path, 'wb').write(updated)
            print('updated', path)
    elif appid is None and not artwork_only:
        appid = (zlib.crc32((exe + name).encode()) & 0xFFFFFFFF) | 0x80000000
        if not data.endswith(b'\x08\x08'):
            sys.exit('unrecognised shortcuts.vdf: ' + path)
        indices = [int(i) for i in re.findall(rb'\x00(\d+)\x00\x02appid', data)]
        icon = os.path.join(grid, '%d_icon.png' % appid)
        data = data[:-2] + entry(max(indices, default=-1) + 1, appid, icon) + b'\x08\x08'
        open(path, 'wb').write(data)
        print('added to', path)
    if appid is not None:
        fetch_artwork(grid, appid)
