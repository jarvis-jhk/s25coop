#!/usr/bin/env python3
# Copyright (C) 2026 s25coop contributors
# SPDX-License-Identifier: GPL-2.0-or-later
"""Finds the original Settlers II files in a download and copies them into the s25coop data folder.

    s2-extract.py find <folder>...        list the downloads that look like Settlers II, best first
    s2-extract.py extract <dest> <file>   copy DATA, GFX and VIDEO out of one download into <dest>
    s2-extract.py auto <dest> <folder>... try every download of `find` until one works

Only the given folders are looked at (the installer passes the user's Downloads folder), and only
one level deep: never a whole-disk search. Understood: the GOG installer (setup_the_settlers_2_gold_*.exe,
unpacked with innoextract), disc images (.iso, .bin/.cue, .img/.ccd, .mdf, .nrg), and .zip archives
holding either a disc image or an installed game. .7z needs 7z, 7za, 7zz or bsdtar on the system.
Standard library only: this runs on a stock Steam Deck. Exit status: 0 copied, 2 nothing usable, 1 error.
"""
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile

# What a Settlers II download is called: GOG first (the one we can recommend), then the names
# the copies on archive.org go by. The installer's help page lists these, so keep them readable.
GOG_PATTERN = re.compile(r'^setup_(the_settlers_2|die_siedler_2)[a-z0-9_]*.*\.exe$', re.I)
KNOWN_NAMES = [
    'The Settlers II Gold.zip', 'The Settlers II Gold.bin', 'Settlers_II_The_Gold_Edition_1997.zip',
    'siedler2gold.7z', 'Siedler2-Gold_mdf.zip', 'Siedler2-Gold_nrg.zip', 'siedler2.zip', 'Siedler2.bin',
    'SIEDLER2.bin', 'Die Siedler II.img', 'The Settlers II.img', 'Settlers2MASTER.bin',
    'Game4U_12_Settlers-II_cd.bin', 'CD01.img',
]
ARCHIVE_EXT = ('.zip', '.7z', '.iso', '.bin', '.img', '.mdf', '.nrg', '.exe')
NAME_HINT = re.compile(r'settler|siedler', re.I)
WANTED = ('DATA', 'GFX', 'VIDEO')  # VIDEO holds the intro; DATA and GFX are required


def log(*a):
    print(*a, file=sys.stderr, flush=True)


class NotS2(Exception):
    """The download is readable but holds no Settlers II game."""


# ---------------------------------------------------------------- finding downloads

def rank(path):
    name = os.path.basename(path)
    if GOG_PATTERN.match(name):
        return 0
    if name.lower() in (n.lower() for n in KNOWN_NAMES):
        return 1
    return 2


def is_candidate(path):
    name = os.path.basename(path)
    if os.path.isdir(path):
        return find_s2_dir(path, 2) is not None
    low = name.lower()
    if not low.endswith(ARCHIVE_EXT):
        return False
    if low.endswith('.exe'):
        return bool(GOG_PATTERN.match(name))
    return bool(NAME_HINT.search(name)) or low in (n.lower() for n in KNOWN_NAMES)


def find(folders):
    found = []
    for folder in folders:
        try:
            entries = os.listdir(folder)
        except OSError:
            continue
        for e in entries:
            p = os.path.join(folder, e)
            if not e.endswith(('.part', '.crdownload')) and is_candidate(p):
                found.append(p)
    return sorted(found, key=lambda p: (rank(p), -os.path.getmtime(p)))


# ---------------------------------------------------------------- directory trees

def child(dirpath, name):
    """The entry of dirpath called name, ignoring case (copies from discs and GOG differ)."""
    try:
        for e in os.listdir(dirpath):
            if e.lower() == name.lower():
                return os.path.join(dirpath, e)
    except OSError:
        pass
    return None


def find_s2_dir(root, depth=5):
    """The folder under root (or root itself) that has DATA and GFX in it."""
    for dirpath, dirnames, _ in os.walk(root):
        if dirpath[len(root):].count(os.sep) >= depth:
            dirnames[:] = []
            continue
        names = {d.lower() for d in dirnames}
        if 'data' in names and 'gfx' in names:
            return dirpath
    return None


def copy_tree(src_root, dest):
    s2 = find_s2_dir(src_root)
    if not s2:
        raise NotS2('no DATA and GFX folders in it')
    for want in WANTED:
        src = child(s2, want)
        if src and os.path.isdir(src):
            target = os.path.join(dest, want)
            shutil.rmtree(target, ignore_errors=True)
            shutil.copytree(src, target)
    return s2


