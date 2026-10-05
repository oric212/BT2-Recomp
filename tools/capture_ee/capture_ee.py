"""Capture EE RAM at the observed BT2 kernel scan using an isolated PCSX2 process.

Only local ELF/isolated diagnostic process copies are instrumented. Captures,
BIOS copies and patched ELF output stay under ignored work/pcsx2-capture/.
"""
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/inspect_elf'))
from inspect_elf import inspect

HANDLER = 0x00338388
MARKER = 0x00387E80
EXPECTED_SHA = 'f7e5ba8e74657b8533c3a0fedff45de0d052be3a23fe4c475059ef888a81f03e'
TRAP = struct.pack('<6I', 0x3C180038, 0x37187E80, 0x3419B702,
                   0xAF190000, 0x08000000 | ((HANDLER + 16) >> 2), 0)


LOADFILE_POINTS={'loadfile-bind-after':(0x337264,0xB737),'loadfile-version-after':(0x3372A8,0xB738),'loadfile-module-before':(0x337BDC,0xB739),'loadfile-module-after':(0x337BE4,0xB73A),'mcman-module-after':(0x100434,0xB73B),'mcserv-module-after':(0x100454,0xB73C),'dbcman-module-after':(0x100474,0xB73D),'sio2d-module-after':(0x100494,0xB73E),'ds2ud-module-after':(0x1004B4,0xB73F),'libsd-module-after':(0x1004D4,0xB740),'sdrdrv-module-after':(0x1004F4,0xB741),'modhsyn-module-after':(0x100514,0xB742),'modsesq2-module-after':(0x100534,0xB743),'cdvdstm-module-after':(0x100554,0xB744),'sounds-module-after':(0x100574,0xB745),'cri-adxi-module-after':(0x100594,0xB746),'fileio-bind-after':(0x1005A4,0xB747),'heap-alloc-after':(0x3205D4,0xB748),'heap-copy-after':(0x320620,0xB749),'module-buffer-after':(0x320630,0xB74A),'heap-free-after':(0x320650,0xB74B),'dmac-startup-after':(0x1005B4,0xB74C)}
FRESH_BIND_STUB=0x01000000

def fresh_bind_stub(success=False,scmd=False):
    # Earlier invocation resumes displaced BGEZL with its conditional delay slot.
    words=[0x3C1A003A,0x375A95A0,0x8F5A0000,0x3C1B0001,0x377BE640,0,0]
    if success:
        words += [0x3C1A0036 if scmd else 0x3C1A003A,0x375A9F64 if scmd else 0x375A87CC,0x8F5A0000,0,0]
    words += [0x3C180038,0x37187E80,0xFF020008,0xAF1D0010,0x34190000|(0xB736 if scmd else (0xB734 if success else 0xB733)),0xAF190000]
    loop=len(words); words += [0x08000000|((FRESH_BIND_STUB+loop*4)>>2),0]
    early=len(words)
    words += [0x04430003 if scmd else 0x04A30003,0x8E020024 if scmd else 0x8E220024,0x08000000|((0x31F794 if scmd else 0x31F990)>>2),0,0x08000000|((0x31F7DC if scmd else 0x31F9D4)>>2),0]
    words[5]=0x175B0000|(early-6)
    if success:words[10]=0x13400000|(early-11)
    return struct.pack('<'+str(len(words))+'I',*words)

