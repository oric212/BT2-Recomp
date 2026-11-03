"""One call of the CPU opponent's AI in PCSX2, captured before or after it (docs/plans/ee/AI_VERIFICATION_PLAN.md).

The input reader (0x207630) asks the AI for a CPU fighter's input with `jal 0x1214F8` at 0x207760 (a0: the side,
a1 = sp: a 4-byte stick buffer; the buttons return in v0). An instrumented copy of the executable sends that call
through a stub in an added segment that counts the calls and, at the N-th, publishes a0, a1, sp, gp (and v0 after
the call) and halts: before the call (`--when before`) or after it returns (`--when after`). PCSX2 runs unthrottled
with a null renderer and a copy of a memory card holding a BT2 save (no prompts), so the title's demo battle (CPU
against CPU) starts by itself; EE RAM is then read out of the PCSX2 process. Everything stays under ignored
work/pcsx2-capture/ai-call-<when>-<n>/.

Usage: python tools/capture_ee/capture_ai_call.py --pcsx2 <pcsx2-qt.exe> --bios <bios> --disc <iso>
       --card <Mcd001.ps2 with a BT2 save> --call <N> --when before|after [--site input|think] [--timeout 1800]
"""
import argparse
import hashlib
import json
import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import capture_ee as ce  # noqa: E402  (the ELF check, the PE export reader and the process reader)

# The calls a capture can stop at: `input`, the input reader's call of the AI for a CPU fighter's buttons and sticks
# (jal 0x1214F8 / daddu a1,sp,zero); `think`, the frame loop's call of the function that runs the AI's thinking for
# each side the CPU plays (0x11E988; its loop tests the side's CPU byte and calls 0x20A658, 0x126040, 0x120FF0,
# 0x124768, 0x11FE80, 0x11F7D8, 0x121550 and 0x1215F8). The call's delay slot is taken from the executable.
SITES = {'input': (0x207760, 0x1214F8), 'think': (0x102BB8, 0x11E988)}
# The added segment (code), and its data words. Not the capture tool's 0x01000000: the game's heap reaches it by a
# fight. 0x1BF4000 to 0x1EFD000 stayed zero in every port dump of 2026-11-03 (title, fights, the demo battle).
STUB = 0x01D00000
DATA = STUB + 0x300                  # +0 the call count, +4 the marker, +0x10 a0, a1, sp, gp, v0.
MARKERS = {'before': 0xB7C1, 'after': 0xB7C2}


def stub_words(n, when, target, delay, resume):
    lui, ori = 0x3C180000 | (DATA >> 16), 0x37180000 | (DATA & 0xFFFF)        # t8 = DATA
    publish = [0xAF040010, 0xAF050014, 0xAF1D0018, 0xAF1C001C, 0xAF020020,   # sw a0,a1,sp,gp,v0
               0x34190000 | MARKERS[when], 0xAF190004,                         # marker
               0x1000FFFF, 0]                                                  # b . (halt)
    words = [lui, ori, 0x8F190000, 0x27390001, 0xAF190000]                      # count += 1
    if when == 'before':
        words += [0x27390000 | ((-n) & 0xFFFF), 0x17200000 | (len(publish) + 1), 0]   # bne count-n, call (from the delay slot)
        words += publish
    words += [0x0C000000 | (target >> 2), delay]                               # the call and its delay slot
    if when == 'after':
        words += [lui, ori, 0x8F190000, 0x27390000 | ((-n) & 0xFFFF),
                  0x17200000 | (len(publish) + 1), 0]                           # bne count-n, back
        words += publish
    words += [0x08000000 | (resume >> 2), 0]                                     # back after the call
    return words