# ---------------------------------------------------------------- ISO 9660 disc images

class DiscImage:
    """Reads the data track of a CD image, whatever the sector format (cooked 2048, raw 2352 MODE1/MODE2,
    2448 with subchannel, Nero's 150-sector offset). No Joliet: the game's names are plain 8.3."""
    LAYOUTS = [(2048, 0), (2352, 16), (2352, 24), (2336, 8), (2448, 16), (2448, 24)]

    def __init__(self, path):
        self.f = open(path, 'rb')
        for base in (0, 150 * 2048, 150 * 2352):
            for size, head in self.LAYOUTS:
                self.base, self.size, self.head = base, size, head
                pvd = self.sector(16)
                if pvd[0:6] == b'\x01CD001':
                    self.root = self.record(pvd[156:190])
                    return
        self.f.close()
        raise NotS2('not a CD image with a data track')

    def close(self):
        self.f.close()

    def sector(self, lba):
        self.f.seek(self.base + lba * self.size + self.head)
        return self.f.read(2048)

    @staticmethod
    def record(rec):
        if len(rec) < 34 or len(rec) < 33 + rec[32]:
            raise NotS2('damaged directory record in the disc image')
        lba, length, flags, nlen = struct.unpack_from('<I', rec, 2)[0], struct.unpack_from('<I', rec, 10)[0], rec[25], rec[32]
        name = rec[33:33 + nlen].decode('latin-1').split(';')[0].rstrip('.')
        return {'lba': lba, 'len': length, 'dir': bool(flags & 2), 'name': name}

    @staticmethod
    def safe(name):
        """A name from the disc that is one plain path component (never '..' or with a slash)"""
        return name not in ('', '.', '..') and not re.search(r'[/\\\x00]', name)

    def listdir(self, d):
        out, lba, left = [], d['lba'], d['len']
        while left > 0:
            sec = self.sector(lba)
            pos = 0
            if len(sec) < 2048:
                raise NotS2('the disc image ends early')
            while pos < 2048 and sec[pos] != 0:
                rec = self.record(sec[pos:pos + sec[pos]])
                if rec['name'] not in ('\x00', '\x01') and self.safe(rec['name']):
                    out.append(rec)
                pos += sec[pos]
            lba += 1
            left -= 2048
        return out

    def find_s2(self, d=None, depth=0):
        d = d or self.root
        entries = self.listdir(d)
        names = {e['name'].upper(): e for e in entries if e['dir']}
        if 'DATA' in names and 'GFX' in names:
            return entries
        if depth < 3:
            for e in entries:
                if e['dir']:
                    found = self.find_s2(e, depth + 1)
                    if found:
                        return found
        return None

    def extract(self, d, dest):
        os.makedirs(dest, exist_ok=True)
        for e in self.listdir(d):
            target = os.path.join(dest, e['name'])
            if e['dir']:
                self.extract(e, target)
                continue
            with open(target, 'wb') as out:
                lba, left = e['lba'], e['len']
                while left > 0:
                    out.write(self.sector(lba)[:min(left, 2048)])
                    lba += 1
                    left -= 2048


def extract_image(path, dest):
    try:
        img = DiscImage(path)
    except (struct.error, IndexError) as e:
        raise NotS2('damaged disc image: %s' % e)
    try:
        entries = img.find_s2()
        if not entries:
            raise NotS2('the disc has no DATA and GFX folders')
        for e in entries:
            if e['dir'] and e['name'].upper() in WANTED:
                target = os.path.join(dest, e['name'].upper())
                shutil.rmtree(target, ignore_errors=True)
                img.extract(e, target)
    except (struct.error, IndexError) as e:
        raise NotS2('damaged disc image: %s' % e)
    finally:
        img.close()


# ---------------------------------------------------------------- archives

def extract_zip(path, dest, tmp):
    with zipfile.ZipFile(path) as z:
        names = z.namelist()
        # A disc image (or a nested archive) wins over an installed copy next to it: installed copies
        # often lack VIDEO (the intro), the CD always has it.
        inner = [i for i in z.infolist() if i.filename.lower().endswith(('.iso', '.bin', '.img', '.mdf', '.nrg', '.zip', '.7z'))]
        if inner:
            big = max(inner, key=lambda i: i.file_size)
            log('unpacking', big.filename)
            return extract_any(z.extract(big, tmp), dest, tmp)  # the sanitised path zipfile wrote to
        # An installed game zipped up: take its folders directly
        if any(re.search(r'(^|/)DATA/', n, re.I) for n in names) and any(re.search(r'(^|/)GFX/', n, re.I) for n in names):
            z.extractall(tmp)
            return copy_tree(tmp, dest)
        raise NotS2('no game folders and no disc image in the zip')


