"""Lists the draws, copies and display registers in a BT2_GIF_DUMP file (gs/gpu/gpu_gs_backend.cpp).

Consecutive primitives with the same GS state form one draw; each line gives its primitive, vertex
count, screen rectangle (after XYOFFSET, in pixels), texture coordinates, where it ends ("until
<transfer>:<quadword>": the GIF tag that ended it, counted from the dump's start; BT2_GS_PEEK stops there)
and the state that matters for picture passes (frame, Z buffer, texture, scissor, alpha, test, clamp).

    python tools/graphics/gpu/gif_dump_draws.py <dump> [--flip N] [--touching-y Y]
"""
import argparse
import struct
import sys

REG = {0x00: 'PRIM', 0x01: 'RGBAQ', 0x02: 'ST', 0x03: 'UV', 0x04: 'XYZF2', 0x05: 'XYZ2', 0x06: 'TEX0_1',
       0x07: 'TEX0_2', 0x08: 'CLAMP_1', 0x09: 'CLAMP_2', 0x0A: 'FOG', 0x0C: 'XYZF3', 0x0D: 'XYZ3',
       0x14: 'TEX1_1', 0x15: 'TEX1_2', 0x16: 'TEX2_1', 0x17: 'TEX2_2', 0x18: 'XYOFFSET_1', 0x19: 'XYOFFSET_2',
       0x1A: 'PRMODECONT', 0x1B: 'PRMODE', 0x1C: 'TEXCLUT', 0x22: 'SCANMSK', 0x34: 'MIPTBP1_1',
       0x35: 'MIPTBP1_2', 0x36: 'MIPTBP2_1', 0x37: 'MIPTBP2_2', 0x3B: 'TEXA', 0x3D: 'FOGCOL', 0x3F: 'TEXFLUSH',
       0x40: 'SCISSOR_1', 0x41: 'SCISSOR_2', 0x42: 'ALPHA_1', 0x43: 'ALPHA_2', 0x44: 'DIMX', 0x45: 'DTHE',
       0x46: 'COLCLAMP', 0x47: 'TEST_1', 0x48: 'TEST_2', 0x49: 'PABE', 0x4A: 'FBA_1', 0x4B: 'FBA_2',
       0x4C: 'FRAME_1', 0x4D: 'FRAME_2', 0x4E: 'ZBUF_1', 0x4F: 'ZBUF_2', 0x50: 'BITBLTBUF', 0x51: 'TRXPOS',
       0x52: 'TRXREG', 0x53: 'TRXDIR', 0x54: 'HWREG', 0x60: 'SIGNAL', 0x61: 'FINISH', 0x62: 'LABEL'}
PRIV = ['PMODE', 'SMODE1', 'SMODE2', 'SRFSH', 'SYNCH1', 'SYNCH2', 'SYNCV', 'DISPFB1', 'DISPLAY1', 'DISPFB2',
        'DISPLAY2', 'EXTBUF', 'EXTDATA', 'EXTWRITE', 'BGCOLOR']
PRIM_NAMES = ['point', 'line', 'linestrip', 'tri', 'tristrip', 'trifan', 'sprite', 'prim7']
PSM = {0x00: 'CT32', 0x01: 'CT24', 0x02: 'CT16', 0x0A: 'CT16S', 0x13: 'T8', 0x14: 'T4', 0x1B: 'T8H',
       0x24: 'T4HL', 0x2C: 'T4HH', 0x30: 'Z32', 0x31: 'Z24', 0x32: 'Z16', 0x3A: 'Z16S'}
# The registers a draw's state is made of (per context: index 0 or 1 added).
CONTEXT_REGS = ['TEX0', 'CLAMP', 'TEX1', 'XYOFFSET', 'SCISSOR', 'ALPHA', 'TEST', 'FBA', 'FRAME', 'ZBUF']
SHARED_REGS = ['TEXA', 'TEXCLUT', 'DTHE', 'COLCLAMP', 'PABE', 'FOGCOL', 'SCANMSK', 'DIMX']


def bits(value, lo, count):
    return (value >> lo) & ((1 << count) - 1)


def psm(value):
    return PSM.get(value, hex(value))


