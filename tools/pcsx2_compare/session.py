"""One PCSX2 run with checkpoints at call sites (docs/plans/validation/PCSX2_COMPARISON_PLAN.md, step 1).

Each `--site NAME:FIRST:COUNT:STRIDE[:after]` takes the site NAME of config/pcsx2_compare_sites.json and captures
its calls FIRST, FIRST + STRIDE, ... (COUNT of them, counted from 1 over the run), before the call and, with
`:after`, after it returns too. An instrumented copy of the executable sends each site's call through a stub
(context_block.py) that publishes the registers and waits; the host reads EE RAM (`EEmem`), VU memory (`VUmem`) and
the register block out of the PCSX2 process, tells the stub how many calls to let pass, and lets it continue.

PCSX2 runs unthrottled with a null renderer, no sound and a copy of a memory card holding a BT2 save (no prompts),
so the title's demo battle (CPU against CPU) starts by itself. Everything stays under ignored
work/pcsx2-compare/<session>/: session.json and a folder per checkpoint (<site>-<call>-<before|after>) holding
ee-ram.bin, vu-mem.bin and registers.bin.

`--callees-of FUNC:FIRST:COUNT:STRIDE[:after]` adds a site for every call inside a function (the step that splits
a differing function into its callees). NAME may be @<hex address> of any `jal` in the executable, or %<hex function> (the function's entry: every call of
it, from the overlay or through a pointer too), instead of a catalogue name. `--edit SITE:REG=VALUE[/VALUE...]`
changes a0 to a3, t0 to t3 (before the call) or v0, v1 (after it) at the site's successive checkpoints, so a session
can steer the game (the demo battle's fighters, stage and kind). `--idle S` ends the session S
seconds after the last checkpoint (sites a run does not reach are reported).

Usage: python tools/pcsx2_compare/session.py --pcsx2 <pcsx2-qt.exe> --bios <bios> --disc <iso> --card <Mcd001.ps2>
       --session <name> --site think:200:3:350:after [--site ...] [--timeout 1800]
"""
import argparse
import ctypes
import hashlib
import json
import shutil
import struct
import subprocess
import sys
import time
from ctypes import wintypes
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(ROOT / 'tools/capture_ee'))
import context_block as cb  # noqa: E402
import capture_ee as ce  # noqa: E402  (the ELF check and the PE export reader)

SITES = ROOT / 'config/pcsx2_compare_sites.json'
# The added segment. 0x1BF4000 to 0x1EFD000 stayed zero in every port dump of 2026-11-03 (title, fights, the demo
# battle); the game's heap reaches the capture tool's 0x01000000 by a fight.
SEGMENT = 0x01D00000
EE_RAM, VU_MEMORY = 32 * 1024 * 1024, 0xA000      # VU0 micro, VU0 data (4 KB each), VU1 micro, VU1 data (16 KB each)


class ProcessMemory:
    """Reads and writes the memory of the PCSX2 process this tool launched."""

    def __init__(self, pid, executable):
        k = ctypes.WinDLL('kernel32', use_last_error=True)
        p = ctypes.WinDLL('psapi', use_last_error=True)
        k.OpenProcess.argtypes, k.OpenProcess.restype = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD], wintypes.HANDLE
        k.ReadProcessMemory.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
        k.WriteProcessMemory.argtypes = k.ReadProcessMemory.argtypes
        k.CloseHandle.argtypes = [wintypes.HANDLE]
        p.EnumProcessModulesEx.argtypes = [wintypes.HANDLE, ctypes.c_void_p, wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD]
        p.GetModuleFileNameExW.argtypes = [wintypes.HANDLE, wintypes.HMODULE, wintypes.LPWSTR, wintypes.DWORD]
        self.k = k
        # Query information, read, write and operation rights on this process only.
        self.handle = k.OpenProcess(0x400 | 0x10 | 0x20 | 0x08, False, pid)
        if not self.handle:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            modules, needed = (wintypes.HMODULE * 1024)(), wintypes.DWORD()
            if not p.EnumProcessModulesEx(self.handle, modules, ctypes.sizeof(modules), ctypes.byref(needed), 3) or not modules[0]:
                raise OSError('The main module is not loaded yet.')
            path = ctypes.create_unicode_buffer(32768)
            if not p.GetModuleFileNameExW(self.handle, modules[0], path, len(path)):
                raise ctypes.WinError(ctypes.get_last_error())
            if Path(path.value).resolve() != executable.resolve():
                raise ValueError('The launched process is not the requested PCSX2.')
            self.base = modules[0]
        except Exception:
            self.close()
            raise

    def read(self, address, size):
        buffer, got = ctypes.create_string_buffer(size), ctypes.c_size_t()
        if not self.k.ReadProcessMemory(self.handle, address, buffer, size, ctypes.byref(got)) or got.value != size:
            raise ctypes.WinError(ctypes.get_last_error())
        return buffer.raw

    def write(self, address, data):
        got = ctypes.c_size_t()
        if not self.k.WriteProcessMemory(self.handle, address, data, len(data), ctypes.byref(got)) or got.value != len(data):
            raise ctypes.WinError(ctypes.get_last_error())

    def close(self):
        if self.handle:
            self.k.CloseHandle(self.handle)
            self.handle = None


