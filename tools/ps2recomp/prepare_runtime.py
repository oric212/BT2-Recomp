"""Prepare local runtime sources and a bounded guest image from validated generation.

Route emitted direct tail calls through the runtime so missing guest code is a
checked boundary, not an unresolved native symbol or an invented function stub.
"""

import hashlib
import importlib.util
import json
import re
from run_pipeline import ROOT, verify_generated


def write_prepared(path,text):
    if not path.exists() or path.read_text(encoding='utf-8')!=text:
        path.write_text(text,encoding='utf-8')


def route_tail_calls(source):
    pattern = r"\bFUN_[0-9A-Fa-f]{8}_0x([0-9A-Fa-f]+)\(rdram, ctx, runtime\); return;"
    return re.subn(pattern, lambda match:
        f'(void)runtime->dispatchGuestBranch(rdram, ctx, 0x{match[1]}u, ctx->branch_pc, 0u, '
        'PS2Runtime::GuestBranchKind::TailJump, "generated tail call"); return;', source)


def route_vu_status(source):
    pattern=r"SET_GPR_U32\(ctx, (\d+), ctx->vu0_vpu_stat\);"
    text,count=re.subn(pattern,lambda m:f'SET_GPR_U32(ctx, {m[1]}, runtime->readVuStatus(ctx));',source)
    if 'ctx->vu0_vpu_stat' in text:raise ValueError('Unrecognized generated VU status access')
    return text,count

def route_vu_control(source):
    pattern=r"ctx->vu0_fbrst = GPR_U32\(ctx, (\d+)\) & 0x00000C0Cu;"
    text,count=re.subn(pattern,lambda m:f'runtime->writeVuFbrst(ctx, GPR_U32(ctx, {m[1]}));',source)
    if 'ctx->vu0_fbrst =' in text:raise ValueError('Unrecognized generated VU control write')
    return text,count


def route_data_cache(source):
    pattern = (r'(// 0x[0-9a-f]+: 0x[0-9a-f]+  cache       0x18, 0x([0-9A-Fa-f]+)\(\$t0\)'
               r'\s+ctx->pc = 0x[0-9A-Fa-f]+u;\s+)// CACHE instruction \(ignored\)')
    return re.subn(pattern, lambda match: match[1] +
        f'runtime->dataCacheLine(rdram, ADD32(GPR_U32(ctx, 8), 0x{match[2]}u));', source)


def route_indexed_cache(source):
    pattern=(r'(// 0x[0-9a-f]+: 0x[0-9a-f]+  cache       0x(10|14|16), 0x([01])\(\$a2\)'
             r'\s+ctx->pc = 0x[0-9A-Fa-f]+u;\s+)// CACHE instruction \(ignored\)')
    text,count=re.subn(pattern,lambda m:m[1]+
        f'runtime->indexedDataCache(rdram, ctx, 0x{m[2]}u, ADD32(GPR_U32(ctx, 6), 0x{m[3]}u));',source)
    text,reads=re.subn(r'SET_GPR_S32\(ctx, (\d+), \(int32_t\)ctx->cop0_taglo\);',
        lambda m:f'SET_GPR_S32(ctx, {m[1]}, (int32_t)runtime->readCacheTagLo(ctx));',text)
    if 'ctx->cop0_taglo' in text:raise ValueError('Unrecognized generated TagLo access')
    if count or reads:
        if 'CACHE instruction (ignored)' in text:raise ValueError('Unsupported CACHE mixed with indexed TagLo')
    return text,count,reads


def route_fast_halfword_reads(source):
    # Generated constant-address loads retain the checked owned-RAM read path.
    text,count=re.subn(r'\bFAST_READ16\((0x[0-9A-Fa-f]+u)\)',r'READ16(\1)',source)
    if 'FAST_READ16(' in text:raise ValueError('Unrecognized generated fast halfword read')
    return text,count


def route_fast_byte_reads(source):
    # Keep the generated LB/LBU cast; only route its literal address through checked RAM.
    text,count=re.subn(r'\bFAST_READ8\((0x[0-9A-Fa-f]+u)\)',r'READ8(\1)',source)
    if 'FAST_READ8(' in text:raise ValueError('Unrecognized generated fast byte read')
    return text,count


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
        write_prepared(destination,text)
        prepared_sources.append({"source": filename,
            "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            "prepared_sha256": hashlib.sha256(destination.read_bytes()).hexdigest(),
            "tail_dispatch_hooks": count})
    # Every actually selected extra entry gets the same deterministic tail routing.
    # Preserve raw generation and record hashes; no handwritten per-function stub list.
    from expand_startup import verify_extra
    for address in verify_extra():
        source=ROOT/'game/generated/bt2'/f'FUN_{address:08X}_0x{address:x}.cpp'
        text,count=route_tail_calls(source.read_text(encoding='utf-8'))
        text,vu_count=route_vu_status(text)
        text,control_count=route_vu_control(text)
        text,cache_count,tag_count=route_indexed_cache(text)
        if address in (0x32F4B0,0x32F5F0) and (cache_count,tag_count)!=(4,2):
            raise ValueError('Reached indexed cache helper shape/count differs')
        text,halfword_count=route_fast_halfword_reads(text)
        text,byte_count=route_fast_byte_reads(text)
        destination=patched/source.name
        write_prepared(destination,text)
        prepared_sources.append({'source':source.name,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
                                 'prepared_sha256':hashlib.sha256(destination.read_bytes()).hexdigest(),'tail_dispatch_hooks':count,'vu_status_hooks':vu_count,'vu_control_hooks':control_count,'indexed_cache_hooks':cache_count,'taglo_hooks':tag_count,'checked_halfword_hooks':halfword_count,'checked_byte_hooks':byte_count})
    source = ROOT / 'game/generated/bt2/FUN_00331F38_0x331f38.cpp'
    text, count = route_data_cache(source.read_text(encoding='utf-8'))
    if count != 9 or 'CACHE instruction (ignored)' in text:
        raise ValueError('Unexpected generated data cache instruction shape/count.')
    destination = patched / source.name
    destination.write_text(text, encoding='utf-8')
    prepared_sources.append({'source': source.name,
        'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
        'prepared_sha256': hashlib.sha256(destination.read_bytes()).hexdigest(),
        'data_cache_hooks': count})
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
