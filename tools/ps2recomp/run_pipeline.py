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
    manifest = json.loads(path.read_text(encoding='utf-8'))
    if (manifest['source_elf_sha256'] != LOCAL_CANDIDATE_SHA256
            or not manifest['capture_point'].startswith(f'timer{timer}-mode:')
            or manifest['patch']['address'] != address or manifest['patch']['size'] != 40):
        raise ValueError(f'Timer {timer} capture point/source differs.')
    data = path.with_name('ee-ram.bin').read_bytes()
    if len(data) != 32 * 1024 * 1024 or hashlib.sha256(data).hexdigest() != manifest['capture_sha256']:
        raise ValueError('Timer capture RAM hash/size differs.')
    mode, = struct.unpack_from('<I', data, 0x00387E84)
    marker, = struct.unpack_from('<I', data, 0x00387E80)
    trap = struct.pack('<10I', *manifest['patch']['authored_words'])
    expected_trap = struct.pack('<10I', 0x3C181000, 0x37180000 | register, 0x8F190000, 0x3C180038, 0x37187E80,
                                0xAF190004, 0x34190000 | marker_value, 0xAF190000, 0x08000000 | ((address + 32) >> 2), 0)
    if (trap != expected_trap or mode != manifest[f'observed_timer{timer}_mode'] or mode > 0xFFFF or marker != marker_value
            or data[address:address + 40] != trap):
        raise ValueError('Timer capture payload/instrumentation differs.')
    return mode, manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reuse-generated", action="store_true", help="Verify provenance and reuse existing generated output.")
    parser.add_argument('--kernel-snapshot', type=Path, help='Explicit local snapshot with hash/capture-note sidecar; enables the next startup boundary.')
    parser.add_argument('--timer3-capture', type=Path, help='Explicit capture.json from the timer3-mode capture; replays one observed MMIO read.')
    parser.add_argument('--timer2-capture', type=Path, help='Explicit timer2-mode capture; requires kernel snapshot and Timer 3 capture.')
    args = parser.parse_args()
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
    build = ROOT / "build/probe"
    run(["cmake", "-S", str(ROOT), "-B", str(build), "-DBT2_BUILD_GENERATED_PROBE=ON", "-DBT2_BUILD_KERNEL_PAYLOAD=" + ("ON" if timer_capture else "OFF")], folder / "configure.log")
    run(["cmake", "--build", str(build), "--config", "Debug", "--parallel", "1", "--", "/nodeReuse:false"], folder / "probe-build.log")
    run(["ctest", "--test-dir", str(build), "-C", "Debug", "--output-on-failure", "-R", "generated-bt2-probe|bootstrap-runtime|generated-kernel-handler|generated-buffer-fill|generated-timer-write"], folder / "probe-test.log")
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
        run(entry_command)
        entry_stop = json.loads((folder / "entry-stop.json").read_text(encoding="utf-8"))
    (folder / "pipeline-result.json").write_text(json.dumps({"probe_build_and_execution": "passed",
        "kernel_generation_provenance": kernel_generation,
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
