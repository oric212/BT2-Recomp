"""Capture EE RAM at the observed BT2 kernel scan using an isolated PCSX2 process.

Only a local ELF copy is instrumented. Process access is read-only; captures,
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


def instrument(data):
    metadata = inspect(data)
    if metadata['sha256'] != EXPECTED_SHA:
        raise ValueError('Instrumentation requires the validated local BT2 executable hash.')
    matches = [seg for seg in metadata['load_segments']
               if seg['flags'] & 1 and seg['virtual_address'] <= HANDLER
               and HANDLER + len(TRAP) <= seg['virtual_address'] + seg['file_size']]
    if len(matches) != 1:
        raise ValueError('Handler must lie in exactly one executable file-backed segment.')
    seg = matches[0]
    offset = seg['file_offset'] + HANDLER - seg['virtual_address']
    patched = bytearray(data)
    patched[offset:offset + len(TRAP)] = TRAP
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
    parser.add_argument('--timeout', type=float, default=40)
    args = parser.parse_args()
    if sys.platform != 'win32' or struct.calcsize('P') != 8:
        parser.error('Requires 64-bit Windows Python.')
    executable = args.pcsx2.resolve()
    exe_data = executable.read_bytes()
    rva = export_rva(exe_data, 'EEmem')
    source = ROOT / 'rom/SLUS_214.41'
    original = source.read_bytes()
    patched, offset = instrument(original)
    folder = ROOT / 'work/pcsx2-capture'
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
    (profile / 'inis/PCSX2.ini').write_text(settings, encoding='utf-8')
    elf = folder / 'capture.elf'
    elf.write_bytes(patched)
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    command = [str(executable), '-datapath', str(profile_root), '-nogui', '-fastboot',
               '-nofullscreen', '-logfile', str(folder / 'pcsx2.log'), '-elf', str(elf)]
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
                if pointer and reader.read(pointer + MARKER, 4) == struct.pack('<I', 0xB702):
                    break
            except OSError:
                pass  # Module/EE allocation can still be initializing.
            time.sleep(0.1)
        else:
            raise TimeoutError('Handler marker not reached; inspect ignored pcsx2.log.')
        log = (folder / 'pcsx2.log').read_text(encoding='utf-8', errors='replace')
        if f'DataRoot Directory: {profile}' not in log:
            raise ValueError('PCSX2 did not use the isolated data profile.')
        data = reader.read(pointer, 32 * 1024 * 1024)
        if data[HANDLER:HANDLER + len(TRAP)] != TRAP:
            raise ValueError('EE RAM instrumentation does not match.')
        time.sleep(0.1)
        if reader.read(pointer, 0x80004) != data[:0x80004]:
            raise ValueError('Kernel prefix changed during capture.')
        dump = folder / 'ee-ram.bin'
        dump.write_bytes(data)
        manifest = {'pcsx2_sha256': hashlib.sha256(exe_data).hexdigest(),
                    'bios_sha256': hashlib.sha256(bios_copy.read_bytes()).hexdigest(),
                    'source_elf_sha256': hashlib.sha256(original).hexdigest(),
                    'instrumented_elf_sha256': hashlib.sha256(patched).hexdigest(),
                    'capture_sha256': hashlib.sha256(data).hexdigest(),
                    'capture_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
                    'patch': {'address': HANDLER, 'file_offset': offset, 'size': len(TRAP),
                              'authored_words': list(struct.unpack('<6I', TRAP)),
                              'marker_address': MARKER, 'marker_value': 0xB702},
                    'capture_point': 'Reached handler entry, replaced with marker stores and self-loop.',
                    'limitations': 'Instrumented standalone ELF boot; EE running in loop, IOP active. Kernel prefix stable in two reads. Diagnostic only; no gameplay validated.'}
        (folder / 'capture.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
        print(f'Captured {len(data)} bytes at handler trap: {dump}')
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
