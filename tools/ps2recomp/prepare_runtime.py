"""Prepare local runtime sources and a bounded guest image from validated generation.

Route emitted direct tail calls through the runtime so missing guest code is a
checked boundary, not an unresolved native symbol or an invented function stub.
"""

import hashlib
import importlib.util
import json
import re
from run_pipeline import ROOT, verify_generated


def route_tail_calls(source):
    pattern = r"\bFUN_[0-9A-Fa-f]{8}_0x([0-9A-Fa-f]+)\(rdram, ctx, runtime\); return;"
    return re.subn(pattern, lambda match:
        f'(void)runtime->dispatchGuestBranch(rdram, ctx, 0x{match[1]}u, ctx->branch_pc, 0u, '
        'PS2Runtime::GuestBranchKind::TailJump, "generated tail call"); return;', source)


def build_memory(data, metadata, size=32 * 1024 * 1024):
    memory = bytearray(size)
    occupied = []
    for load in metadata["load_segments"]:
        address, length = load["virtual_address"], load["memory_size"]
        if not length:
            continue
        if address < 0 or address + length > size or load["file_size"] > length:
            raise ValueError("ELF load exceeds bounded guest RAM.")
        if any(address < end and start < address + length for start, end in occupied):
            raise ValueError("Overlapping ELF loads need explicit policy.")
        occupied.append((address, address + length))
        offset, file_size = load["file_offset"], load["file_size"]
        if offset < 0 or offset + file_size > len(data):
            raise ValueError("ELF load payload is out of file bounds.")
        memory[address:address + file_size] = data[offset:offset + file_size]
    return memory


def main():
    verify_generated()
    folder = ROOT / "work/bt2-generation"
    patched = ROOT / "game/generated/bt2-runtime"
    patched.mkdir(parents=True, exist_ok=True)
    prepared_sources = []
    tail_sources = {
        "FUN_00100008_0x100008.cpp": 1,
        "FUN_00338520_0x338520.cpp": 1,
        "FUN_0033A238_0x33a238.cpp": 1,
        "FUN_00321E50_0x321e50.cpp": 1,
        "FUN_00321D90_0x321d90.cpp": 1,
        "FUN_00331FE8_0x331fe8.cpp": 2,
        "FUN_00331888_0x331888.cpp": 3,
    }
    for filename, expected_count in tail_sources.items():
        source = ROOT / "game/generated/bt2" / filename
        text, count = route_tail_calls(source.read_text(encoding="utf-8"))
        if count != expected_count:
            raise ValueError(f"Unexpected emitted tail call count in {filename}; review changed output.")
        destination = patched / source.name
        destination.write_text(text, encoding="utf-8")
        prepared_sources.append({"source": filename,
            "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            "prepared_sha256": hashlib.sha256(destination.read_bytes()).hexdigest(),
            "tail_dispatch_hooks": count})
    spec = importlib.util.spec_from_file_location("bt2_elf", ROOT / "tools/inspect_elf/inspect_elf.py")
    inspector = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(inspector)
    data = (ROOT / "rom/SLUS_214.41").read_bytes()
    memory = build_memory(data, inspector.inspect(data))
    image = folder / "guest-memory.bin"
    image.write_bytes(memory)
    (folder / "runtime-preparation.json").write_text(json.dumps({
        "prepared_sources": prepared_sources, "memory_size": len(memory),
        "memory_sha256": hashlib.sha256(memory).hexdigest(), "scope": "ELF PT_LOAD image, main RAM only; no BIOS/MMIO/TLB/IOP"}, indent=2) + "\n", encoding="utf-8")
    print("Prepared entry dispatch hook and bounded ELF memory image; originals unchanged.")


if __name__ == "__main__":
    main()
