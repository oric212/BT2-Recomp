"""Automated provisional map -> generation -> probe build/run -> integration diagnostics.

Probes execute a real BT2 routine and entry up to a checked unsupported boundary.
"""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import struct
from generate_game import ROOT, LOCAL_CANDIDATE_SHA256


def run(command, log=None, check=True):
    if log is None:
        return subprocess.run(command, cwd=ROOT, check=check)
    with log.open("w", encoding="utf-8") as stream:
        return subprocess.run(command, cwd=ROOT, check=check, stdout=stream, stderr=subprocess.STDOUT)


def verify_generated():
    folder = ROOT / "work/bt2-generation"
    manifest = json.loads((folder / "generation.manifest.json").read_text(encoding="utf-8"))
    if (manifest["executable_sha256"] != hashlib.sha256((ROOT / "rom/SLUS_214.41").read_bytes()).hexdigest()
            or manifest["executable_sha256"] != LOCAL_CANDIDATE_SHA256
            or manifest["map_sha256"] != hashlib.sha256((folder / "candidates.csv").read_bytes()).hexdigest()
            or manifest["lock_sha256"] != hashlib.sha256((ROOT / "config/ps2recomp.lock.json").read_bytes()).hexdigest()
            or manifest["recompiler_sha256"] != hashlib.sha256((ROOT / "build/ps2recomp-tools/ps2xRecomp/Debug/ps2_recomp.exe").read_bytes()).hexdigest()):
        raise ValueError("Generation provenance differs; rerun generation.")
    generated = ROOT / "game/generated/bt2"
    actual = {path.name for path in generated.iterdir() if path.is_file() and path.suffix in (".cpp", ".h")}
    if actual != set(manifest["files"]):
        raise ValueError("Generated source file set changed.")
    for name, expected in manifest["files"].items():
        if Path(name).name != name or hashlib.sha256((generated / name).read_bytes()).hexdigest() != expected:
            raise ValueError(f"Generated source changed: {name}")


def verify_snapshot(path):
    data = path.read_bytes()
    manifest = json.loads(path.with_suffix('.json').read_text(encoding='utf-8'))
    if len(data) != 0x80004 or hashlib.sha256(data).hexdigest() != manifest['snapshot_sha256'] or not manifest['capture_note']:
        raise ValueError('Kernel snapshot size/hash/capture provenance differs.')
    return manifest


def verify_timer_capture(path, timer=3):
    if timer not in (2, 3):
        raise ValueError("Unsupported timer capture.")
    address, marker_value, register = (0x00338A1C, 0xB703, 0x1810) if timer == 3 else (0x00338C58, 0xB704, 0x1010)
    return verify_word_capture(path, f'timer{timer}-mode', f'observed_timer{timer}_mode', address, marker_value, register)


def verify_intc_capture(path):
    return verify_word_capture(path, 'intc-mask', 'observed_intc_mask', 0x0032E9A4, 0xB705, 0xF010)


def verify_timer2_readback_capture(path):
    return verify_word_capture(path, 'timer2-readback', 'observed_timer2_readback', 0x00338DD8, 0xB706, 0x1010)


def verify_word_capture(path, point, field, address, marker_value, register, max_word=0xFFFF):
    manifest = json.loads(path.read_text(encoding='utf-8'))
    if (manifest['source_elf_sha256'] != LOCAL_CANDIDATE_SHA256
            or not manifest['capture_point'].startswith(point + ':')
            or manifest['patch']['address'] != address or manifest['patch']['size'] != 40):
        raise ValueError(f'{point} capture point/source differs.')
    data = path.with_name('ee-ram.bin').read_bytes()
    if len(data) != 32 * 1024 * 1024 or hashlib.sha256(data).hexdigest() != manifest['capture_sha256']:
        raise ValueError('Timer capture RAM hash/size differs.')
    mode, = struct.unpack_from('<I', data, 0x00387E84)
    marker, = struct.unpack_from('<I', data, 0x00387E80)
    trap = struct.pack('<10I', *manifest['patch']['authored_words'])
    expected_trap = struct.pack('<10I', 0x3C181000, 0x37180000 | register, 0x8F190000, 0x3C180038, 0x37187E80,
                                0xAF190004, 0x34190000 | marker_value, 0xAF190000, 0x08000000 | ((address + 32) >> 2), 0)
    if (trap != expected_trap or mode != manifest[field] or mode > max_word or marker != marker_value
            or data[address:address + 40] != trap):
        raise ValueError('Timer capture payload/instrumentation differs.')
    return mode, manifest