def minimize_windows(pid):
    """Minimizes the visible windows of the PCSX2 process this tool launched, without taking the focus (the software
    renderer opens a window; the user keeps working on the PC meanwhile)."""
    user32 = ctypes.WinDLL('user32', use_last_error=True)
    found = []

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def visit(window, _):
        owner = wintypes.DWORD()
        user32.GetWindowThreadProcessId(window, ctypes.byref(owner))
        if owner.value == pid and user32.IsWindowVisible(window) and not user32.IsIconic(window):
            found.append(window)
        return True

    user32.EnumWindows(visit, 0)
    for window in found:
        user32.ShowWindow(window, 7)  # SW_SHOWMINNOACTIVE


def parse_site(text, catalogue):
    parts = text.split(':')
    if len(parts) not in (4, 5) or (len(parts) == 5 and parts[4] != 'after'):
        raise ValueError(f'A site is NAME:FIRST:COUNT:STRIDE[:after], not {text!r}')
    name, first, count, stride = parts[0], *map(int, parts[1:4])
    if first < 1 or count < 1 or stride < 1:
        raise ValueError('FIRST, COUNT and STRIDE are 1 or more.')
    entry = name.startswith('%')
    if name.startswith('@'):                                   # A call by its address; the target from its jal.
        call, target = int(name[1:], 16), None
    elif entry:                                                # A function's entry: every call of it.
        call = target = int(name[1:], 16)
    elif name in catalogue:
        call, target = int(catalogue[name]['call'], 16), int(catalogue[name]['target'], 16)
    else:
        raise ValueError(f'Unknown site {name!r}; config/pcsx2_compare_sites.json has {", ".join(catalogue)}, '
                         '@<hex call address> or %<hex function>')
    return {'name': name, 'call': call, 'target': target, 'first': first, 'count': count, 'stride': stride,
            'after': len(parts) == 5, 'entry': entry}


REGISTERS = {'a0': 4, 'a1': 5, 'a2': 6, 'a3': 7, 't0': 8, 't1': 9, 't2': 10, 't3': 11, 'v0': 2, 'v1': 3}
AFTER_REGISTERS = {'v0', 'v1'}  # Loaded back after the call; the others before it.


def parse_edit(text, sites):
    """SITE:REG=VALUE[/VALUE...][,REG=...]: the registers a site's k-th checkpoint gets (the k-th value, the last
    one from then on), written into the block before the stub loads it back."""
    name, _, rest = text.partition(':')
    if name not in {s['name'] for s in sites} or not rest:
        raise ValueError(f'An edit is SITE:REG=VALUE[/VALUE...][,REG=...] for a site of this session, not {text!r}')
    edits = {}
    for item in rest.split(','):
        register, _, values = item.partition('=')
        if register not in REGISTERS or not values:
            raise ValueError(f'{item!r}: registers are {", ".join(REGISTERS)}')
        edits[register] = [int(v, 0) for v in values.split('/')]
    return name, edits


def callees(data, text):
    """FUNC:FIRST:COUNT:STRIDE[:after] as one site per movable jal inside the function (its extent from the generation
    map, work/bt2-generation/candidates.csv)."""
    function, rest = text.split(':', 1)
    start = int(function, 16)
    rows = [line.split(',') for line in (ROOT / 'work/bt2-generation/candidates.csv').read_text().splitlines()[1:]]
    end = next((int(r[2], 16) for r in rows if int(r[1], 16) == start), None)
    if end is None:
        raise ValueError(f'{start:#x} is not a function of the generation map.')
    seg = next(s for s in ce.inspect(data)['load_segments'] if s['flags'] & 1 and s['virtual_address'] <= start < s['virtual_address'] + s['file_size'])
    out = []
    for address in range(start, end, 4):
        word, delay = struct.unpack_from('<2I', data, seg['file_offset'] + address - seg['virtual_address'])
        if word >> 26 == 3:
            try:
                cb.check_delay_slot(delay)
                out.append(f'@{address:X}:{rest}')
            except ValueError as error:
                print(f'{address:#x} left out: {error}')
    return out


