"""Generate only the observed copied-kernel entry; all derived output stays ignored."""
import hashlib
import json
import re
import struct
import subprocess
import uuid
from generate_game import ROOT, LOCAL_CANDIDATE_SHA256
from run_pipeline import verify_generated

SOURCE, TARGET, SIZE = 0x0036B328, 0x80076000, 0x740


def extract_payload(data, source=SOURCE, size=SIZE):
    if len(data) < 52 or data[:7] != b'\x7fELF\x01\x01\x01' or size <= 0:
        raise ValueError('Invalid ELF/payload size')
    table = struct.unpack_from('<I', data, 28)[0]
    stride, count = struct.unpack_from('<HH', data, 42)
    if stride != 32 or table + count * stride > len(data):
        raise ValueError('Invalid load table')
    matches = []
    for index in range(count):
        kind, offset, address, _, length, _, _, _ = struct.unpack_from('<8I', data, table + index * stride)
        if kind == 1 and address <= source and source + size <= address + length:
            begin = offset + source - address
            if begin + size > len(data):
                raise ValueError('Payload exceeds file')
            matches.append((begin, data[begin:begin + size]))
    if len(matches) != 1:
        raise ValueError('Payload must have exactly one file-backed load')
    return matches[0]


def package_elf(payload, target, code_size):
    if not payload or len(payload) % 4 or target % 4 or code_size % 4 or not 0 < code_size <= len(payload) or target + len(payload) > 2**32:
        raise ValueError('Invalid payload mapping')
    names = b'\0.text\0.shstrtab\0.data\0'
    text_offset = 256
    names_offset = text_offset + len(payload)
    result = bytearray(names_offset + len(names))
    result[text_offset:names_offset] = payload
    result[names_offset:] = names
    struct.pack_into('<16sHHIIIIIHHHHHH', result, 0,
                     b'\x7fELF\x01\x01\x01' + bytes(9), 2, 8, 1, target, 52, 96, 0, 52, 32, 1, 40, 4, 2)
    struct.pack_into('<8I', result, 52, 1, text_offset, target, target, len(payload), len(payload), 5, 16)
    struct.pack_into('<10I', result, 136, 1, 1, 6, target, text_offset, code_size, 0, 0, 4, 0)
    struct.pack_into('<10I', result, 176, 7, 3, 0, 0, names_offset, len(names), 0, 0, 1, 0)
    struct.pack_into('<10I', result, 216, 17, 1, 2, target + code_size, text_offset + code_size, len(payload) - code_size, 0, 0, 4, 0)
    return bytes(result)


def generate(folder, output, executable, tool, function_map=None):
    output.mkdir(parents=True)
    config = folder / 'config.toml'
    config.write_text('[general]\n' + '\n'.join(f'{key} = {json.dumps(path.resolve().as_posix())}'
        for key, path in [('input', executable), ('output', output)] + ([('ghidra_output', function_map)] if function_map else [])) +
        '\nentry_points = ["0x80076000"]\nsingle_file_output = false\nlow_memory_mode = true\noutput_worker_threads = 1\n'
        'patch_syscalls = false\npatch_cop0 = false\npatch_cache = false\nstubs = []\nskip = []\n', encoding='utf-8')
    with (folder / 'generation.log').open('w', encoding='utf-8') as log:
        subprocess.run([str(tool), str(config)], cwd=folder, stdout=log, stderr=subprocess.STDOUT, check=True)