def verify_dma_capture(path, channel=False):
    point, address, marker, register, field = (
        ('dma5-control', 0x003319EC, 0xB70A, 0xC000, 'observed_dma5_control') if channel else
        ('dma-status', 0x003319CC, 0xB709, 0xE010, 'observed_dma_status'))
    return verify_word_capture(path, point, field, address, marker, register, max_word=0xFFFFFFFF)


def verify_sif_capture(path, rpc=False):
    manifest = json.loads(path.read_text(encoding='utf-8'))
    data = path.with_name('ee-ram.bin').read_bytes()
    address, marker, point = (0x332110, 0xB70E, 'sif-rpc-state') if rpc else (0x331A34, 0xB70B, 'sif-command')
    words = [0x3C180038, 0x37187E80, 0xAF020004, 0xAF040008,
             0x34190000 | marker, 0xAF190000, 0x08000000 | ((address + 24) >> 2), 0]
    trap = struct.pack('<8I', *words)
    patch = manifest['patch']
    if (manifest['source_elf_sha256'] != LOCAL_CANDIDATE_SHA256
            or not manifest['capture_point'].startswith(point + ':')
            or patch['address'] != address or patch['size'] != len(trap)
            or patch['authored_words'] != words or patch['marker_address'] != 0x387E80
            or patch['marker_value'] != marker or len(data) != 32 * 1024 * 1024
            or hashlib.sha256(data).hexdigest() != manifest['capture_sha256']
            or data[address:address + len(trap)] != trap):
        raise ValueError('SIF capture provenance/instrumentation differs.')
    actual_marker, word, selector = struct.unpack_from('<III', data, 0x387E80)
    if (actual_marker != marker or word != manifest['observed_sif_rpc_state' if rpc else 'observed_sif_command']
            or selector != manifest['observed_sif_rpc_selector' if rpc else 'observed_sif_selector'] or selector != (0x80000002 if rpc else 0x80000000)):
        raise ValueError('SIF capture payload/selector differs.')
    return word, manifest