def describe_state(regs, prim, ctx):
    def r(name):
        return regs.get(f'{name}_{ctx + 1}', 0)
    frame, zbuf, tex0, sc = r('FRAME'), r('ZBUF'), r('TEX0'), r('SCISSOR')
    out = [f"FRAME {bits(frame, 0, 9) * 32:#x} w{bits(frame, 16, 6)} {psm(bits(frame, 24, 6))}"
           + (f" mask {bits(frame, 32, 32):#010x}" if bits(frame, 32, 32) else '')]
    test = r('TEST')
    if bits(test, 16, 1):
        out.append(f"Z {bits(zbuf, 0, 9) * 32:#x} {psm(bits(zbuf, 24, 4) | 0x30)} test{bits(test, 17, 2)}"
                   + (' nowrite' if bits(zbuf, 32, 1) else ''))
    if bits(prim, 4, 1):
        tw, th = 1 << bits(tex0, 26, 4), 1 << bits(tex0, 30, 4)
        out.append(f"TEX {bits(tex0, 0, 14):#x} w{bits(tex0, 14, 6)} {psm(bits(tex0, 20, 6))} {tw}x{th}"
                   f" tfx{bits(tex0, 35, 2)} tcc{bits(tex0, 34, 1)}"
                   + (f" clut {bits(tex0, 37, 14):#x}" if bits(tex0, 20, 6) in (0x13, 0x14, 0x1B, 0x24, 0x2C) else '')
                   + f" tex1 {r('TEX1'):#x} clamp {r('CLAMP'):#x}")
    out.append(f"SC {bits(sc, 0, 11)}..{bits(sc, 16, 11)} x {bits(sc, 32, 11)}..{bits(sc, 48, 11)}")
    if bits(prim, 6, 1):
        a = r('ALPHA')
        out.append(f"ALPHA {bits(a, 0, 2)}{bits(a, 2, 2)}{bits(a, 4, 2)}{bits(a, 6, 2)} fix {bits(a, 32, 8):#x}")
    if test & 0xFFFF:
        out.append(f"TEST {test & 0xFFFF:#x}")
    return ' | '.join(out)


