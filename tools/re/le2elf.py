#!/usr/bin/env python3
"""Turn a DOS/4GW Linear Executable (LE) into a 32-bit i386 ELF that Ghidra can load.

Ghidra has no LE loader, so the original Settlers II S2.EXE cannot be analysed
(by Ghidra or REA) as shipped. This keeps every object at its preferred linear
address, applies the internal fixups, and writes one PT_LOAD segment plus one
section per object. Selector fixups are dropped: in a flat DOS/4GW model they
do not affect what the code does.

Usage: le2elf.py S2.EXE out.elf      (prints the object map)
The input and output are the user's own copy of the game: never commit either.
"""
import struct
import sys


def u8(d, o): return d[o]
def u16(d, o): return struct.unpack_from('<H', d, o)[0]
def s16(d, o): return struct.unpack_from('<h', d, o)[0]
def u32(d, o): return struct.unpack_from('<I', d, o)[0]


def load_le(data):
    le = u32(data, 0x3C)
    if data[le:le + 2] != b'LE':
        raise SystemExit('not an LE executable')
    h = lambda off: u32(data, le + off)
    page_size, last_page = h(0x28), h(0x2C)
    n_pages = h(0x14)
    obj_tab, n_objs = le + h(0x40), h(0x44)
    page_map = le + h(0x48)
    fix_pages, fix_recs = le + h(0x68), le + h(0x6C)
    data_pages = h(0x80)  # absolute file offset
    eip_obj, eip = h(0x18), h(0x1C)

    objs = []
    for i in range(n_objs):
        vsize, base, flags, pidx, pcount, _ = struct.unpack_from('<6I', data, obj_tab + 24 * i)
        objs.append(dict(vsize=vsize, base=base, flags=flags, pidx=pidx, pcount=pcount,
                         mem=bytearray(max(vsize, pcount * page_size))))

    # LE object page table: 24-bit big-endian page number, then a flags byte.
    def page_file_offset(logical):  # logical = 1-based object page index
        e = data[page_map + 4 * (logical - 1): page_map + 4 * logical]
        num = (e[0] << 16) | (e[1] << 8) | e[2]
        return data_pages + (num - 1) * page_size, num

    for o in objs:
        for j in range(o['pcount']):
            off, num = page_file_offset(o['pidx'] + j)
            size = last_page if num == n_pages else page_size
            o['mem'][j * page_size: j * page_size + size] = data[off: off + size]

    applied = skipped = 0
    for o in objs:
        for j in range(o['pcount']):
            logical = o['pidx'] + j
            start = fix_recs + u32(data, fix_pages + 4 * (logical - 1))
            end = fix_recs + u32(data, fix_pages + 4 * logical)
            p = start
            while p < end:
                src, flg = u8(data, p), u8(data, p + 1)
                p += 2
                stype = src & 0x0F
                if src & 0x20:
                    count = u8(data, p); p += 1
                    srcoffs = None
                else:
                    srcoffs = [s16(data, p)]; p += 2
                if flg & 0x03 != 0:
                    raise SystemExit(f'import/entry fixup type {flg & 3} at page {logical} not supported')
                if flg & 0x40:
                    tobj = u16(data, p); p += 2
                else:
                    tobj = u8(data, p); p += 1
                toff = 0
                if stype != 0x02:
                    if flg & 0x10:
                        toff = u32(data, p); p += 4
                    else:
                        toff = u16(data, p); p += 2
                if flg & 0x04:  # additive
                    toff += u32(data, p) if flg & 0x20 else u16(data, p)
                    p += 4 if flg & 0x20 else 2
                if srcoffs is None:
                    srcoffs = [s16(data, p + 2 * k) for k in range(count)]
                    p += 2 * count
                target = objs[tobj - 1]['base'] + toff
                for so in srcoffs:
                    at = j * page_size + so
                    if at < 0 or at + 4 > len(o['mem']):
                        skipped += 1
                        continue
                    if stype == 0x07 or stype == 0x06:      # 32-bit offset (16:32 keeps its selector out)
                        struct.pack_into('<I', o['mem'], at, target & 0xFFFFFFFF)
                    elif stype == 0x08:                    # 32-bit self-relative
                        struct.pack_into('<I', o['mem'], at, (target - (o['base'] + at + 4)) & 0xFFFFFFFF)
                    elif stype == 0x05:                    # 16-bit offset
                        struct.pack_into('<H', o['mem'], at, target & 0xFFFF)
                    elif stype in (0x02, 0x03):            # selector, 16:16 far pointer
                        skipped += 1
                        continue
                    else:
                        raise SystemExit(f'fixup source type {stype:#x} not supported')
                    applied += 1
    for o in objs:
        del o['mem'][o['vsize']:]
    entry = objs[eip_obj - 1]['base'] + eip
    return objs, entry, applied, skipped


def write_elf(path, objs, entry):
    shstr = b'\0' + b''.join(f'.obj{i + 1}\0'.encode() for i in range(len(objs))) + b'.shstrtab\0'
    ehsize, phsize, shsize = 52, 32, 40
    n = len(objs)
    off = ehsize + phsize * n
    offs = []
    for o in objs:
        off = (off + 0xFFF) & ~0xFFF
        offs.append(off)
        off += len(o['mem'])
    shstr_off = off
    sh_off = (shstr_off + len(shstr) + 3) & ~3
    out = bytearray(sh_off + shsize * (n + 2))
    out[0:16] = b'\x7fELF\x01\x01\x01' + b'\0' * 9
    struct.pack_into('<HHIIIIIHHHHHH', out, 16, 2, 3, 1, entry, ehsize, sh_off, 0,
                     ehsize, phsize, n, shsize, n + 2, n + 1)
    name_off = 1
    for i, o in enumerate(objs):
        exe, wr = o['flags'] & 0x4, o['flags'] & 0x2
        pflags = 4 | (1 if exe else 0) | (2 if wr else 0)
        struct.pack_into('<8I', out, ehsize + phsize * i, 1, offs[i], o['base'], o['base'],
                         len(o['mem']), o['vsize'], pflags, 0x1000)
        out[offs[i]: offs[i] + len(o['mem'])] = o['mem']
        shflags = 2 | (4 if exe else 0) | (1 if wr else 0)
        struct.pack_into('<10I', out, sh_off + shsize * (i + 1), name_off, 1, shflags, o['base'],
                         offs[i], len(o['mem']), 0, 0, 16, 0)
        name_off += len(f'.obj{i + 1}') + 1
    out[shstr_off: shstr_off + len(shstr)] = shstr
    struct.pack_into('<10I', out, sh_off + shsize * (n + 1), name_off, 3, 0, 0, shstr_off,
                     len(shstr), 0, 0, 1, 0)
    open(path, 'wb').write(out)


if __name__ == '__main__':
    objs, entry, applied, skipped = load_le(open(sys.argv[1], 'rb').read())
    write_elf(sys.argv[2], objs, entry)
    for i, o in enumerate(objs):
        print(f'obj{i + 1}: base {o["base"]:#010x} size {o["vsize"]:#x} flags {o["flags"]:#x}')
    print(f'entry {entry:#010x}, fixups applied {applied}, skipped {skipped}')