def instrument(data, sites):
    """The executable with each site's call sent to its stub, the stubs in one added load segment."""
    metadata = ce.inspect(data)
    if metadata['sha256'] != ce.EXPECTED_SHA:
        raise ValueError('Instrumentation requires the validated local BT2 executable.')
    if any(s['virtual_address'] < SEGMENT + len(sites) * cb.SLOT and SEGMENT < s['virtual_address'] + s['memory_size']
           for s in metadata['load_segments']):
        raise ValueError('The added segment overlaps the game.')
    if SEGMENT + len(sites) * cb.SLOT > 0x01EFD000:
        raise ValueError('Too many sites for the region the game leaves empty (0x1D00000 to 0x1EFD000).')
    patched, segment = bytearray(data), bytearray()
    for k, site in enumerate(sites):
        hook, target = site['call'], site['target']
        seg = next((s for s in metadata['load_segments']
                    if s['flags'] & 1 and s['virtual_address'] <= hook < s['virtual_address'] + s['file_size']), None)
        if seg is None:
            raise ValueError(f'{site["name"]}: {hook:#x} is not in the executable (overlay sites are not supported yet).')
        offset = seg['file_offset'] + hook - seg['virtual_address']
        call, delay = struct.unpack_from('<2I', data, offset)
        slot = SEGMENT + k * cb.SLOT
        if site['entry']:
            struct.pack_into('<2I', patched, offset, 0x08000000 | (slot >> 2), 0)
            segment += cb.stub_words(slot, target, 0, target + 8, site['first'] - 1, site['after'], entry=[call, delay])
            site['slot'] = slot
            continue
        if target is None and call >> 26 == 3:
            target = site['target'] = (call & 0x3FFFFFF) << 2
        if target is None or call != 0x0C000000 | (target >> 2):
            raise ValueError(f'{site["name"]}: the instruction at {hook:#x} is not jal {target:#x}.')
        struct.pack_into('<2I', patched, offset, 0x08000000 | (slot >> 2), 0)
        segment += cb.stub_words(slot, target, delay, hook + 8, site['first'] - 1, site['after'])
        site['slot'] = slot
    phoff, = struct.unpack_from('<I', data, 28)
    phsize, count = struct.unpack_from('<HH', data, 42)
    empty = [phoff + k * phsize for k in range(count)
             if struct.unpack_from('<8I', data, phoff + k * phsize)[0] == 1 and struct.unpack_from('<8I', data, phoff + k * phsize)[4:6] == (0, 0)]
    if len(empty) != 1:
        raise ValueError('Expected one unused ELF load record.')
    file_offset = (len(patched) + 4095) & ~4095
    patched.extend(bytes(file_offset - len(patched)) + segment)
    struct.pack_into('<8I', patched, empty[0], 1, file_offset, SEGMENT, SEGMENT, len(segment), len(segment), 7, 4096)
    return bytes(patched)