def capture_trap(point):
    if point in LOADFILE_POINTS:
        address,marker=LOADFILE_POINTS[point]
        words=[0x3C180038,0x37187E80,0xFF020008,0xAF1D0010,0x34190000|marker,0xAF190000,0x08000000|((address+24)>>2),0]
        return address,struct.pack('<8I',*words),marker
    if point=='post-scmd-bind-success':return 0x31F78C,struct.pack('<2I',0x08000000|(FRESH_BIND_STUB>>2),0),0xB736
    if point in ('post-cd-bind-after','post-cd-bind-success','post-scmd-bind-success'):
        return 0x31F984,struct.pack('<2I',0x08000000|(FRESH_BIND_STUB>>2),0x0040282D),(0xB734 if point.endswith('success') else 0xB733)
    if point in ('post-sif-before','post-sif-after','post-mmode-after','post-cd-after'):
        address,marker={'post-sif-before':(0x1003E0,0xB730),'post-sif-after':(0x1003E8,0xB731),'post-cd-after':(0x1003F0,0xB732),'post-mmode-after':(0x1003F8,0xB735)}[point]
        # Unique caller after the GS helper; following startup is intentionally
        # replaced by the halt. Earlier shared SIF initialization remains intact.
        words=[0x3C180038,0x37187E80,0xFF020008,0xAF1D0010]
        for register,offset in ((0x1000E010,20),(0x1000C000,24),(0x1000C010,28),
                                (0x1000F230,32),(0x1000F200,36),(0x1000F210,40)):
            words += [0x3C190000|(register>>16),0x37390000|(register&65535),0x8F220000,0xAF020000|offset]
        words += [0x34190000|marker,0xAF190000,0x08000000|((address+120)>>2),0]
        return address,struct.pack('<32I',*words),marker

    if point in ('gif-sync-before','gif-sync-after','gif-sync-poll'):
        address,marker={'gif-sync-before':(0x310564,0xB72B),'gif-sync-after':(0x3105DC,0xB72C),'gif-sync-poll':(0x3105D0,0xB72D)}[point]
        words=[0x3C180038,0x37187E80,0xFF030008,0xAF1D0010]
        if point=='gif-sync-poll':words[2]=0xFF020008
        for register,offset in ((0x10009000,20),(0x1000A000,24),(0x1000A020,28),
                                (0x1000A010,32),(0x10003C00,36),(0x10003020,40)):
            words += [0x3C190000|(register>>16),0x37390000|(register&65535),0x8F220000,0xAF020000|offset]
        words += [0x34190000|marker,0xAF190000,0x08000000|((address+120)>>2),0]
        return address,struct.pack('<32I',*words),marker

    if point in ('gif-vu-idle','gif-path-after'):
        address,marker={'gif-vu-idle':(0x310630,0xB72E),'gif-path-after':(0x1002B8,0xB72F)}[point]
        words=[0x3C180038,0x37187E80,0xFF060008 if point=='gif-vu-idle' else 0xFF020008,
               0xAF1D0010,0x34190000|marker,0xAF190000,0x08000000|((address+24)>>2),0]
        return address,struct.pack('<8I',*words),marker
    if point=='gif-idle':
        address,marker=0x3100E8,0xB728
        words=[0x3C180038,0x37187E80,0xFF020008,0xAF1D0010,
               0x3C191000,0x3739A000,0x8F220000,0xAF020014,
               0x8F220020,0xAF020018,0x8F220010,0xAF02001C,
               0x3C191000,0x3739E000,0x8F220000,0xAF020020,
               0x8F220010,0xAF020024,0x3C191000,0x3739F520,
               0x8F220000,0xAF020028,0x34190000|marker,0xAF190000,
               0x08000000|((address+96)>>2),0]
        return address,struct.pack('<26I',*words),marker
    if point in ('gif-qwc','gif-after'):
        address,marker={'gif-qwc':(0x31014C,0xB729),'gif-after':(0x1002AC,0xB72A)}[point]
        words=[0x3C180038,0x37187E80,0xFF020008,0xAF1D0010,
               0x34190000|marker,0xAF190000,0x08000000|((address+24)>>2),0]
        return address,struct.pack('<8I',*words),marker
    if point in ('gs-intc-before','gs-intc-cleared','gs-intc-event','gs-intc-acknowledged'):
        # Halt before later paths execute; the event trap preserves the preceding polling loop.
        address,marker={'gs-intc-before':(0x32F10C,0xB724),
                        'gs-intc-cleared':(0x32F114,0xB725),
                        'gs-intc-event':(0x32F14C,0xB726),
                        'gs-intc-acknowledged':(0x32F168,0xB727)}[point]
        # Publish INTC_STAT, INTC_MASK, SP and the preceding v0 independently.
        words=[0x3C180038,0x37187E80,0xAF020014,0x3C191000,
               0x3739F000,0x8F220000,0xAF020008,0x8F220010,0xAF02000C,
               0xAF1D0010,0x34190000|marker,0xAF190000,
               0x08000000|((address+48)>>2),0]
        return address,struct.pack('<14I',*words),marker
    if point=='gs-intc-after':
        address,marker=0x3104DC,0xB722
        words=[0x3C191000,0x3739F000,0x8F390000,0x3C180038,0x37187E80,0xAF190008,0xAF00000C,0xAF1D0010,0x3419B722,0xAF190000,0x08000000|((address+40)>>2),0]
        return address,struct.pack('<12I',*words),marker
    if point in ('gs-csr-reset','gs-imr-after','gs-crt-after','gs-csr-field'):
        address,marker={'gs-csr-reset':(0x30FE54,0xB71F),'gs-imr-after':(0x30FE6C,0xB720),'gs-crt-after':(0x100274,0xB721),'gs-csr-field':(0x3104F8,0xB723)}[point]
        words=[0x3C180038,0x37187E80,0xFF020008,0xAF1D0010,
               0x34190000|marker,0xAF190000,0x08000000|((address+24)>>2),0]
        return address,struct.pack('<8I',*words),marker
    if point == 'reset-ready':
        # Ready branch only; eight words preserve the earlier not-ready return at 0x33814C.
        # v1 is the 0x40000 mask used by the preceding conditional, not full SMFLAG.
        address,marker=0x338128,0xB71E
        words=[0x3C180038,0x37187E80,0xAF030004,0xAF1D0008,
               0x34190000|marker,0xAF190000,0x08000000|((address+24)>>2),0]
        return address,struct.pack('<8I',*words),marker
    if point in ('reset-smflag-before','reset-smflag-after','reset-flags-after','reset-rpc-clear','reset-subaddr-clear','reset-cmd-clear'):
        address, marker = {'reset-smflag-before':(0x33806C,0xB717),'reset-smflag-after':(0x338078,0xB718),
                           'reset-flags-after':(0x3380A0,0xB71A),'reset-rpc-clear':(0x3380B0,0xB71B),
                           'reset-subaddr-clear':(0x3380BC,0xB71C),'reset-cmd-clear':(0x338094,0xB71D)}[point]
        words = [0x3C180038,0x37187E80,0xAF020004,0xAF1D0008,
                 0x3C19B000,0x3739F230,0x8F390000,0xAF19000C,
                 0x34190000|marker,0xAF190000,0x08000000|((address+40)>>2),0]
        return address,struct.pack('<12I',*words),marker
    if point == 'kernel-handler':
        return HANDLER, TRAP, 0xB702
    if point in ('sif-dma-before', 'sif-dma-after', 'sif-bind-after', 'rpc-call-sema', 'cd-init-after', 'cd-semaphores', 'scmd-bind-after', 'scmd-mmode-after', 'sif-stop-after', 'sif-reset-subaddr', 'reset-submit-after'):
        # SifSetDma boundary or caller resumed after first/change-address or bind submission.
        address, marker = {'sif-dma-before': (0x331D44, 0xB70C),
                           'sif-dma-after': (0x331D4C, 0xB70D),
                           'sif-bind-after': (0x33284C, 0xB70F),
                           'rpc-call-sema': (0x332AD0, 0xB710),
                           'cd-init-after': (0x332B0C, 0xB711),
                           'cd-semaphores': (0x31EE6C, 0xB712),
                           'scmd-bind-after': (0x31F78C, 0xB713),
                           'scmd-mmode-after': (0x32050C, 0xB714),
                           'sif-stop-after': (0x337FC0, 0xB715),
                           'sif-reset-subaddr': (0x337FC8, 0xB716),
                           'reset-submit-after': (0x338084, 0xB719)}[point]
        words = [0x3C180038, 0x37187E80, 0xAF020004, 0xAF1D0008,
                 0x34190000 | marker, 0xAF190000,
                 0x08000000 | ((address + 24) >> 2), 0]
        return address, struct.pack('<8I', *words), marker
    if point in ('sif-command', 'sif-rpc-state'):
        # Caller resumed after the first SifGetReg; publish return and selector.
        address, marker = (0x00331A34, 0xB70B) if point == 'sif-command' else (0x00332110, 0xB70E)
        words = [0x3C180038, 0x37187E80, 0xAF020004, 0xAF040008,
                 0x34190000 | marker, 0xAF190000,
                 0x08000000 | ((address + 24) >> 2), 0]
        return address, struct.pack('<8I', *words), marker
    if point == 'osd-readback':
        # First setter and second getter returned; capture original and readback.
        address, marker = 0x00338620, 0xB708
        words = [0x3C180038, 0x37187E80, 0x8FB90004, 0xAF190004,
                 0x8FB90000, 0xAF190008, 0x34190000 | marker, 0xAF190000,
                 0x08000000 | ((address + 32) >> 2), 0]
        return address, struct.pack('<10I', *words), marker
    if point == 'osd-config':
        # First GetOsdConfigParam has returned; publish its stack output word.
        address, marker = 0x003385F4, 0xB707
        words = [0x8FB90000, 0x3C180038, 0x37187E80, 0xAF190004,
                 0x34190000 | marker, 0xAF190000,
                 0x08000000 | ((address + 24) >> 2), 0]
        return address, struct.pack('<8I', *words), marker
    points = {'timer3-mode': (0x00338A1C, 0x1810, 0xB703),
              'timer2-mode': (0x00338C58, 0x1010, 0xB704),
              'intc-mask': (0x0032E9A4, 0xF010, 0xB705),
              'timer2-readback': (0x00338DD8, 0x1010, 0xB706),
              'dma-status': (0x003319CC, 0xE010, 0xB709),
              'dma5-control': (0x003319EC, 0xC000, 0xB70A)}
    if point not in points:
        raise ValueError('Unknown capture point.')
    address, register, marker = points[point]
    # Read the observed MMIO word once, publish it beside the marker, then spin.
    words = [0x3C181000, 0x37180000 | register, 0x8F190000, 0x3C180038, 0x37187E80,
             0xAF190004, 0x34190000 | marker, 0xAF190000,
             0x08000000 | ((address + 32) >> 2), 0]
    return address, struct.pack('<10I', *words), marker


