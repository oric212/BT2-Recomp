"""Generate local BT2 C++ only after a reviewed function map is supplied."""

import argparse
import csv
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
LOCAL_CANDIDATE_SHA256 = "f7e5ba8e74657b8533c3a0fedff45de0d052be3a23fe4c475059ef888a81f03e"


def validate_map(path, metadata):
    with path.open(encoding="utf-8-sig", newline="") as source:
        reader = csv.DictReader(source)
        if reader.fieldnames is None or [name.lower() for name in reader.fieldnames] != ["name", "start", "end", "size"]:
            raise ValueError("Function map must have exactly Name,Start,End,Size columns.")
        raw_rows = list(reader)
        if any(None in row or any(value is None for value in row.values()) for row in raw_rows):
            raise ValueError("Function map has missing or extra CSV fields.")
        rows = [{key.lower(): value for key, value in row.items()} for row in raw_rows]
    if not rows:
        raise ValueError("Function map is empty.")
    starts = set()
    for row in rows:
        start, end, size = (int(row[key], 0) for key in ("start", "end", "size"))
        if start in starts or start % 4 or end % 4 or end <= start or end - start != size:
            raise ValueError("Function map has duplicate starts or invalid contiguous aligned ranges/sizes.")
        starts.add(start)
        if not re.fullmatch(r"FUN_[0-9a-fA-F]{8}", row["name"]) or int(row["name"][4:], 16) != start:
            raise ValueError("Initial BT2 function names must be FUN_<eight-digit-start-address>.")
        if not any(section["flags"] & 4 and section["address"] <= start < end <= section["address"] + section["size"] for section in metadata["sections"]):
            raise ValueError("Function range is outside an executable section.")
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--function-map", required=True, type=Path, help="Reviewed Ghidra CSV with name,start,end,size columns; unknown names must be FUN_<address>.")
    parser.add_argument("--executable", type=Path, default=ROOT / "rom" / "SLUS_214.41")
    args = parser.parse_args()
    try:
        if args.executable.stat().st_size != 2675936:
            raise ValueError("Input size differs from the locally inspected executable candidate.")
        data = args.executable.read_bytes()
        if hashlib.sha256(data).hexdigest() != LOCAL_CANDIDATE_SHA256:
            raise ValueError("Input differs from the locally inspected candidate; inspect and document the new file first.")
        spec = importlib.util.spec_from_file_location("bt2_inspector", ROOT / "tools" / "inspect_elf" / "inspect_elf.py")
        inspector = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(inspector)
        metadata = inspector.inspect(data)
        validate_map(args.function_map, metadata)
        build = ROOT / "build" / "ps2recomp-tools"
        tool = build / "ps2xRecomp" / "Debug" / "ps2_recomp.exe"
        provenance = json.loads((build / "bt2-tool-provenance.json").read_text(encoding="utf-8"))
        if provenance["lock_sha256"] != hashlib.sha256((ROOT / "config" / "ps2recomp.lock.json").read_bytes()).hexdigest() or provenance["recompiler_sha256"] != hashlib.sha256(tool.read_bytes()).hexdigest():
            raise ValueError("Tool provenance does not match the lock file; rebuild with build_tools.py.")
        folder = ROOT / "work" / "bt2-generation"
        folder.mkdir(parents=True, exist_ok=True)
        output = ROOT / "game" / "generated" / "bt2"
        config = folder / "config.toml"
        config.write_text(
            '[general]\n' + '\n'.join(f'{key} = {json.dumps(path.resolve().as_posix())}' for key, path in (
                ("input", args.executable), ("ghidra_output", args.function_map), ("output", output))) +
            '\nsingle_file_output = false\nlow_memory_mode = true\noutput_worker_threads = 1\n'
            'patch_syscalls = false\npatch_cop0 = false\npatch_cache = false\nstubs = []\nskip = []\n', encoding="utf-8")
        subprocess.run([str(tool), str(config)], cwd=folder, check=True)
        print(f"Generated output stays local at {output}. It has not been compiled or executed.")
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Generation stopped: {error}\n")


if __name__ == "__main__":
    main()