def prepare(folder, pcsx2, bios, card):
    """A copy of PCSX2 without the user's portable marker, and a profile of its own (as the capture tools do)."""
    package = folder / 'package'
    package.mkdir(parents=True, exist_ok=True)
    for dependency in [pcsx2, *pcsx2.parent.glob('*.dll')]:
        target = package / dependency.name
        if not target.exists() or target.stat().st_size != dependency.stat().st_size:
            shutil.copyfile(dependency, target)
    for name in ('resources', 'QtPlugins'):
        if (pcsx2.parent / name).exists():
            shutil.copytree(pcsx2.parent / name, package / name, dirs_exist_ok=True)
    if (pcsx2.parent / 'qt.conf').exists():
        shutil.copyfile(pcsx2.parent / 'qt.conf', package / 'qt.conf')
    profile = folder / 'profile' / 'PCSX2'
    for directory in ('inis', 'bios', 'logs', 'memcards', 'cache'):
        (profile / directory).mkdir(parents=True, exist_ok=True)
    shutil.copyfile(bios, profile / 'bios' / bios.name)
    shutil.copyfile(card, profile / 'memcards' / 'Mcd001.ps2')
    (profile / 'inis/PCSX2.ini').write_text(
        '[UI]\nSettingsVersion = 1\nSetupWizardIncomplete = false\nStartPaused = false\nPauseOnFocusLoss = false\nConfirmShutdown = false\n'
        '[Folders]\nBios = bios\nLogs = logs\nMemoryCards = memcards\nCache = cache\n'
        f'[Filenames]\nBIOS = {bios.name}\n'
        '[EmuCore]\nEnablePatches = false\nEnableCheats = false\nEnableWideScreenPatches = false\nEnableNoInterlacingPatches = false\n'
        'SaveStateOnShutdown = false\nEnableDiscordPresence = false\n'
        # The software renderer (13): the null renderer (11) stopped PCSX2 at an assertion
        # (IsCoverageAlphaSupported) when a versus battle began drawing; the software one draws as the console.
        '[EmuCore/GS]\nRenderer = 13\nFrameLimitEnable = false\n'
        '[SPU2/Output]\nOutputModule = nullout\n'
        '[MemoryCards]\nSlot1_Enable = true\nSlot1_Filename = Mcd001.ps2\nSlot2_Enable = false\n', encoding='utf-8')
    return package / pcsx2.name