def instrument(data, point='kernel-handler'):
    address, trap, marker = capture_trap(point)
    metadata = inspect(data)
    if metadata['sha256'] != EXPECTED_SHA:
        raise ValueError('Instrumentation requires the validated local BT2 executable hash.')
    matches = [seg for seg in metadata['load_segments']
               if seg['flags'] & 1 and seg['virtual_address'] <= address
               and address + len(trap) <= seg['virtual_address'] + seg['file_size']]
    if len(matches) != 1:
        raise ValueError('Handler must lie in exactly one executable file-backed segment.')
    seg = matches[0]
    offset = seg['file_offset'] + address - seg['virtual_address']
    patched = bytearray(data)
    patched[offset:offset + len(trap)] = trap
    if point in ('post-cd-bind-after','post-cd-bind-success','post-scmd-bind-success'):
        expected=struct.pack('<2I',0x04430013,0x8E020024) if point=='post-scmd-bind-success' else struct.pack('<3I',0x0040282D,0x04A30012,0x8E220024)
        if data[offset:offset+len(expected)]!=expected:raise ValueError('Displaced bind branch differs')
        phoff=struct.unpack_from('<I',data,28)[0]; phsize,count=struct.unpack_from('<HH',data,42)
        empty=[phoff+n*phsize for n in range(count) if struct.unpack_from('<8I',data,phoff+n*phsize)[0]==1 and struct.unpack_from('<8I',data,phoff+n*phsize)[4:6]==(0,0)]
        if len(empty)!=1:raise ValueError('Expected one unused ELF load record')
        if any(x['virtual_address']<=FRESH_BIND_STUB<x['virtual_address']+x['memory_size'] for x in metadata['load_segments']):raise ValueError('Authored stub overlaps retail load')
        file_offset=(len(patched)+4095)&~4095; stub=fresh_bind_stub(point.endswith('success'),point=='post-scmd-bind-success')
        patched.extend(bytes(file_offset-len(patched))+stub)
        struct.pack_into('<8I',patched,empty[0],1,file_offset,FRESH_BIND_STUB,FRESH_BIND_STUB,len(stub),len(stub),5,4096)
    return bytes(patched), offset


def export_rva(data, symbol):
    """Read a named, non-forwarded PE32+ export without loading executable code."""
    def unpack(fmt, offset):
        if offset < 0 or offset + struct.calcsize(fmt) > len(data):
            raise ValueError('Truncated PE metadata.')
        return struct.unpack_from(fmt, data, offset)
    if data[:2] != b'MZ':
        raise ValueError('Expected PE executable.')
    pe, = unpack('<I', 0x3C)
    if data[pe:pe + 4] != b'PE\0\0':
        raise ValueError('Invalid PE signature.')
    machine, count = unpack('<HH', pe + 4)
    optsize, = unpack('<H', pe + 20)
    opt = pe + 24
    magic, = unpack('<H', opt)
    if machine != 0x8664 or magic != 0x20B or optsize < 120:
        raise ValueError('Expected Windows x64 PE32+ executable.')
    sections = [unpack('<IIII', opt + optsize + i * 40 + 8) for i in range(count)]
    def offset(rva, size=1):
        for virtual_size, va, raw_size, raw in sections:
            if va <= rva and rva + size <= va + raw_size:
                result = raw + rva - va
                if result + size <= len(data):
                    return result
        raise ValueError('PE RVA has no file-backed range.')
    er, es = unpack('<II', opt + 112)
    _, functions, names, fr, nr, ordinal_r = unpack('<6I', offset(er, 40) + 16)
    for i in range(names):
        name_r, = unpack('<I', offset(nr + i * 4, 4))
        start = offset(name_r)
        end = data.find(b'\0', start, start + 256)
        if end < 0:
            raise ValueError('Invalid export name.')
        if data[start:end] != symbol.encode('ascii'):
            continue
        ordinal, = unpack('<H', offset(ordinal_r + i * 2, 2))
        if ordinal >= functions:
            raise ValueError('Invalid export ordinal.')
        result, = unpack('<I', offset(fr + ordinal * 4, 4))
        if er <= result < er + es:
            raise ValueError('Forwarded export unsupported.')
        return result
    raise ValueError(f'PCSX2 does not export {symbol}.')