def instrument(data, n, when, site):
    hook, target = SITES[site]
    metadata = ce.inspect(data)
    if metadata['sha256'] != ce.EXPECTED_SHA:
        raise ValueError('Instrumentation requires the validated local BT2 executable.')
    seg = next(s for s in metadata['load_segments'] if s['flags'] & 1 and s['virtual_address'] <= hook < s['virtual_address'] + s['file_size'])
    offset = seg['file_offset'] + hook - seg['virtual_address']
    call, delay = struct.unpack_from('<2I', data, offset)
    if call != 0x0C000000 | (target >> 2):
        raise ValueError(f'The call at {hook:#x} differs.')
    if any(s['virtual_address'] <= STUB < s['virtual_address'] + s['memory_size'] for s in metadata['load_segments']):
        raise ValueError('The added segment overlaps the game.')
    patched = bytearray(data)
    struct.pack_into('<2I', patched, offset, 0x08000000 | (STUB >> 2), 0)
    phoff = struct.unpack_from('<I', data, 28)[0]
    phsize, count = struct.unpack_from('<HH', data, 42)
    empty = [phoff + k * phsize for k in range(count)
             if struct.unpack_from('<8I', data, phoff + k * phsize)[0] == 1 and struct.unpack_from('<8I', data, phoff + k * phsize)[4:6] == (0, 0)]
    if len(empty) != 1:
        raise ValueError('Expected one unused ELF load record.')
    segment = bytearray(0x400)
    words = stub_words(n, when, target, delay, hook + 8)
    struct.pack_into(f'<{len(words)}I', segment, 0, *words)
    file_offset = (len(patched) + 4095) & ~4095
    patched.extend(bytes(file_offset - len(patched)) + segment)
    struct.pack_into('<8I', patched, empty[0], 1, file_offset, STUB, STUB, len(segment), len(segment), 7, 4096)
    return bytes(patched)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--pcsx2', type=Path, required=True)
    parser.add_argument('--bios', type=Path, required=True)
    parser.add_argument('--disc', type=Path, required=True)
    parser.add_argument('--card', type=Path, required=True, help='A PCSX2 memory card file with a BT2 save (copied).')
    parser.add_argument('--call', type=int, required=True, help='Which AI call (1-based, both sides counted).')
    parser.add_argument('--when', choices=('before', 'after'), required=True)
    parser.add_argument('--site', choices=tuple(SITES), default='input')
    parser.add_argument('--timeout', type=float, default=1800)
    args = parser.parse_args()
    if not 1 <= args.call < 0x8000:
        parser.error('--call is 1 to 32767')
    folder = ce.ROOT / 'work/pcsx2-capture' / f'ai-{args.site}-{args.when}-{args.call}'
    folder.mkdir(parents=True, exist_ok=True)
    executable = args.pcsx2.resolve()
    rva = ce.export_rva(executable.read_bytes(), 'EEmem')
    # A package copy without the user's portable marker, and a profile of its own.
    package = folder / 'package'
    package.mkdir(exist_ok=True)
    for dependency in [executable, *executable.parent.glob('*.dll')]:
        target = package / dependency.name
        if not target.exists() or target.stat().st_size != dependency.stat().st_size:
            shutil.copyfile(dependency, target)
    for name in ('resources', 'QtPlugins'):
        if (executable.parent / name).exists():
            shutil.copytree(executable.parent / name, package / name, dirs_exist_ok=True)
    if (executable.parent / 'qt.conf').exists():
        shutil.copyfile(executable.parent / 'qt.conf', package / 'qt.conf')
    executable = package / executable.name
    profile = folder / 'profile' / 'PCSX2'
    for directory in ('inis', 'bios', 'logs', 'memcards', 'cache'):
        (profile / directory).mkdir(parents=True, exist_ok=True)
    bios = profile / 'bios' / args.bios.name
    shutil.copyfile(args.bios, bios)
    shutil.copyfile(args.card, profile / 'memcards' / 'Mcd001.ps2')
    (profile / 'inis/PCSX2.ini').write_text(
        '[UI]\nSettingsVersion = 1\nSetupWizardIncomplete = false\nStartPaused = false\nPauseOnFocusLoss = false\nConfirmShutdown = false\n'
        '[Folders]\nBios = bios\nLogs = logs\nMemoryCards = memcards\nCache = cache\n'
        f'[Filenames]\nBIOS = {bios.name}\n'
        '[EmuCore]\nEnablePatches = false\nEnableCheats = false\nEnableWideScreenPatches = false\nEnableNoInterlacingPatches = false\n'
        'SaveStateOnShutdown = false\nEnableDiscordPresence = false\n'
        '[EmuCore/GS]\nRenderer = 11\nFrameLimitEnable = false\n'
        '[SPU2/Output]\nOutputModule = nullout\n'
        '[MemoryCards]\nSlot1_Enable = true\nSlot1_Filename = Mcd001.ps2\nSlot2_Enable = false\n', encoding='utf-8')
    original = (ce.ROOT / 'rom/SLUS_214.41').read_bytes()
    elf = folder / 'capture.elf'
    elf.write_bytes(instrument(original, args.call, args.when, args.site))
    command = [str(executable), '-datapath', str(folder / 'profile'), '-nogui', '-fastboot', '-nofullscreen',
               '-logfile', str(folder / 'pcsx2.log'), '-elf', str(elf), str(args.disc.resolve())]
    started = time.monotonic()
    process = subprocess.Popen(command, cwd=folder, creationflags=subprocess.CREATE_NO_WINDOW)
    reader = None
    try:
        while True:
            if time.monotonic() - started > args.timeout:
                raise TimeoutError('The AI call was not reached; see pcsx2.log.')
            if process.poll() is not None:
                raise RuntimeError(f'PCSX2 exited ({process.returncode}); see pcsx2.log.')
            try:
                reader = reader or ce.ProcessReader(process.pid, executable)
                pointer, = struct.unpack('<Q', reader.read(reader.base + rva, 8))
                if pointer:
                    count, marker = struct.unpack('<2I', reader.read(pointer + DATA, 8))
                    if marker == MARKERS[args.when]:
                        break
            except OSError:
                pass
            time.sleep(0.2)
        ram = reader.read(pointer, 32 * 1024 * 1024)
        (folder / 'ee-ram.bin').write_bytes(ram)
        a0, a1, sp, gp, v0 = struct.unpack_from('<5I', ram, DATA + 0x10)
        result = {'site': args.site, 'when': args.when, 'call': args.call, 'calls_counted': count, 'a0': a0, 'a1': a1, 'sp': sp, 'gp': gp,
                  'v0': v0 if args.when == 'after' else None, 'seconds': round(time.monotonic() - started, 1),
                  'ram_sha256': hashlib.sha256(ram).hexdigest(), 'stub_segment': [STUB, 0x400]}
        (folder / 'capture.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
        print(json.dumps(result))
    finally:
        if reader is not None:
            reader.close()
        process.kill()


if __name__ == '__main__':
    main()