def run(args, sites):
    folder = ROOT / 'work/pcsx2-compare' / args.session
    folder.mkdir(parents=True, exist_ok=True)
    executable = prepare(folder, args.pcsx2.resolve(), args.bios.resolve(), args.card.resolve())
    image = executable.read_bytes()
    ee_rva, vu_rva = ce.export_rva(image, 'EEmem'), ce.export_rva(image, 'VUmem')
    elf = folder / 'capture.elf'
    elf.write_bytes(instrument((ROOT / 'rom/SLUS_214.41').read_bytes(), sites))
    log = folder / 'pcsx2.log'
    command = [str(executable), '-datapath', str(folder / 'profile'), '-nogui', '-fastboot', '-nofullscreen',
               '-logfile', str(log), '-elf', str(elf), str(args.disc.resolve())]
    print(f'PCSX2 log: {log}', flush=True)
    started = time.monotonic()
    process = subprocess.Popen(command, cwd=folder, creationflags=subprocess.CREATE_NO_WINDOW)
    memory, taken = None, []
    left = {s['name']: s['count'] for s in sites}
    try:
        def busy(site):
            return left[site['name']] or site.get('pending_after')
        while any(busy(s) for s in sites if not args.until or s['name'] == args.until):
            if taken and args.idle and time.monotonic() - started - taken[-1]['seconds'] > args.idle:
                print(f'No checkpoint for {args.idle} s; not reached: '
                      + ', '.join(f'{n} ({c} left)' for n, c in left.items() if c), flush=True)
                break
            if time.monotonic() - started > args.timeout:
                print(f'Timed out after {args.timeout} s; not finished: '
                      + ', '.join(f'{n} ({c} left)' for n, c in left.items() if c), flush=True)
                break
            if process.poll() is not None:
                print(f'PCSX2 exited ({process.returncode}) after {len(taken)} checkpoints; see {log}.', flush=True)
                break
            minimize_windows(process.pid)
            try:
                memory = memory or ProcessMemory(process.pid, executable)
                ee, = struct.unpack('<Q', memory.read(memory.base + ee_rva, 8))
                vu, = struct.unpack('<Q', memory.read(memory.base + vu_rva, 8))
            except OSError:
                time.sleep(0.2)
                continue
            if not ee or not vu:
                time.sleep(0.2)
                continue
            found = False
            for site in sites:
                data = ee + site['slot'] + cb.DATA
                count, _, _, _, mark = struct.unpack('<5I', memory.read(data, 20))
                if mark not in (cb.MARK_BEFORE, cb.MARK_AFTER):
                    continue
                found = True
                when = 'before' if mark == cb.MARK_BEFORE else 'after'
                ram, vu_memory = memory.read(ee, EE_RAM), memory.read(vu, VU_MEMORY)
                block = ram[site['slot'] + cb.DATA + cb.BLOCK:site['slot'] + cb.DATA + cb.BLOCK + cb.BLOCK_SIZE]
                out = folder / f'{site["name"]}-{count}-{when}'
                out.mkdir(exist_ok=True)
                (out / 'ee-ram.bin').write_bytes(ram)
                (out / 'vu-mem.bin').write_bytes(vu_memory)
                (out / 'registers.bin').write_bytes(block)
                taken.append({'site': site['name'], 'call': count, 'when': when, 'folder': out.name,
                              'seconds': round(time.monotonic() - started, 1),
                              'ram_sha256': hashlib.sha256(ram).hexdigest(), 'vu_sha256': hashlib.sha256(vu_memory).hexdigest()})
                print(f'{site["name"]} call {count} {when} at {taken[-1]["seconds"]} s', flush=True)
                # This checkpoint's place among the site's captures (an 'after' follows its 'before', already counted).
                index = site['count'] - left[site['name']] - (when == 'after')
                edits = {r: v[min(index, len(v) - 1)] for r, v in site.get('edits', {}).items()
                         if (r in AFTER_REGISTERS) == (when == 'after')}
                for register, value in edits.items():
                    word = value & 0xFFFFFFFF                          # A 32-bit value, sign-extended as the EE loads it.
                    memory.write(data + cb.BLOCK + cb.GPRS + 16 * REGISTERS[register],
                                 struct.pack('<q', word - (1 << 32) if word & 0x80000000 else word))
                if edits:
                    taken[-1]['edits'] = {r: hex(v) for r, v in edits.items()}
                    print(f'  set {", ".join(f"{r}={v:#x}" for r, v in edits.items())}', flush=True)
                if when == 'before':
                    left[site['name']] -= 1
                    site['pending_after'] = site['after']
                    memory.write(data + cb.SKIP, struct.pack('<I', site['stride'] - 1 if left[site['name']] else cb.NEVER))
                else:
                    site['pending_after'] = False
                memory.write(data + cb.MARK, struct.pack('<I', 0))
            if not found:
                time.sleep(0.05)
    finally:
        if memory is not None:
            memory.close()
        process.kill()
        process.wait()
        summary = {'sites': [{k: (f'{v:#x}' if k in ('call', 'target', 'slot') else v) for k, v in s.items() if k != 'pending_after'} for s in sites],
                   'segment': [SEGMENT, len(sites) * cb.SLOT], 'checkpoints': taken,
                   'pcsx2': str(args.pcsx2), 'seconds': round(time.monotonic() - started, 1)}
        (folder / 'session.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    return taken


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--pcsx2', type=Path, required=True)
    parser.add_argument('--bios', type=Path, required=True)
    parser.add_argument('--disc', type=Path, required=True)
    parser.add_argument('--card', type=Path, required=True, help='A PCSX2 memory card file with a BT2 save (copied).')
    parser.add_argument('--session', required=True, help='The session folder under work/pcsx2-compare/.')
    parser.add_argument('--site', action='append', default=[], help='NAME:FIRST:COUNT:STRIDE[:after]')
    parser.add_argument('--callees-of', action='append', default=[], help='FUNC:FIRST:COUNT:STRIDE[:after]: every call inside a function.')
    parser.add_argument('--timeout', type=float, default=1800)
    parser.add_argument('--edit', action='append', default=[], help='SITE:REG=VALUE[/VALUE...][,REG=...]: registers changed at its checkpoints.')
    parser.add_argument('--until', help='End the session when this site has taken its checkpoints (others may be left).')
    parser.add_argument('--idle', type=float, default=0, help='End the session this many seconds after the last checkpoint.')
    args = parser.parse_args()
    catalogue = json.loads(SITES.read_text(encoding='utf-8'))['sites']
    try:
        executable = (ROOT / 'rom/SLUS_214.41').read_bytes()
        texts = args.site + [t for spec in args.callees_of for t in callees(executable, spec)]
        if not texts:
            parser.error('Give --site or --callees-of.')
        sites = [parse_site(text, catalogue) for text in texts]
    except ValueError as error:
        parser.error(str(error))
    if len({s['name'] for s in sites}) != len(sites):
        parser.error('Each site once per session.')
    if args.until and args.until not in {s['name'] for s in sites}:
        parser.error('--until names a site of this session.')
    try:
        for text in args.edit:
            name, edits = parse_edit(text, sites)
            next(s for s in sites if s['name'] == name).setdefault('edits', {}).update(edits)
    except ValueError as error:
        parser.error(str(error))
    taken = run(args, sites)
    print(f'{len(taken)} checkpoints in work/pcsx2-compare/{args.session}/')
    done = {(c['site'], c['when']) for c in taken}
    return 0 if all((s['name'], 'before') in done for s in sites) else 1


if __name__ == '__main__':
    sys.exit(main())