def main():
    verify_generated()
    data = (ROOT / 'rom/SLUS_214.41').read_bytes()
    if hashlib.sha256(data).hexdigest() != LOCAL_CANDIDATE_SHA256:
        raise ValueError('Retail source provenance changed')
    offset, payload = extract_payload(data)
    tool = ROOT / 'build/ps2recomp-tools/ps2xRecomp/Debug/ps2_recomp.exe'
    folder = ROOT / 'work/kernel-generation'
    folder.mkdir(exist_ok=True)
    discovery = folder / ('discovery-' + uuid.uuid4().hex)
    discovery.mkdir()
    executable = discovery / 'payload.elf'
    executable.write_bytes(package_elf(payload, TARGET, SIZE))
    generate(discovery, discovery / 'generated', executable, tool)
    entry = discovery / 'generated/sub_80076000_0x80076000.cpp'
    match = re.search(r'// Address: 0x80076000 - 0x([0-9a-fA-F]+)', entry.read_text(encoding='utf-8'))
    if not match:
        raise ValueError('Observed kernel entry was not discovered')
    end = int(match[1], 16)
    if not TARGET < end <= TARGET + SIZE:
        raise ValueError('Provisional entry exceeds copied payload')
    executable = folder / 'payload.elf'
    executable.write_bytes(package_elf(payload, TARGET, end - TARGET))
    function_map = folder / 'functions.csv'
    function_map.write_text(f'Name,Start,End,Size\nFUN_{TARGET:08X},0x{TARGET:08X},0x{end:08X},{end-TARGET}\n', encoding='utf-8')
    output = ROOT / 'game/generated/bt2-kernel'
    if output.exists():
        if output.is_symlink() or output.resolve().parent != (ROOT / 'game/generated').resolve():
            raise ValueError('Unexpected kernel output path')
        output.rename(folder / ('previous-output-' + uuid.uuid4().hex))
    generate(folder, output, executable, tool, function_map)
    log = (folder / 'generation.log').read_text(encoding='utf-8')
    if not re.search(r'Warnings: \d+, errors: 0', log) or 'decode failures: 0' not in log:
        raise ValueError('Scoped kernel generation has errors; inspect log')
    # Isolate generated headers from the main ELF headers, preserving raw originals.
    prepared = ROOT / 'game/generated/bt2-kernel-runtime'
    prepared.mkdir(exist_ok=True)
    files = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in output.iterdir() if p.suffix in ('.cpp', '.h')}
    prepared_files = {}
    for name in ('FUN_80076000_0x80076000.cpp', 'ps2_recompiled_functions.h', 'ps2_recompiled_stubs.h'):
        text = (output / name).read_text(encoding='utf-8').replace('ps2_recompiled_functions.h', 'bt2_kernel_functions.h').replace('ps2_recompiled_stubs.h', 'bt2_kernel_stubs.h').replace('PS2_RECOMPILED_FUNCTIONS_H', 'BT2_KERNEL_FUNCTIONS_H').replace('PS2_RECOMPILED_STUBS_H', 'BT2_KERNEL_STUBS_H')
        destination = prepared / name.replace('ps2_recompiled_functions.h', 'bt2_kernel_functions.h').replace('ps2_recompiled_stubs.h', 'bt2_kernel_stubs.h')
        destination.write_text(text, encoding='utf-8')
        prepared_files[destination.name] = hashlib.sha256(destination.read_bytes()).hexdigest()
    manifest = {'source_elf_sha256': LOCAL_CANDIDATE_SHA256, 'source_address': SOURCE, 'source_file_offset': offset,
        'runtime_address': TARGET, 'copy_size': SIZE, 'payload_sha256': hashlib.sha256(payload).hexdigest(),
        'entry_end': end, 'map_sha256': hashlib.sha256(function_map.read_bytes()).hexdigest(),
        'packaged_elf_sha256': hashlib.sha256(executable.read_bytes()).hexdigest(),
        'lock_sha256': hashlib.sha256((ROOT / 'config/ps2recomp.lock.json').read_bytes()).hexdigest(),
        'recompiler_sha256': hashlib.sha256(tool.read_bytes()).hexdigest(), 'files': files, 'prepared_files': prepared_files,
        'discovery_log': str(discovery / 'generation.log'),
        'limitations': 'Provisional automatic entry boundary only. Bytes preserved at observed runtime address; no instruction rewriting. Remaining payload is non-executable data in scoped generation; discovery of other entries is not validated.'}
    (folder / 'generation.manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(f'Generated scoped kernel entry [0x{TARGET:X},0x{end:X}); payload/code/maps remain ignored.')


if __name__ == '__main__':
    main()