class Draws:
    def __init__(self, touching_y=None):
        self.regs = {}
        self.vertices = []
        self.key = None
        self.uv = None
        self.st = (0.0, 0.0, 1.0)
        self.touching_y = touching_y
        self.lines = []
        self.place = (0, 0)  # The current GIF tag: transfer record (from the dump's start), quadword.

    def context(self):
        prim = self.regs.get('PRIM', 0)
        if not bits(self.regs.get('PRMODECONT', 1), 0, 1):
            prim = (prim & 7) | (self.regs.get('PRMODE', 0) & ~7)
        return prim, bits(prim, 9, 1)

    def state_key(self):
        prim, ctx = self.context()
        regs = tuple(self.regs.get(f'{n}_{ctx + 1}') for n in CONTEXT_REGS)
        return (prim, ctx, regs, tuple(self.regs.get(n) for n in SHARED_REGS))

    def end(self):
        if not self.vertices:
            return
        prim, ctx = self.key[0], self.key[1]
        off = self.regs_at_key
        ofx, ofy = bits(off, 0, 16) / 16, bits(off, 32, 16) / 16
        xs = [v[0] / 16 - ofx for v in self.vertices]
        ys = [v[1] / 16 - ofy for v in self.vertices]
        if self.touching_y is None or (min(ys) <= self.touching_y + 1 and max(ys) >= self.touching_y):
            t = [v[2] for v in self.vertices if v[2] is not None]
            tex = ''
            if t:
                tex = f" uv {min(a for a, _ in t):.2f},{min(b for _, b in t):.2f}..{max(a for a, _ in t):.2f},{max(b for _, b in t):.2f}"
            self.lines.append(f"  draw {PRIM_NAMES[prim & 7]} x{len(self.vertices)} ctx{ctx}"
                              f" rect {min(xs):.2f},{min(ys):.2f}..{max(xs):.2f},{max(ys):.2f}{tex}"
                              f" {'fst' if bits(prim, 8, 1) else 'stq'} until {self.place[0]}:{self.place[1]}"
                              f"\n      {self.state_text}")
        self.vertices = []
        self.key = None

    def vertex(self, x, y):
        key = self.state_key()
        if key != self.key:
            self.end()
            self.key = key
            prim, ctx = self.context()
            self.regs_at_key = self.regs.get(f'XYOFFSET_{ctx + 1}', 0)
            self.state_text = describe_state(self.regs, prim, ctx)
        prim, _ = self.context()
        texcoord = None
        if bits(prim, 4, 1):
            if bits(prim, 8, 1):
                uv = self.regs.get('UV', 0)
                texcoord = (bits(uv, 0, 14) / 16, bits(uv, 16, 14) / 16)
            else:
                s, t, q = self.st
                tex0 = self.regs.get(f'TEX0_{bits(prim, 9, 1) + 1}', 0)
                q = q or 1.0
                texcoord = (s / q * (1 << bits(tex0, 26, 4)), t / q * (1 << bits(tex0, 30, 4)))
        self.vertices.append((x, y, texcoord))

    def write(self, reg, value):
        name = REG.get(reg, f'reg{reg:#x}')
        if name in ('XYZ2', 'XYZF2', 'XYZ3', 'XYZF3'):
            self.vertex(bits(value, 0, 16), bits(value, 16, 16))
            return
        if name == 'ST':
            self.st = (struct.unpack('<f', struct.pack('<I', bits(value, 0, 32)))[0],
                       struct.unpack('<f', struct.pack('<I', bits(value, 32, 32)))[0], self.st[2])
        elif name == 'RGBAQ':
            self.st = (self.st[0], self.st[1], struct.unpack('<f', struct.pack('<I', bits(value, 32, 32)))[0])
        if name == 'TRXDIR':
            self.end()
            b, p, r = self.regs.get('BITBLTBUF', 0), self.regs.get('TRXPOS', 0), self.regs.get('TRXREG', 0)
            direction = ['host->local', 'local->host', 'local->local', 'off'][bits(value, 0, 2)]
            self.lines.append(f"  copy {direction} src {bits(b, 0, 14):#x} w{bits(b, 16, 6)} {psm(bits(b, 24, 6))}"
                              f" at {bits(p, 0, 11)},{bits(p, 16, 11)} -> dst {bits(b, 32, 14):#x} w{bits(b, 48, 6)}"
                              f" {psm(bits(b, 56, 6))} at {bits(p, 32, 11)},{bits(p, 48, 11)} size {bits(r, 0, 12)}x{bits(r, 32, 12)}")
        if name == 'PRIM':
            self.end()
        self.regs[name] = value

    def packed(self, reg, lo, hi):
        if reg == 0x0E:
            self.write(bits(hi, 0, 8), lo)
        elif reg == 0x00:
            self.write(0x00, bits(lo, 0, 11))
        elif reg == 0x01:
            # The colour only: packed RGBAQ keeps the Q the last packed ST set.
            self.regs['RGBAQ'] = bits(lo, 0, 8) | bits(lo, 32, 8) << 8 | bits(hi, 0, 8) << 16 | bits(hi, 32, 8) << 24
        elif reg == 0x02:
            self.write(0x02, lo)
            self.st = (self.st[0], self.st[1], struct.unpack('<f', struct.pack('<I', bits(hi, 0, 32)))[0])
        elif reg == 0x03:
            self.write(0x03, bits(lo, 0, 14) | bits(lo, 32, 14) << 16)
        elif reg in (0x04, 0x05, 0x0C, 0x0D):
            self.vertex(bits(lo, 0, 16), bits(lo, 32, 16))
        elif reg != 0x0F:
            self.write(reg, lo)

    def transfer(self, data, record):
        at = 0
        while at + 16 <= len(data):
            self.place = (record, at // 16)
            lo, hi = struct.unpack_from('<QQ', data, at)
            at += 16
            nloop, flg, nreg = bits(lo, 0, 15), bits(lo, 58, 2), bits(lo, 60, 4) or 16
            if bits(lo, 46, 1):
                self.write(0x00, bits(lo, 47, 11))
            if flg == 0:
                for _ in range(nloop):
                    for n in range(nreg):
                        a, b = struct.unpack_from('<QQ', data, at)
                        at += 16
                        self.packed(bits(hi, n * 4, 4), a, b)
            elif flg == 1:
                count = nloop * nreg
                for n in range(count):
                    self.write(bits(hi, (n % nreg) * 4, 4), struct.unpack_from('<Q', data, at + n * 8)[0])
                at += (count * 8 + 15) // 16 * 16
            else:
                at += nloop * 16


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('dump')
    parser.add_argument('--flip', type=int, help='only this flip')
    parser.add_argument('--touching-y', type=float, help='only draws whose rectangle reaches this screen line')
    args = parser.parse_args()
    data = open(args.dump, 'rb').read()
    draws = Draws(args.touching_y)
    privileged = {}
    at = 0
    record = 0
    while at < len(data):
        kind = data[at:at + 1]
        at += 1
        if kind == b'T':
            size, = struct.unpack_from('<I', data, at)
            draws.transfer(data[at + 4:at + 4 + size], record)
            record += 1
            at += 4 + size
        elif kind == b'P':
            index, value = struct.unpack_from('<IQ', data, at)
            privileged[PRIV[index]] = value
            at += 12
        elif kind == b'F':
            flip, = struct.unpack_from('<I', data, at)
            at += 4
            draws.end()
            if args.flip is None or args.flip == flip:
                print(f"flip {flip}")
                for name in ('PMODE', 'DISPFB1', 'DISPLAY1', 'DISPFB2', 'DISPLAY2'):
                    if name in privileged:
                        v = privileged[name]
                        if name.startswith('DISPFB'):
                            text = f"fbp {bits(v, 0, 9) * 32:#x} w{bits(v, 9, 6)} {psm(bits(v, 15, 5))} at {bits(v, 32, 11)},{bits(v, 43, 11)}"
                        elif name.startswith('DISPLAY'):
                            text = f"dx {bits(v, 0, 12)} dy {bits(v, 12, 11)} magh {bits(v, 23, 4)} magv {bits(v, 27, 2)} dw {bits(v, 32, 12)} dh {bits(v, 44, 11)}"
                        else:
                            text = hex(v)
                        print(f"  {name}: {text}")
                sys.stdout.write('\n'.join(draws.lines) + ('\n' if draws.lines else ''))
            draws.lines = []
        else:
            raise SystemExit(f"bad record at {at - 1}")


if __name__ == '__main__':
    main()
