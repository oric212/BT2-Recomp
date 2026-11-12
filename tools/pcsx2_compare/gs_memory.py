"""GS memory out of a PCSX2 save state, and pictures out of GS memory (docs/plans/validation/PCSX2_COMPARISON_PLAN.md,
step 7).

A save state (.p2s) is a zip; its GS.bin is PCSX2's GS freeze. In version 9 (PCSX2 2.9): the version (u32), the
environment registers (16 x 64 bits from PRIM), the two contexts' registers (12 x 64 bits each, XYOFFSET to ZBUF),
the vertex registers, the transfer state, a byte, then GS memory (4 MB) at VRAM_OFFSET, then the GIF paths and Q.
The offset was found on states of the fight and is checked against the version.

GS memory is kept swizzled. A PSMCT32 picture: pages of 64 x 32 pixels (2,048 words), FBW pages a row; in a page,
blocks of 8 x 8 pixels in BLOCK32 order; in a block, words in COLUMN32 order. The same tables are in every GS
description (the GS User's Manual, section 8); written out here from that layout.
"""
import struct
import zipfile

import numpy as np

# GS memory starts one byte after the transfer state's last word (found from textures: at 424 every texel read
# shifted by a byte, its alpha 0x80 in the next word).
VERSION, VRAM_OFFSET, VRAM_SIZE = 9, 425, 4 * 1024 * 1024
# The environment's registers from offset 4 (TRXREG twice, the second unused), each with its GS address.
ENVIRONMENT = (('PRIM', 0x00), ('PRMODECONT', 0x1A), ('TEXCLUT', 0x1C), ('SCANMSK', 0x22), ('TEXA', 0x3B),
               ('FOGCOL', 0x3D), ('DIMX', 0x44), ('DTHE', 0x45), ('COLCLAMP', 0x46), ('PABE', 0x49),
               ('BITBLTBUF', 0x50), ('TRXDIR', 0x53), ('TRXPOS', 0x51), ('TRXREG', 0x52), ('TRXREG (unused)', None))
CONTEXTS = 124                                   # The first context's XYOFFSET; 12 registers of 8 bytes each.
CONTEXT_REGISTERS = ('XYOFFSET', 'TEX0', 'TEX1', 'CLAMP', 'MIPTBP1', 'MIPTBP2', 'SCISSOR', 'ALPHA', 'TEST', 'FBA',
                     'FRAME', 'ZBUF')
CONTEXT_ADDRESSES = {'XYOFFSET': 0x18, 'TEX0': 0x06, 'TEX1': 0x14, 'CLAMP': 0x08, 'MIPTBP1': 0x34, 'MIPTBP2': 0x36,
                     'SCISSOR': 0x40, 'ALPHA': 0x42, 'TEST': 0x47, 'FBA': 0x4A, 'FRAME': 0x4C, 'ZBUF': 0x4E}
VERTEX = 316                                     # RGBAQ, ST, UV after the contexts.
VERTEX_REGISTERS = (('RGBAQ', 0x01), ('ST', 0x02), ('UV', 0x03))
BAND = 2048                                      # Rows of the upload per transfer (one transfer).

BLOCK32 = np.array([[0, 1, 4, 5, 16, 17, 20, 21],
                    [2, 3, 6, 7, 18, 19, 22, 23],
                    [8, 9, 12, 13, 24, 25, 28, 29],
                    [10, 11, 14, 15, 26, 27, 30, 31]])
COLUMN32 = np.array([[0, 1, 4, 5, 8, 9, 12, 13],
                     [2, 3, 6, 7, 10, 11, 14, 15],
                     [16, 17, 20, 21, 24, 25, 28, 29],
                     [18, 19, 22, 23, 26, 27, 30, 31],
                     [32, 33, 36, 37, 40, 41, 44, 45],
                     [34, 35, 38, 39, 42, 43, 46, 47],
                     [48, 49, 52, 53, 56, 57, 60, 61],
                     [50, 51, 54, 55, 58, 59, 62, 63]])