def verify_sif_dma_pair(before_path, after_path):
    captures = []
    for path, point, address, marker in (
            (before_path, 'sif-dma-before', 0x331D44, 0xB70C),
            (after_path, 'sif-dma-after', 0x331D4C, 0xB70D)):
        m = json.loads(path.read_text(encoding='utf-8'))
        ee = path.with_name('ee-ram.bin').read_bytes()
        iop = path.with_name('iop-ram.bin').read_bytes()
        words = [0x3C180038, 0x37187E80, 0xAF020004, 0xAF1D0008,
                 0x34190000 | marker, 0xAF190000, 0x08000000 | ((address + 24) >> 2), 0]
        patch = m['patch']
        if (m['source_elf_sha256'] != LOCAL_CANDIDATE_SHA256
                or not m['capture_point'].startswith(point + ':')
                or patch['address'] != address or patch['size'] != 32
                or patch['authored_words'] != words or patch['marker_address'] != 0x387E80
                or patch['marker_value'] != marker or len(ee) != 32 * 1024 * 1024
                or len(iop) != 2 * 1024 * 1024
                or hashlib.sha256(ee).hexdigest() != m['capture_sha256']
                or hashlib.sha256(iop).hexdigest() != m['iop_capture_sha256']
                or ee[address:address + 32] != struct.pack('<8I', *words)):
            raise ValueError('SIF DMA capture provenance/instrumentation differs.')
        actual_marker, result, sp = struct.unpack_from('<III', ee, 0x387E80)
        if (actual_marker != marker or result != m['observed_sif_dma_result']
                or sp != m['observed_sif_dma_sp'] or sp != 0x1FFEC70
                or struct.unpack_from('<4I', ee, sp) != (0x3A9580, 0x19600, 20, 0x44)
                or struct.unpack_from('<5I', ee, 0x3A9580) != (20, 0, 0x80000000, 0, 0x3A94C0)
                or struct.unpack_from('<2I', iop, 0x196C0) != (0x19600, 0x19680)):
            raise ValueError('SIF DMA descriptor/packet/table differs.')
        captures.append((m, ee, iop, result))
    before, after = captures
    if (before[0]['pcsx2_sha256'] != after[0]['pcsx2_sha256']
            or before[0]['bios_sha256'] != after[0]['bios_sha256']
            or after[3] != 0x701
            or struct.unpack_from('<I', before[2], 0x196C8)[0] != 0x93640
            or struct.unpack_from('<I', after[2], 0x196C8)[0] != 0x3A94C0
            or after[2][0x19600:0x19614] != bytes(4) + after[1][0x3A9584:0x3A9594]):
        raise ValueError('SIF DMA receive effects/return differ.')
    return before[2] + struct.pack('<I', after[3]), {'before': before[0], 'after': after[0]}


def verify_osd_capture(path):
    manifest = json.loads(path.read_text(encoding='utf-8'))
    data = path.with_name('ee-ram.bin').read_bytes()
    address, marker = 0x003385F4, 0xB707
    words = [0x8FB90000, 0x3C180038, 0x37187E80, 0xAF190004,
             0x34190000 | marker, 0xAF190000, 0x08000000 | ((address + 24) >> 2), 0]
    trap = struct.pack('<8I', *words)
    patch = manifest['patch']
    if (manifest['source_elf_sha256'] != LOCAL_CANDIDATE_SHA256
            or not manifest['capture_point'].startswith('osd-config:')
            or patch['address'] != address or patch['size'] != len(trap)
            or patch['authored_words'] != words
            or patch['marker_address'] != 0x387E80 or patch['marker_value'] != marker
            or len(data) != 32 * 1024 * 1024
            or hashlib.sha256(data).hexdigest() != manifest['capture_sha256']
            or data[address:address + len(trap)] != trap):
        raise ValueError('OSD capture provenance/instrumentation differs.')
    actual_marker, word = struct.unpack_from('<II', data, 0x387E80)
    if actual_marker != marker or word != manifest['observed_osd_config']:
        raise ValueError('OSD capture payload differs.')
    return word, manifest


