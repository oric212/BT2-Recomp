"""Automated provisional map -> generation -> probe build/run -> integration diagnostics.

Probes execute a real BT2 routine and entry up to a checked unsupported boundary.
"""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reuse-generated", action="store_true", help="Verify provenance and reuse existing generated output.")
    args = parser.parse_args()
    folder = ROOT / "work/bt2-generation"
    folder.mkdir(parents=True, exist_ok=True)
    if not args.reuse_generated:
        run([sys.executable, str(ROOT / "tools/ps2recomp/prepare_map.py")])
        run([sys.executable, str(ROOT / "tools/ps2recomp/generate_game.py"), "--candidate-map", str(folder / "candidates.csv")])
    verify_generated()
    run([sys.executable, str(ROOT / "tools/ps2recomp/prepare_runtime.py")])
    build = ROOT / "build/probe"
    run(["cmake", "-S", str(ROOT), "-B", str(build), "-DBT2_BUILD_GENERATED_PROBE=ON"], folder / "configure.log")
    run(["cmake", "--build", str(build), "--config", "Debug", "--parallel", "1", "--", "/nodeReuse:false"], folder / "probe-build.log")
    run(["ctest", "--test-dir", str(build), "-C", "Debug", "--output-on-failure", "-R", "generated-bt2-probe|bootstrap-runtime"], folder / "probe-test.log")
    run([str(build / "Debug/bt2-generated-probe.exe")])
    result = run(["cmake", "--build", str(build), "--config", "Debug", "--target", "bt2-entry-probe",
                  "--parallel", "1", "--", "/nodeReuse:false"], folder / "integration-build.log", check=False)
    entry_stop = None
    if result.returncode == 0:
        run(["ctest", "--test-dir", str(build), "-C", "Debug", "--output-on-failure", "-R", "^generated-entry-probe$"], folder / "entry-test.log")
        run([str(build / "Debug/bt2-entry-probe.exe"), str(folder / "guest-memory.bin"), str(folder / "entry-stop.json")])
        entry_stop = json.loads((folder / "entry-stop.json").read_text(encoding="utf-8"))
    (folder / "pipeline-result.json").write_text(json.dumps({"probe_build_and_execution": "passed",
        "entry_build_exit_code": result.returncode, "entry_stop": entry_stop, "game_booted": False,
        "integration_log": str(folder / "integration-build.log")}, indent=2) + "\n", encoding="utf-8")
    print(f"Probes verified. Entry build exit code: {result.returncode}; inspect {folder / 'integration-build.log'}.")
    if result.returncode:
        print("Boot integration remains blocked; this diagnostic failure is recorded separately from the passing probe.")
    elif entry_stop:
        run([sys.executable, str(ROOT / "tools/ps2recomp/inspect_blocker.py")])
        print(f"Entry stop: {entry_stop['status']}; game boot remains blocked. See {folder / 'entry-stop.json'}.")


if __name__ == "__main__":
    main()