def read_state(path):
    """GS memory (bytes) and the two contexts' registers ({name: value} each) from a save state."""
    vram, contexts, _ = read_freeze(zipfile.ZipFile(path).read('GS.bin'))
    return vram, contexts


def read_freeze(data):
    """GS memory, the contexts' registers and [(GS address, value)] of the other registers, from GS.bin."""
    version, = struct.unpack_from('<I', data, 0)
    if version != VERSION:
        raise ValueError(f'GS freeze version {version}; this reader knows {VERSION}')
    q = lambda offset: struct.unpack_from('<Q', data, offset)[0]
    contexts = [{name: q(CONTEXTS + 96 * k + 8 * n) for n, name in enumerate(CONTEXT_REGISTERS)} for k in range(2)]
    others = [(address, q(4 + 8 * n)) for n, (_, address) in enumerate(ENVIRONMENT) if address is not None]
    others += [(address, q(VERTEX + 8 * n)) for n, (_, address) in enumerate(VERTEX_REGISTERS)]
    return data[VRAM_OFFSET:VRAM_OFFSET + VRAM_SIZE], contexts, others


def _gif_tag(nloop, flg, nreg=1, regs=0xE, eop=False):
    low = nloop | (int(eop) << 15) | (flg << 58) | (nreg << 60)
    return struct.pack('<QQ', low, regs)


def _a_d(writes):
    return _gif_tag(len(writes), 0) + b''.join(struct.pack('<QQ', value, address) for address, value in writes)


def setup_packets(gs_bin):
    """GIF data that gives a GS PCSX2's state from its freeze: the whole GS memory uploaded (a 512 x 2048 PSMCT32
    picture with DBW 8 covers its 512 pages once), then the registers written back (A+D). Not written: TRXDIR
    (it would start a transfer), XYZ (a vertex kick) and TEX0's CLUT load bits (the CLUT buffer is not in the
    freeze, so the CLUT loaded last is not restored)."""
    vram, contexts, others = read_freeze(gs_bin)
    picture = ct32_picture(vram, 0, 8, 512, 2048)
    out = b''
    for top in range(0, 2048, BAND):
        image = picture[top:top + BAND].tobytes()
        # BITBLTBUF (DBP 0, DBW 8, DPSM CT32: the destination half, bits 32 up), TRXPOS (DSAY), TRXREG, TRXDIR.
        out += _a_d([(0x50, 8 << 48), (0x51, top << 48), (0x52, 512 | (BAND << 32)), (0x53, 0)])
        qwords = len(image) // 16
        for start in range(0, qwords, 32767):
            count = min(32767, qwords - start)
            out += _gif_tag(count, 2, nreg=0, regs=0) + image[16 * start:16 * (start + count)]   # IMAGE
    writes = [(0x3F, 0)]                                                                      # TEXFLUSH
    for k, context in enumerate(contexts):
        for name, value in context.items():
            if name == 'TEX0':
                value &= (1 << 61) - 1                                                        # CLD 0: no CLUT load
            writes.append((CONTEXT_ADDRESSES[name] + k, value))
    writes += [(address, value) for address, value in others if address != 0x53]
    return out + _a_d(writes)


def ct32_words(base_page, width_pages, width, height):
    """The word index in GS memory of each pixel of a PSMCT32 picture (height x width)."""
    y, x = np.mgrid[0:height, 0:width]
    page = base_page + (y >> 5) * width_pages + (x >> 6)
    block = BLOCK32[(y >> 3) & 3, (x >> 3) & 7]
    return page * 2048 + block * 64 + COLUMN32[y & 7, x & 7]


def ct32_picture(vram, base_page, width_pages, width, height):
    """The picture's pixels as an array of RGBA bytes (height x width x 4)."""
    words = np.frombuffer(vram, dtype='<u4')
    index = ct32_words(base_page, width_pages, width, height) % len(words)
    return words[index].view(np.uint8).reshape(height, width, 4)


def frame_register(value):
    """FBP (pages), FBW (64-pixel units) and PSM of a FRAME register."""
    return value & 0x1FF, (value >> 16) & 0x3F, (value >> 24) & 0x3F