def verify_osd_readback_capture(path):
    manifest = json.loads(path.read_text(encoding='utf-8'))
    data = path.with_name('ee-ram.bin').read_bytes()
    address, marker = 0x00338620, 0xB708
    words = [0x3C180038, 0x37187E80, 0x8FB90004, 0xAF190004,
             0x8FB90000, 0xAF190008, 0x34190000 | marker, 0xAF190000,
             0x08000000 | ((address + 32) >> 2), 0]
    trap = struct.pack('<10I', *words)
    patch = manifest['patch']
    if (manifest['source_elf_sha256'] != LOCAL_CANDIDATE_SHA256
            or not manifest['capture_point'].startswith('osd-readback:')
            or patch['address'] != address or patch['size'] != len(trap)
            or patch['authored_words'] != words
            or patch['marker_address'] != 0x387E80 or patch['marker_value'] != marker
            or len(data) != 32 * 1024 * 1024
            or hashlib.sha256(data).hexdigest() != manifest['capture_sha256']
            or data[address:address + len(trap)] != trap):
        raise ValueError('OSD readback provenance/instrumentation differs.')
    actual_marker, readback, original = struct.unpack_from('<III', data, 0x387E80)
    if (actual_marker != marker or readback != manifest['observed_osd_readback']
            or original != manifest['original_osd_config']
            or readback != original or ((original & 0xFFFF1FFF) | 0x2000) != original):
        raise ValueError('OSD readback does not validate the observed same-value write.')
    return readback, manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reuse-generated", action="store_true", help="Verify provenance and reuse existing generated output.")
    parser.add_argument('--kernel-snapshot', type=Path, help='Explicit local snapshot with hash/capture-note sidecar; enables the next startup boundary.')
    parser.add_argument('--timer3-capture', type=Path, help='Explicit capture.json from the timer3-mode capture; replays one observed MMIO read.')
    parser.add_argument('--timer2-capture', type=Path, help='Explicit timer2-mode capture; requires kernel snapshot and Timer 3 capture.')
    parser.add_argument('--intc-capture', type=Path, help='Explicit intc-mask checkpoint; requires Timer 2 capture.')
    parser.add_argument('--timer2-readback-capture', type=Path, help='Explicit validation of the post-write mode read; requires INTC checkpoint.')
    parser.add_argument('--osd-capture', type=Path, help='First OSD config output checkpoint; requires Timer 2 readback.')
    parser.add_argument('--osd-readback-capture', type=Path, help='Validate first same-value OSD setter/readback; requires OSD capture.')
    parser.add_argument('--dma-status-capture', type=Path, help='Explicit first D_STAT checkpoint; requires OSD readback.')
    parser.add_argument('--dma5-control-capture', type=Path, help='Explicit first channel 5 CHCR checkpoint; requires D_STAT.')
    parser.add_argument('--sif-command-capture', type=Path, help='Explicit first SifGetReg return checkpoint; requires channel 5 capture.')
    parser.add_argument('--sif-dma-before-capture', type=Path, help='First SIF submission EE/IOP capture; requires matching after capture.')
    parser.add_argument('--sif-dma-after-capture', type=Path, help='First SIF return/receive capture; requires first-query capture.')
    parser.add_argument('--sif-rpc-state-capture', type=Path, help='Explicit reached RPCINIT query result; requires first DMA receive captures.')
    args = parser.parse_args()
    if args.sif_rpc_state_capture and not args.sif_dma_after_capture:
        parser.error('--sif-rpc-state-capture requires --sif-dma-after-capture')
    sif_rpc = verify_sif_capture(args.sif_rpc_state_capture, rpc=True) if args.sif_rpc_state_capture else None
    if bool(args.sif_dma_before_capture) != bool(args.sif_dma_after_capture) or (args.sif_dma_after_capture and not args.sif_command_capture):
        parser.error('SIF DMA captures require both before/after and --sif-command-capture')
    sif_dma = verify_sif_dma_pair(args.sif_dma_before_capture, args.sif_dma_after_capture) if args.sif_dma_after_capture else None
    if sif_dma and sif_rpc and any(sif_dma[1]['after'][key] != sif_rpc[1][key] for key in ('pcsx2_sha256', 'bios_sha256')):
        raise ValueError('SIF RPC query and DMA reference environment differ.')
    if args.sif_command_capture and not args.dma5_control_capture:
        parser.error('--sif-command-capture requires --dma5-control-capture')
    sif_command = verify_sif_capture(args.sif_command_capture) if args.sif_command_capture else None
    if args.dma_status_capture and not args.osd_readback_capture:
        parser.error('--dma-status-capture requires --osd-readback-capture')
    if args.dma5_control_capture and not args.dma_status_capture:
        parser.error('--dma5-control-capture requires --dma-status-capture')
    dma_status = verify_dma_capture(args.dma_status_capture) if args.dma_status_capture else None
    dma5_control = verify_dma_capture(args.dma5_control_capture, channel=True) if args.dma5_control_capture else None
    if args.osd_readback_capture and not args.osd_capture:
        parser.error('--osd-readback-capture requires --osd-capture')
    osd_readback = verify_osd_readback_capture(args.osd_readback_capture) if args.osd_readback_capture else None
    if args.osd_capture and not args.timer2_readback_capture:
        parser.error('--osd-capture requires --timer2-readback-capture')
    osd_capture = verify_osd_capture(args.osd_capture) if args.osd_capture else None
    if osd_readback and osd_capture and osd_readback[0] != osd_capture[0]:
        parser.error('OSD captures differ in their original configuration')
    if args.timer2_readback_capture and not args.intc_capture:
        parser.error('--timer2-readback-capture requires --intc-capture')
    readback_capture = verify_timer2_readback_capture(args.timer2_readback_capture) if args.timer2_readback_capture else None
    if args.intc_capture and not args.timer2_capture:
        parser.error('--intc-capture requires --timer2-capture')
    intc_capture = verify_intc_capture(args.intc_capture) if args.intc_capture else None
    if args.timer2_capture and not args.timer3_capture:
        parser.error('--timer2-capture requires --timer3-capture')
    timer2_capture = verify_timer_capture(args.timer2_capture, 2) if args.timer2_capture else None
    if args.timer3_capture and not args.kernel_snapshot:
        parser.error('--timer3-capture requires --kernel-snapshot')
    timer_capture = verify_timer_capture(args.timer3_capture) if args.timer3_capture else None
    snapshot_provenance = verify_snapshot(args.kernel_snapshot) if args.kernel_snapshot else None
    folder = ROOT / "work/bt2-generation"
    folder.mkdir(parents=True, exist_ok=True)
    if not args.reuse_generated:
        run([sys.executable, str(ROOT / "tools/ps2recomp/prepare_map.py")])
        run([sys.executable, str(ROOT / "tools/ps2recomp/generate_game.py"), "--candidate-map", str(folder / "candidates.csv")])
    verify_generated()
    run([sys.executable, str(ROOT / "tools/ps2recomp/prepare_runtime.py")])
    kernel_generation = None
    if timer_capture:
        run([sys.executable, str(ROOT / "tools/ps2recomp/generate_kernel.py")])
        kernel_generation = json.loads((ROOT / "work/kernel-generation/generation.manifest.json").read_text(encoding="utf-8"))
    startup_kernel_generation = None
    if osd_readback:
        run([sys.executable, str(ROOT / "tools/ps2recomp/generate_kernel.py"), "--payload", "startup"])
        startup_kernel_generation = json.loads((ROOT / "work/kernel-generation-startup/generation.manifest.json").read_text(encoding="utf-8"))
    build = ROOT / "build/probe"
    run(["cmake", "-S", str(ROOT), "-B", str(build), "-DBT2_BUILD_GENERATED_PROBE=ON", "-DBT2_BUILD_KERNEL_PAYLOAD=" + ("ON" if timer_capture else "OFF"), "-DBT2_BUILD_STARTUP_KERNEL_PAYLOAD=" + ("ON" if osd_readback else "OFF")], folder / "configure.log")
    run(["cmake", "--build", str(build), "--config", "Debug", "--parallel", "1", "--", "/nodeReuse:false"], folder / "probe-build.log")
    run(["ctest", "--test-dir", str(build), "-C", "Debug", "--output-on-failure", "-R", "generated-bt2-probe|bootstrap-runtime|generated-kernel-handler|generated-buffer-fill|generated-timer-write|generated-copy-handler"], folder / "probe-test.log")
    run([str(build / "Debug/bt2-generated-probe.exe")])
    result = run(["cmake", "--build", str(build), "--config", "Debug", "--target", "bt2-entry-probe",
                  "--parallel", "1", "--", "/nodeReuse:false"], folder / "integration-build.log", check=False)
    entry_stop = None
    if result.returncode == 0:
        run(["ctest", "--test-dir", str(build), "-C", "Debug", "--output-on-failure", "-R", "^generated-entry-probe$"], folder / "entry-test.log")
        entry_command = [str(build / "Debug/bt2-entry-probe.exe"), str(folder / "guest-memory.bin"), str(folder / "entry-stop.json")]
        if args.kernel_snapshot:
            entry_command.append(str(args.kernel_snapshot.resolve()))
        if timer_capture:
            timer_word = folder / 'timer3-mode.bin'
            timer_word.write_bytes(struct.pack('<I', timer_capture[0]))
            entry_command.append(str(timer_word))
        if timer2_capture:
            timer2_word = folder / 'timer2-mode.bin'
            timer2_word.write_bytes(struct.pack('<I', timer2_capture[0]))
            entry_command.append(str(timer2_word))
        if intc_capture:
            intc_word = folder / 'intc-mask.bin'
            intc_word.write_bytes(struct.pack('<I', intc_capture[0]))
            entry_command.append(str(intc_word))
        if readback_capture:
            readback_word = folder / 'timer2-readback.bin'
            readback_word.write_bytes(struct.pack('<I', readback_capture[0]))
            entry_command.append(str(readback_word))
        if osd_capture:
            osd_word = folder / 'osd-config.bin'
            osd_word.write_bytes(struct.pack('<I', osd_capture[0]))
            entry_command.append(str(osd_word))
        if osd_readback:
            osd_readback_word = folder / 'osd-readback.bin'
            osd_readback_word.write_bytes(struct.pack('<I', osd_readback[0]))
            entry_command.append(str(osd_readback_word))
        for checkpoint, name in ((dma_status, 'dma-status'), (dma5_control, 'dma5-control'), (sif_command, 'sif-command')):
            if checkpoint:
                word_path = folder / (name + '.bin')
                word_path.write_bytes(struct.pack('<I', checkpoint[0]))
                entry_command.append(str(word_path))
        if sif_dma:
            bundle = folder / 'sif-dma.bin'
            bundle.write_bytes(sif_dma[0])
            entry_command.append(str(bundle))
        if sif_rpc:
            word_path = folder / 'sif-rpc-state.bin'
            word_path.write_bytes(struct.pack('<I', sif_rpc[0]))
            entry_command.append(str(word_path))
        run(entry_command)
        entry_stop = json.loads((folder / "entry-stop.json").read_text(encoding="utf-8"))
    (folder / "pipeline-result.json").write_text(json.dumps({"probe_build_and_execution": "passed",
        "sif_rpc_provenance": sif_rpc[1] if sif_rpc else None,
        "sif_dma_provenance": sif_dma[1] if sif_dma else None,
        "sif_command_provenance": sif_command[1] if sif_command else None,
        "dma_status_provenance": dma_status[1] if dma_status else None,
        "dma5_control_provenance": dma5_control[1] if dma5_control else None,
        "osd_readback_provenance": osd_readback[1] if osd_readback else None,
        "osd_capture_provenance": osd_capture[1] if osd_capture else None,
        "startup_kernel_generation_provenance": startup_kernel_generation,
        "kernel_generation_provenance": kernel_generation,
        "timer2_readback_capture_provenance": readback_capture[1] if readback_capture else None,
        "intc_capture_provenance": intc_capture[1] if intc_capture else None,
        "timer2_capture_provenance": timer2_capture[1] if timer2_capture else None,
        "timer3_capture_provenance": timer_capture[1] if timer_capture else None, "kernel_snapshot_provenance": snapshot_provenance, "entry_build_exit_code": result.returncode, "entry_stop": entry_stop, "game_booted": False,
        "integration_log": str(folder / "integration-build.log")}, indent=2) + "\n", encoding="utf-8")
    print(f"Probes verified. Entry build exit code: {result.returncode}; inspect {folder / 'integration-build.log'}.")
    if result.returncode:
        print("Boot integration remains blocked; this diagnostic failure is recorded separately from the passing probe.")
    elif entry_stop:
        run([sys.executable, str(ROOT / "tools/ps2recomp/inspect_blocker.py")])
        print(f"Entry stop: {entry_stop['status']}; game boot remains blocked. See {folder / 'entry-stop.json'}.")


if __name__ == "__main__":
    main()