def tool(*names):
    for n in names:
        p = shutil.which(n)
        if p:
            return p
    return None


def extract_7z(path, dest, tmp):
    exe = tool('7z', '7za', '7zz', 'bsdtar')
    if not exe:
        raise NotS2('a .7z archive, and neither 7z nor bsdtar is installed to open it')
    out = os.path.join(tmp, 'x7z')
    os.makedirs(out, exist_ok=True)
    cmd = [exe, '-xf', path, '-C', out] if exe.endswith('bsdtar') else [exe, 'x', '-y', '-o' + out, path]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)
    return extract_folder_or_image(out, dest, tmp)


def innoextract():
    here = os.path.dirname(os.path.abspath(__file__))
    for p in (os.environ.get('S25COOP_INNOEXTRACT'), os.path.join(here, 'bin', 'innoextract'), shutil.which('innoextract')):
        if p and os.access(p, os.X_OK):
            return p
    return None


def extract_gog(path, dest, tmp):
    exe = innoextract()
    if not exe:
        raise RuntimeError('innoextract is missing from this s25coop installation')
    out = os.path.join(tmp, 'gog')
    log('unpacking the GOG installer')
    subprocess.run([exe, '--extract', '--silent', '-d', out, path], check=True, stdout=subprocess.DEVNULL)
    return copy_tree(out, dest)


def extract_folder_or_image(folder, dest, tmp):
    if find_s2_dir(folder):
        return copy_tree(folder, dest)
    images = []
    for dirpath, _, files in os.walk(folder):
        images += [os.path.join(dirpath, f) for f in files if f.lower().endswith(('.iso', '.bin', '.img', '.mdf', '.nrg'))]
    if not images:
        raise NotS2('no game folders and no disc image inside')
    return extract_any(max(images, key=os.path.getsize), dest, tmp)


def extract_any(path, dest, tmp):
    low = path.lower()
    if os.path.isdir(path):
        return copy_tree(path, dest)
    if low.endswith('.exe'):
        return extract_gog(path, dest, tmp)
    if low.endswith('.zip'):
        return extract_zip(path, dest, tmp)
    if low.endswith('.7z'):
        return extract_7z(path, dest, tmp)
    return extract_image(path, dest)


def extract(path, dest):
    """Copies DATA, GFX (and VIDEO) into dest. Works in a scratch folder next to dest, so a half-done
    copy never looks like a finished one."""
    parent = os.path.dirname(os.path.abspath(dest))
    os.makedirs(parent, exist_ok=True)
    tmp = tempfile.mkdtemp(prefix='s2-extract-', dir=parent)
    staged = os.path.join(tmp, 'S2')
    try:
        os.makedirs(staged)
        extract_any(path, staged, tmp)
        if not (child(staged, 'DATA') and child(staged, 'GFX')):
            raise NotS2('no DATA and GFX folders in it')
        # Swap the whole folder, so an interrupted copy never leaves a mix of two downloads
        old = os.path.join(tmp, 'old')
        if os.path.lexists(dest):
            os.rename(dest, old)
        try:
            os.rename(staged, dest)
        except OSError:
            if os.path.lexists(old):
                os.rename(old, dest)
            raise
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main(argv):
    if len(argv) >= 2 and argv[0] == 'find':
        for p in find(argv[1:]):
            print(p)
        return 0
    if len(argv) == 3 and argv[0] == 'extract':
        try:
            extract(argv[2], argv[1])
        except NotS2 as e:
            log('%s: %s' % (argv[2], e))
            return 2
        except (OSError, RuntimeError, subprocess.CalledProcessError, zipfile.BadZipFile) as e:
            log('%s: %s' % (argv[2], e))
            return 1
        return 0
    if len(argv) >= 3 and argv[0] == 'auto':
        dest = argv[1]
        failed = False
        for p in find(argv[2:]):
            log('trying', p)
            try:
                extract(p, dest)
                print(p)
                return 0
            except NotS2 as e:
                log('  not usable:', e)
            except Exception as e:  # one broken download must not stop the others from being tried
                log('  failed:', e)
                failed = True
        return 1 if failed else 2
    print(__doc__, file=sys.stderr)
    return 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