class ProcessReader:
    def __init__(self, pid, executable):
        self.kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        self.psapi = ctypes.WinDLL('psapi', use_last_error=True)
        self.kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        self.kernel.OpenProcess.restype = wintypes.HANDLE
        self.kernel.ReadProcessMemory.argtypes = [wintypes.HANDLE, ctypes.c_void_p,
                                                  ctypes.c_void_p, ctypes.c_size_t,
                                                  ctypes.POINTER(ctypes.c_size_t)]
        self.kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        self.psapi.EnumProcessModulesEx.argtypes = [wintypes.HANDLE, ctypes.c_void_p,
                                                    wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD]
        self.psapi.GetModuleFileNameExW.argtypes = [wintypes.HANDLE, wintypes.HMODULE,
                                                   wintypes.LPWSTR, wintypes.DWORD]
        self.pid=pid
        self.handle = self.kernel.OpenProcess(0x400 | 0x10, False, pid)
        if not self.handle:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            modules = (wintypes.HMODULE * 1024)()
            needed = wintypes.DWORD()
            if not self.psapi.EnumProcessModulesEx(self.handle, modules, ctypes.sizeof(modules),
                                                   ctypes.byref(needed), 3):
                raise ctypes.WinError(ctypes.get_last_error())
            path = ctypes.create_unicode_buffer(32768)
            if not self.psapi.GetModuleFileNameExW(self.handle, modules[0], path, len(path)):
                raise ctypes.WinError(ctypes.get_last_error())
            if Path(path.value).resolve() != executable.resolve():
                raise ValueError('Launched process module differs from requested PCSX2.')
            if not modules[0]:
                raise OSError("Main module is still initializing.")
            self.base = modules[0]
        except Exception:
            self.close()
            raise

    def read(self, address, size):
        buffer = ctypes.create_string_buffer(size)
        got = ctypes.c_size_t()
        if not self.kernel.ReadProcessMemory(self.handle, address, buffer, size, ctypes.byref(got)):
            raise ctypes.WinError(ctypes.get_last_error())
        if got.value != size:
            raise ValueError('Incomplete process read.')
        return buffer.raw

    def close(self):
        if self.handle:
            self.kernel.CloseHandle(self.handle)
            self.handle = None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pcsx2', type=Path, required=True)
    parser.add_argument('--bios', type=Path, required=True)
    parser.add_argument('--capture-point', choices=['kernel-handler', 'timer3-mode', 'timer2-mode', 'intc-mask', 'timer2-readback', 'osd-config', 'osd-readback', 'dma-status', 'dma5-control', 'sif-command', 'sif-dma-before', 'sif-dma-after', 'sif-rpc-state', 'sif-bind-after', 'rpc-call-sema', 'cd-init-after', 'cd-semaphores', 'scmd-bind-after', 'scmd-mmode-after', 'sif-stop-after', 'sif-reset-subaddr', 'reset-smflag-before', 'reset-smflag-after', 'reset-submit-after', 'reset-flags-after', 'reset-rpc-clear', 'reset-subaddr-clear', 'reset-cmd-clear', 'reset-ready', 'gs-csr-reset', 'gs-imr-after', 'gs-crt-after', 'gs-intc-after', 'gs-csr-field', 'gs-intc-before', 'gs-intc-cleared', 'gs-intc-event', 'gs-intc-acknowledged','gif-idle','gif-qwc','gif-after','gif-sync-before','gif-sync-after','gif-sync-poll','gif-vu-idle','gif-path-after','post-sif-before','post-sif-after','post-scmd-bind-success','post-mmode-after','post-cd-after','post-cd-bind-after','post-cd-bind-success','loadfile-bind-after','loadfile-version-after','loadfile-module-before','loadfile-module-after','mcman-module-after','mcserv-module-after','dbcman-module-after','sio2d-module-after','ds2ud-module-after','libsd-module-after','sdrdrv-module-after','modhsyn-module-after','modsesq2-module-after','cdvdstm-module-after','sounds-module-after','cri-adxi-module-after','fileio-bind-after','heap-alloc-after','heap-copy-after','module-buffer-after','heap-free-after','dmac-startup-after'], default='kernel-handler')
    parser.add_argument('--disc', type=Path, help='Explicit user-local ISO mounted alongside the instrumented ELF.')
    parser.add_argument('--pad-poll-read-count',type=int,choices=[5,10],default=5)
    parser.add_argument('--capture-first-pad-poll',action='store_true',help='Instrument the first IOP FIFO receive after SIO2D return; isolated interpreter profile only.')
    parser.add_argument('--capture-spu2-key-readback',choices=['on','off'],help='Terminal first LIBSD key-on readback after SOUNDS return.')
    parser.add_argument('--capture-spu2-initialized',action='store_true',help='Terminal LIBSD sceSdInit return and register snapshot after SOUNDS return.')
    parser.add_argument('--capture-cri-low-read',action='store_true',help='Terminal reached CRI low-memory argument read after SOUNDS return.')
    parser.add_argument('--capture-spu2-transfer',action='store_true',help='Terminal first LIBSD transfer poll and sound RAM readback after SOUNDS return.')
    parser.add_argument('--capture-spu2-interrupts',action='store_true',help='Record three linked LIBSD interrupt disables after SOUNDS return.')
    parser.add_argument('--capture-spu2-status',action='store_true',help='Terminal first LIBSD status read capture after SOUNDS return.')
    parser.add_argument('--timeout', type=float, default=40)
    args = parser.parse_args()
    if sys.platform != 'win32' or struct.calcsize('P') != 8:
        parser.error('Requires 64-bit Windows Python.')
    executable = args.pcsx2.resolve()
    exe_data = executable.read_bytes()
    rva = export_rva(exe_data, 'EEmem')
    capture_iop = args.capture_point in ('sif-dma-before', 'sif-dma-after', 'sif-bind-after', 'cd-init-after', 'scmd-bind-after', 'scmd-mmode-after')
    capture_boot_iop = args.capture_point in ('reset-ready','post-sif-before','post-sif-after','post-scmd-bind-success','post-mmode-after','post-cd-after','post-cd-bind-after','post-cd-bind-success','loadfile-bind-after','loadfile-version-after','loadfile-module-before','loadfile-module-after','mcman-module-after','mcserv-module-after','dbcman-module-after','sio2d-module-after','ds2ud-module-after','libsd-module-after','sdrdrv-module-after','modhsyn-module-after','modsesq2-module-after','cdvdstm-module-after','sounds-module-after','cri-adxi-module-after','fileio-bind-after','heap-alloc-after','heap-copy-after','module-buffer-after','heap-free-after','dmac-startup-after')
    if (capture_boot_iop or args.capture_point in ('gs-csr-reset','gs-imr-after','gs-crt-after','gs-intc-after','gs-csr-field','gs-intc-before','gs-intc-cleared','gs-intc-event','gs-intc-acknowledged','gif-idle','gif-qwc','gif-after','gif-sync-before','gif-sync-after','gif-sync-poll','gif-vu-idle','gif-path-after','post-sif-before','post-sif-after','post-scmd-bind-success','post-mmode-after','post-cd-after','post-cd-bind-after','post-cd-bind-success','loadfile-bind-after','loadfile-version-after','loadfile-module-before','loadfile-module-after','mcman-module-after','mcserv-module-after','dbcman-module-after','sio2d-module-after','ds2ud-module-after','libsd-module-after','sdrdrv-module-after','modhsyn-module-after','modsesq2-module-after','cdvdstm-module-after','sounds-module-after','cri-adxi-module-after','fileio-bind-after','heap-alloc-after','heap-copy-after','module-buffer-after','heap-free-after','dmac-startup-after')) and not args.disc:parser.error('This capture point requires an explicit local disc')
    iop_rva = export_rva(exe_data, 'IOPmem') if capture_iop or capture_boot_iop else None
    source = ROOT / 'rom/SLUS_214.41'
    original = source.read_bytes()
    address, trap, marker = capture_trap(args.capture_point)
    patched, offset = instrument(original, args.capture_point)
    folder = ROOT / 'work/pcsx2-capture'
    if args.capture_point != 'kernel-handler':
        folder = folder / (args.capture_point+"-pad-poll-"+str(args.pad_poll_read_count) if args.capture_first_pad_poll else args.capture_point)
    if args.capture_spu2_key_readback:folder=folder.with_name(folder.name+'-spu2-key-readback-'+args.capture_spu2_key_readback)
    if args.capture_spu2_initialized:folder=folder.with_name(folder.name+'-spu2-initialized')
    if args.capture_cri_low_read:folder=folder.with_name(folder.name+'-cri-low-read')
    if args.capture_spu2_transfer:folder=folder.with_name(folder.name+'-spu2-transfer')
    if args.capture_spu2_interrupts:folder=folder.with_name(folder.name+'-spu2-interrupts')
    if args.capture_spu2_status:folder=folder.with_name(folder.name+'-spu2-status')
    # The user's portable.ini overrides -datapath. Launch a local package copy
    # without that marker; never change the user's installation/profile.
    package = folder / 'package'
    package.mkdir(parents=True, exist_ok=True)
    for dependency in [executable, *executable.parent.glob('*.dll')]:
        target = package / dependency.name
        if not target.exists() or hashlib.sha256(target.read_bytes()).digest() != hashlib.sha256(dependency.read_bytes()).digest():
            shutil.copyfile(dependency, target)
    for name in ('resources', 'QtPlugins'):
        shutil.copytree(executable.parent / name, package / name, dirs_exist_ok=True)
    qt_config = executable.parent / 'qt.conf'
    if qt_config.exists():
        shutil.copyfile(qt_config, package / 'qt.conf')
    executable = package / executable.name
    profile_root = folder / 'profile'
    profile = profile_root / 'PCSX2'
    for directory in ('inis', 'bios', 'logs', 'memcards', 'cache'):
        (profile / directory).mkdir(parents=True, exist_ok=True)
    bios_copy = profile / 'bios' / args.bios.name
    shutil.copyfile(args.bios, bios_copy)
    # PCSX2 may update NVM; any companion copies also stay in the isolated profile.
    for suffix in ('.NVM', '.MEC'):
        companion = args.bios.with_suffix(suffix)
        if companion.exists():
            shutil.copyfile(companion, bios_copy.with_suffix(suffix))
    settings = ('[UI]\nSettingsVersion = 1\nSetupWizardIncomplete = false\n'
                'StartPaused = false\nPauseOnFocusLoss = false\nConfirmShutdown = false\n'
                '[Folders]\nBios = bios\nLogs = logs\nMemoryCards = memcards\nCache = cache\n'
                f'[Filenames]\nBIOS = {bios_copy.name}\n'
                '[EmuCore]\nEnablePatches = false\nEnableCheats = false\n'
                'EnableWideScreenPatches = false\nEnableNoInterlacingPatches = false\n'
                'SaveStateOnShutdown = false\nEnableDiscordPresence = false\n'
                '[EmuCore/GS]\nRenderer = 11\n'
                '[SPU2/Output]\nOutputModule = nullout\n')
    if args.capture_spu2_status or args.capture_spu2_interrupts or args.capture_spu2_transfer or args.capture_spu2_key_readback or args.capture_spu2_initialized or args.capture_cri_low_read:
        if args.capture_point!='sounds-module-after' or args.capture_first_pad_poll or (sum(map(bool,[args.capture_spu2_status,args.capture_spu2_interrupts,args.capture_spu2_transfer,args.capture_spu2_key_readback,args.capture_spu2_initialized,args.capture_cri_low_read]))>1):raise ValueError('SPU2 status capture requires SOUNDS checkpoint without FIFO capture')
        settings+='[EmuCore/CPU/Recompiler]\nEnableEE = false\nEnableIOP = false\n'
    if args.capture_first_pad_poll:
        if args.capture_point!='sio2d-module-after':raise ValueError('First poll capture requires SIO2D return checkpoint')
        settings+='[EmuCore/CPU/Recompiler]\nEnableEE = false\nEnableIOP = false\n'
    (profile / 'inis/PCSX2.ini').write_text(settings, encoding='utf-8')
    elf = folder / 'capture.elf'
    elf.write_bytes(patched)
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    command = [str(executable), '-datapath', str(profile_root), '-nogui', '-fastboot',
               '-nofullscreen', '-logfile', str(folder / 'pcsx2.log'), '-elf', str(elf)]
    if args.disc:
        command.append(str(args.disc.resolve()))
    process = subprocess.Popen(command, cwd=folder, startupinfo=startup,
                               creationflags=subprocess.CREATE_NO_WINDOW)
    reader = None
    try:
        deadline = time.monotonic() + args.timeout
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise RuntimeError(f'PCSX2 exited ({process.returncode}); inspect ignored pcsx2.log.')
            try:
                if reader is None:
                    reader = ProcessReader(process.pid, executable)
                pointer, = struct.unpack('<Q', reader.read(reader.base + rva, 8))
                if pointer and reader.read(pointer + MARKER, 4) == struct.pack('<I', marker):
                    break
            except OSError:
                pass  # Module/EE allocation can still be initializing.
            time.sleep(0.1)
        else:
            raise TimeoutError('Handler marker not reached; inspect ignored pcsx2.log.')
        log = (folder / 'pcsx2.log').read_text(encoding='utf-8', errors='replace')
        if f'DataRoot Directory: {profile}' not in log:
            raise ValueError('PCSX2 did not use the isolated data profile.')
        if args.capture_spu2_key_readback:
            from capture_spu2_key_readback import capture
            capture(reader,pointer,iop_rva,original,folder,address,args)
            return
        if args.capture_spu2_initialized:
            from capture_spu2_initialized import capture
            capture(reader,pointer,iop_rva,original,folder,address,args)
            return
        if args.capture_cri_low_read:
            from capture_cri_low_read import capture
            capture(reader,pointer,iop_rva,original,folder,address,args)
            return
        if args.capture_spu2_transfer:
            from capture_spu2_transfer import capture
            capture(reader,pointer,iop_rva,original,folder,address,args)
            return
        if args.capture_spu2_interrupts:
            from capture_spu2_interrupts import capture
            capture(reader,pointer,iop_rva,original,folder,address,args)
            return
        if args.capture_spu2_status:
            from capture_spu2_status import capture
            capture(reader,pointer,iop_rva,original,folder,address,args)
            return
        if args.capture_first_pad_poll:
            from capture_pad_poll import capture
            capture(reader,pointer,iop_rva,original,folder,address,args)
            return
        data = reader.read(pointer, 32 * 1024 * 1024)
        if data[address:address + len(trap)] != trap:
            raise ValueError('EE RAM instrumentation does not match.')
        time.sleep(0.1)
        if not capture_iop and args.capture_point not in ('sif-rpc-state', 'rpc-call-sema', 'cd-semaphores', 'sif-stop-after', 'sif-reset-subaddr', 'reset-smflag-before', 'reset-smflag-after', 'reset-submit-after', 'reset-flags-after', 'reset-rpc-clear', 'reset-subaddr-clear', 'reset-cmd-clear', 'reset-ready', 'gs-csr-reset', 'gs-imr-after', 'gs-crt-after', 'gs-intc-after', 'gs-csr-field', 'gs-intc-before', 'gs-intc-cleared', 'gs-intc-event', 'gs-intc-acknowledged','gif-idle','gif-qwc','gif-after','gif-sync-before','gif-sync-after','gif-sync-poll','gif-vu-idle','gif-path-after','post-sif-before','post-sif-after','post-scmd-bind-success','post-mmode-after','post-cd-after','post-cd-bind-after','post-cd-bind-success','loadfile-bind-after','loadfile-version-after','loadfile-module-before','loadfile-module-after','mcman-module-after','mcserv-module-after','dbcman-module-after','sio2d-module-after','ds2ud-module-after','libsd-module-after','sdrdrv-module-after','modhsyn-module-after','modsesq2-module-after','cdvdstm-module-after','sounds-module-after','cri-adxi-module-after','fileio-bind-after','heap-alloc-after','heap-copy-after','module-buffer-after','heap-free-after','dmac-startup-after') and reader.read(pointer, 0x80004) != data[:0x80004]:
            raise ValueError('Kernel prefix changed during capture.')
        if capture_iop:
            sp, = struct.unpack_from('<I', data, MARKER + 8)
            if sp > len(data) - 16 or reader.read(pointer + sp, 16) != data[sp:sp + 16]:
                raise ValueError('SIF DMA descriptor changed during capture.')
            packet_address, packet_size = (0x3A97C0, 64) if args.capture_point in ('sif-bind-after', 'cd-init-after', 'scmd-bind-after', 'scmd-mmode-after') else (0x3A9580, 20)
            if reader.read(pointer + packet_address, packet_size) != data[packet_address:packet_address + packet_size]:
                raise ValueError('SIF packet changed during capture.')
        if args.capture_point in ('rpc-call-sema', 'cd-semaphores'):
            sp, = struct.unpack_from('<I', data, MARKER + 8)
            if sp > len(data) - 24 or reader.read(pointer + sp, 24) != data[sp:sp + 24]:
                raise ValueError('RPC call semaphore descriptor changed during capture.')
            if reader.read(pointer + MARKER, 12) != data[MARKER:MARKER + 12]:
                raise ValueError('RPC call semaphore publication changed during capture.')
        if args.capture_point == 'cd-semaphores':
            if reader.read(pointer + 0x3683E8, 12) != data[0x3683E8:0x3683F4]:
                raise ValueError('CD semaphore IDs changed during capture.')
        if args.capture_point in ('sif-bind-after', 'cd-init-after', 'scmd-bind-after', 'scmd-mmode-after'):
            ranges = ((MARKER, 12), (0x3A94C0, 64), (0x3A87A8, 40), (0x3A95C0, 13 * 12))
            if args.capture_point in ('scmd-bind-after', 'scmd-mmode-after'):
                ranges += ((0x369F40, 40), (0x3695C0,4), (0x369A00,4))
            if args.capture_point == 'cd-init-after':
                ranges += ((0x3695C0, 16), (0x3A8800, 4))
            for start, size in ranges:
                if reader.read(pointer + start, size) != data[start:start + size]:
                    raise ValueError('RPC response/client/handler table changed during capture.')
        if args.capture_point == 'sif-rpc-state' and reader.read(pointer + MARKER, 12) != data[MARKER:MARKER + 12]:
            raise ValueError('SIF query publication changed during capture.')
        iop_data = None
        if capture_boot_iop:
            iop_pointer, = struct.unpack('<Q', reader.read(reader.base + iop_rva, 8))
            if not iop_pointer:raise ValueError('IOP memory is not allocated.')
            iop_data=reader.read(iop_pointer,2*1024*1024)
            if args.capture_point in ('post-sif-before','post-sif-after','post-scmd-bind-success','post-mmode-after','post-cd-after','post-cd-bind-after','post-cd-bind-success','loadfile-bind-after','loadfile-version-after','loadfile-module-before','loadfile-module-after','mcman-module-after','mcserv-module-after','dbcman-module-after','sio2d-module-after','ds2ud-module-after','libsd-module-after','sdrdrv-module-after','modhsyn-module-after','modsesq2-module-after','cdvdstm-module-after','sounds-module-after','cri-adxi-module-after','fileio-bind-after','heap-alloc-after','heap-copy-after','module-buffer-after','heap-free-after','dmac-startup-after'):
                for start,size in ((0x1E640,20),(0x1E700,16)):
                    if reader.read(iop_pointer+start,size)!=iop_data[start:start+size]:
                        raise ValueError('Fresh SIF IOP mailbox/ownership changed.')
            if args.capture_point in ('post-cd-after','post-cd-bind-after','post-cd-bind-success','post-mmode-after','post-scmd-bind-success'):
                for start,size in ((0x1E640,64),(0x1E700,16),(0x43388,68),(0x43358,12),(0x441B8,16)):
                    if reader.read(iop_pointer+start,size)!=iop_data[start:start+size]:raise ValueError('Fresh CD service state changed.')
                for start,size in ((MARKER,44),(0x3A94C0,64),(0x3A97C0,64),(0x3A87A8,40),(0x3A8800,4),(0x3695C0,16)):
                    if reader.read(pointer+start,size)!=data[start:start+size]:raise ValueError('Fresh CD response/client changed.')
            if args.capture_point in ('post-mmode-after','post-scmd-bind-success'):
                for start,size in ((0x369F40,40),(0x369A00,4)):
                    if reader.read(pointer+start,size)!=data[start:start+size]:raise ValueError('Fresh SCMD client changed.')
                server=struct.unpack_from('<I',data,0x369F64)[0]
                if not server or server>len(iop_data)-68:raise ValueError('Fresh SCMD server bounds differ')
                for start,size in ((server,68),(struct.unpack_from('<I',iop_data,server+8)[0],4)):
                    if start>len(iop_data)-size or reader.read(iop_pointer+start,size)!=iop_data[start:start+size]:raise ValueError('Fresh SCMD ownership changed.')
            if args.capture_point in LOADFILE_POINTS:
                client_address=0x3AC840 if args.capture_point in ('fileio-bind-after','heap-alloc-after','heap-copy-after','heap-free-after') else 0x3ACC00
                for start,size in ((MARKER,44),(client_address,40),(0x3ACA00,512),(0x3A94C0,64),(0x3A97C0,64)):
                    if reader.read(pointer+start,size)!=data[start:start+size]:raise ValueError('LOADFILE client/request changed.')
                server=struct.unpack_from('<I',data,client_address+36)[0]
                if not server or server>len(iop_data)-68:raise ValueError('LOADFILE server bounds differ')
                buffer=struct.unpack_from('<I',iop_data,server+8)[0]
                for start,size in ((server,68),(buffer,256 if args.capture_point in ('fileio-bind-after','heap-alloc-after','heap-copy-after','heap-free-after') else 512)):
                    if start>len(iop_data)-size or reader.read(iop_pointer+start,size)!=iop_data[start:start+size]:raise ValueError('LOADFILE service changed.')
            (folder / 'iop-ram.bin').write_bytes(iop_data)
            if reader.read(pointer+MARKER,12)!=data[MARKER:MARKER+12]:raise ValueError('Boot-ready publication changed.')
        if capture_iop:
            # Pinned PCSX2 MemoryTypes.h: Main is the first member of IOPmem.
            iop_pointer, = struct.unpack('<Q', reader.read(reader.base + iop_rva, 8))
            if not iop_pointer:
                raise ValueError('IOP memory is not allocated.')
            iop_data = reader.read(iop_pointer, 2 * 1024 * 1024)
            time.sleep(0.1)
            receive_size = 64 if args.capture_point in ('sif-bind-after', 'cd-init-after', 'scmd-bind-after', 'scmd-mmode-after') else 20
            for start, size in ((0x19600, receive_size), (0x196C0, 12)) + (((0x3F570, 68), (0x3F558, 12)) if args.capture_point in ('sif-bind-after', 'cd-init-after', 'scmd-bind-after', 'scmd-mmode-after') else ()) + (((0x3FB20, 16),) if args.capture_point == 'cd-init-after' else ()):
                if reader.read(iop_pointer + start, size) != iop_data[start:start + size]:
                    raise ValueError('IOP receive packet/table changed during capture.')
            if args.capture_point in ('scmd-bind-after','scmd-mmode-after'):
                server = struct.unpack_from('<I', data, 0x369F64)[0]
                if not server or server > len(iop_data) - 68 or reader.read(iop_pointer + server, 68) != iop_data[server:server + 68]:
                    raise ValueError('SCMD service changed during capture.')
                buffer = struct.unpack_from('<I', iop_data, server + 8)[0]
                if buffer > len(iop_data) - 4 or reader.read(iop_pointer + buffer,4) != iop_data[buffer:buffer+4] or reader.read(iop_pointer+0x3F648,68) != iop_data[0x3F648:0x3F68C]:
                    raise ValueError('SCMD buffer/queue changed during capture.')
            (folder / 'iop-ram.bin').write_bytes(iop_data)
        if args.capture_point in ('sif-stop-after','sif-reset-subaddr'):
            for start, size in ((MARKER,12),(0x6380,44),(0x14EAC,4)):
                if reader.read(pointer+start,size) != data[start:start+size]:
                    raise ValueError('SIF stop publication/kernel handler changed.')
        if args.capture_point in ('reset-smflag-before','reset-smflag-after','reset-flags-after','reset-rpc-clear','reset-subaddr-clear','reset-cmd-clear'):
            if reader.read(pointer+MARKER,16) != data[MARKER:MARKER+16]:
                raise ValueError('Reset SMFLAG publication changed.')
        if args.capture_point == 'reset-submit-after':
            for start,size in ((MARKER,12),(0x3ACC40,104)):
                if reader.read(pointer+start,size) != data[start:start+size]:
                    raise ValueError('Reboot submission publication/packet changed.')
        if args.capture_point in ('gs-csr-reset','gs-imr-after','gs-crt-after','gs-intc-after','gs-csr-field','gs-intc-before','gs-intc-cleared','gs-intc-event','gs-intc-acknowledged','gif-idle','gif-qwc','gif-after','gif-sync-before','gif-sync-after','gif-sync-poll','gif-vu-idle','gif-path-after','post-sif-before','post-sif-after','post-scmd-bind-success','post-mmode-after','post-cd-after','post-cd-bind-after','post-cd-bind-success','loadfile-bind-after','loadfile-version-after','loadfile-module-before','loadfile-module-after','mcman-module-after','mcserv-module-after','dbcman-module-after','sio2d-module-after','ds2ud-module-after','libsd-module-after','sdrdrv-module-after','modhsyn-module-after','modsesq2-module-after','cdvdstm-module-after','sounds-module-after','cri-adxi-module-after','fileio-bind-after','heap-alloc-after','heap-copy-after','module-buffer-after','heap-free-after','dmac-startup-after') and reader.read(pointer+MARKER,24)!=data[MARKER:MARKER+24]:raise ValueError('GS CSR publication changed.')
        if args.capture_point in ('gif-sync-before','gif-sync-after','gif-sync-poll'):
            if reader.read(pointer+MARKER,44)!=data[MARKER:MARKER+44]:
                raise ValueError('GIF synchronization publication changed.')
        if args.capture_point in ('post-sif-before','post-sif-after','post-scmd-bind-success','post-mmode-after','post-cd-after','post-cd-bind-after','post-cd-bind-success','loadfile-bind-after','loadfile-version-after','loadfile-module-before','loadfile-module-after','mcman-module-after','mcserv-module-after','dbcman-module-after','sio2d-module-after','ds2ud-module-after','libsd-module-after','sdrdrv-module-after','modhsyn-module-after','modsesq2-module-after','cdvdstm-module-after','sounds-module-after','cri-adxi-module-after','fileio-bind-after','heap-alloc-after','heap-copy-after','module-buffer-after','heap-free-after','dmac-startup-after'):
            for start,size in ((MARKER,44),(0x3A87A8,40),(0x3A94C0,64),(0x3A95C0,156),(0x3A9740,4)):
                if reader.read(pointer+start,size)!=data[start:start+size]:raise ValueError('Post-reset SIF publication/table changed.')
        if args.capture_point=='gif-path-after':
            sp=struct.unpack_from('<I',data,MARKER+16)[0]
            if reader.read(pointer+sp+320,240)!=data[sp+320:sp+560]:
                raise ValueError('Second GIF packet changed.')
        if args.capture_point=='gif-idle':
            sp=struct.unpack_from('<I',data,MARKER+16)[0]
            packet=sp+96
            count=1+(struct.unpack_from('<Q',data,packet)[0]&0x7fff)
            if count!=15 or packet+count*16>len(data):raise ValueError('First GIF packet bounds/count differ.')
            for start,size in ((MARKER,44),(packet,count*16)):
                if reader.read(pointer+start,size)!=data[start:start+size]:raise ValueError('GIF publication/packet changed.')
        dump = folder / 'ee-ram.bin'
        dump.write_bytes(data)
        disc_sha = None
        if args.disc:
            with args.disc.open('rb') as disc_stream:
                disc_sha = hashlib.file_digest(disc_stream,'sha256').hexdigest()
        manifest = {'disc_sha256':disc_sha, 'pcsx2_sha256': hashlib.sha256(exe_data).hexdigest(),
                    'bios_sha256': hashlib.sha256(bios_copy.read_bytes()).hexdigest(),
                    'source_elf_sha256': hashlib.sha256(original).hexdigest(),
                    'instrumented_elf_sha256': hashlib.sha256(patched).hexdigest(),
                    'capture_sha256': hashlib.sha256(data).hexdigest(),
                    'capture_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
                    'authored_extra_segment': {'address':FRESH_BIND_STUB,'words':list(struct.unpack('<'+str(len(fresh_bind_stub(args.capture_point.endswith('success'),args.capture_point=='post-scmd-bind-success'))//4)+'I',fresh_bind_stub(args.capture_point.endswith('success'),args.capture_point=='post-scmd-bind-success')))} if args.capture_point in ('post-cd-bind-after','post-cd-bind-success','post-scmd-bind-success') else None,
                    'patch': {'address': address, 'file_offset': offset, 'size': len(trap),
                              'authored_words': list(struct.unpack('<' + str(len(trap) // 4) + 'I', trap)),
                              'marker_address': MARKER, 'marker_value': marker},
                    'capture_point': args.capture_point + ': authored marker and self-loop at the observed boundary',
                    'observed_timer3_mode': struct.unpack_from('<I', data, MARKER + 4)[0] if args.capture_point == 'timer3-mode' else None,
                    'observed_timer2_mode': struct.unpack_from('<I', data, MARKER + 4)[0] if args.capture_point == 'timer2-mode' else None,
                    'observed_intc_mask': struct.unpack_from('<I', data, MARKER + 4)[0] if args.capture_point == 'intc-mask' else None,
                    'observed_timer2_readback': struct.unpack_from('<I', data, MARKER + 4)[0] if args.capture_point == 'timer2-readback' else None,
                    'observed_osd_config': struct.unpack_from('<I', data, MARKER + 4)[0] if args.capture_point == 'osd-config' else None,
                    'observed_osd_readback': struct.unpack_from('<I', data, MARKER + 4)[0] if args.capture_point == 'osd-readback' else None,
                    'observed_dma_status': struct.unpack_from('<I', data, MARKER + 4)[0] if args.capture_point == 'dma-status' else None,
                    'observed_dma5_control': struct.unpack_from('<I', data, MARKER + 4)[0] if args.capture_point == 'dma5-control' else None,
                    'observed_sif_command': struct.unpack_from('<I', data, MARKER + 4)[0] if args.capture_point == 'sif-command' else None,
                    'observed_sif_selector': struct.unpack_from('<I', data, MARKER + 8)[0] if args.capture_point == 'sif-command' else None,
                    'observed_sif_rpc_state': struct.unpack_from('<I', data, MARKER + 4)[0] if args.capture_point == 'sif-rpc-state' else None,
                    'observed_sif_rpc_selector': struct.unpack_from('<I', data, MARKER + 8)[0] if args.capture_point == 'sif-rpc-state' else None,
                    'iop_capture_sha256': hashlib.sha256(iop_data).hexdigest() if iop_data is not None else None,
                    'observed_sif_dma_result': struct.unpack_from('<I', data, MARKER + 4)[0] if capture_iop else None,
                    'observed_sif_dma_sp': struct.unpack_from('<I', data, MARKER + 8)[0] if capture_iop else None,
                    'original_osd_config': struct.unpack_from('<I', data, MARKER + 8)[0] if args.capture_point == 'osd-readback' else None,
                    'limitations': ('Disc-backed LOADFILE caller checkpoint; publication/client/request and service/buffer stable twice. Full IOP RAM live, no native module execution established.' if args.capture_point in LOADFILE_POINTS else 'Post-GS disc-backed unique SIF caller; EE publication/table/SREG and selected IOP mailbox/ownership ranges stable twice. Full IOP RAM remains live; no native IOP boot or scheduling established.' if args.capture_point.startswith('post-sif-') else 'Disc-backed independent GIF checkpoint; register/return/SP publication stable twice. First source packet checked twice at gif-idle, second source packet checked twice at gif-path-after. No continuous timing, native transfer or presentation inferred.' if args.capture_point.startswith('gif-') else 'Disc-backed independent INTC point; status, mask, SP and preceding v0 publication stable twice. No continuous timing trace, native video clock or callback execution established.' if args.capture_point in ('gs-intc-before','gs-intc-cleared','gs-intc-event','gs-intc-acknowledged','gif-idle','gif-qwc','gif-after','gif-sync-before','gif-sync-after','gif-sync-poll','gif-vu-idle','gif-path-after','post-sif-before','post-sif-after','post-scmd-bind-success','post-mmode-after','post-cd-after','post-cd-bind-after','post-cd-bind-success','loadfile-bind-after','loadfile-version-after','loadfile-module-before','loadfile-module-after','mcman-module-after','mcserv-module-after','dbcman-module-after','sio2d-module-after','ds2ud-module-after','libsd-module-after','sdrdrv-module-after','modhsyn-module-after','modsesq2-module-after','cdvdstm-module-after','sounds-module-after','cri-adxi-module-after','fileio-bind-after','heap-alloc-after','heap-copy-after','module-buffer-after','heap-free-after','dmac-startup-after') else 'Disc-backed reached GS register/syscall return, 64-bit value and SP publication stable twice; no renderer or general GS state captured.' if args.capture_point in ('gs-csr-reset','gs-imr-after','gs-crt-after','gs-intc-after','gs-csr-field','gs-intc-before','gs-intc-cleared','gs-intc-event','gs-intc-acknowledged','gif-idle','gif-qwc','gif-after','gif-sync-before','gif-sync-after','gif-sync-poll','gif-vu-idle','gif-path-after','post-sif-before','post-sif-after','post-scmd-bind-success','post-mmode-after','post-cd-after','post-cd-bind-after','post-cd-bind-success','loadfile-bind-after','loadfile-version-after','loadfile-module-before','loadfile-module-after','mcman-module-after','mcserv-module-after','dbcman-module-after','sio2d-module-after','ds2ud-module-after','libsd-module-after','sdrdrv-module-after','modhsyn-module-after','modsesq2-module-after','cdvdstm-module-after','sounds-module-after','cri-adxi-module-after','fileio-bind-after','heap-alloc-after','heap-copy-after','module-buffer-after','heap-free-after','dmac-startup-after') else 'Disc-mounted ready-branch EE publication stable twice; post-reset IOP RAM is a live diagnostic snapshot, not atomically frozen or proven stable. No native IOP execution validated.' if capture_boot_iop else 'Instrumented ELF with explicit local disc when disc_sha256 is present; reboot submission publication/EE packet stable twice; no IOP image execution or reset-ready result inferred.' if args.capture_point == 'reset-submit-after' else 'Instrumented ELF with explicit local disc when disc_sha256 is present; register return/SP and direct SMFLAG readback publication stable twice; no post-reset IOP snapshot or boot completion established.' if args.capture_point in ('reset-flags-after','reset-rpc-clear','reset-subaddr-clear','reset-cmd-clear') else 'Standalone ELF; authored direct SMFLAG read before/after reached register write, publication stable twice; no reset completion established.' if args.capture_point in ('reset-smflag-before','reset-smflag-after','reset-flags-after','reset-rpc-clear','reset-subaddr-clear','reset-cmd-clear') else 'Standalone ELF; SifStopDma return/publication and kernel implementation windows stable twice; no active DMA cancellation or IOP reset captured.' if args.capture_point in ('sif-stop-after','sif-reset-subaddr') else 'Instrumented standalone ELF with no disc; second bind/media-mode request/reply/client and server windows stable twice, semaphore inferred from completed client and residual descriptor; no timing or hardware DMA result established.' if args.capture_point in ('scmd-bind-after','scmd-mmode-after') else 'Instrumented standalone ELF boot; semaphore descriptor/publication/IDs stable in two reads; no scheduler or attribute semantics established.' if args.capture_point == 'cd-semaphores' else 'Instrumented standalone ELF boot; CD-init request/reply/client/data and IOP service windows stable in two reads; no disc mounted or hardware timing established.' if args.capture_point == 'cd-init-after' else 'Instrumented standalone ELF boot; semaphore descriptor/publication stable in two reads; kernel and IOP remain live.' if args.capture_point == 'rpc-call-sema' else 'Instrumented standalone ELF boot; EE running in loop, IOP active. Bind request, response, client, handler table and IOP service/queue stable in two reads; no interrupt timing captured.' if args.capture_point == 'sif-bind-after' else 'Instrumented standalone ELF boot; EE running in loop, IOP active. SIF descriptor/packet and IOP receive packet/table stable in two reads; kernel and IOP continue running.' if capture_iop else 'Instrumented standalone ELF boot; EE running in loop, IOP active. SIF query publication stable; kernel continues running.' if args.capture_point == 'sif-rpc-state' else 'Instrumented standalone ELF boot; EE running in loop, IOP active. Kernel prefix stable in two reads.') + ' Diagnostic only; no gameplay validated.'}
        (folder / 'capture.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
        print(f'Captured {len(data)} bytes at {args.capture_point}: {dump}')
    finally:
        if reader:
            reader.close()
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)


if __name__ == '__main__':
    main()
