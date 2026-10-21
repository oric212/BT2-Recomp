"""Rebuilds paraLLEl-GS's ubershader in its generated shader header (gs/shaders/slangmosh.hpp) from the
patched source, with the Vulkan SDK's glslc.

paraLLEl-GS ships its shaders compiled: slangmosh.hpp holds every program's SPIR-V in one word array
(`spirv_bank`), each loaded by offset and byte size. The project's texture replacement patch changes
ubershader.comp (tools/graphics/gpu/parallel_gs_texture_replacement.patch), so its four variants
(FEEDBACK_COLOR x FEEDBACK_DEPTH, as gs/shaders/slangmosh.json declares them) are compiled again and put in
place of the shipped ones; the offsets of the programs after them move. The programs' resource layouts
(`reflection_bank`) stay as they are: the patched shader uses no binding the original does not. Compiling
the unpatched source this way gives pictures equal to the shipped binaries' (checked 2026-10-21).

    python tools/graphics/gpu/build_parallel_gs_shaders.py [--checkout <paraLLEl-GS folder>] [--glslc <path>]

fetch_parallel_gs.py runs this after applying the patches. It always starts from the shipped header,
kept next to it as slangmosh.hpp.orig, so running it again gives the same result.
"""
import argparse, os, re, shutil, subprocess, sys, tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
REQUEST = re.compile(r'this->(\w+)((?:\[\d+\])*) = device\.request_(program|shader)\(spirv_bank \+ (\d+), (\d+), &layout\);')


def find_glslc(given):
    if given:
        return given
    sdk = os.environ.get('VULKAN_SDK')
    candidates = [Path(sdk) / 'Bin' / 'glslc.exe'] if sdk else []
    candidates += sorted(Path('C:/VulkanSDK').glob('*/Bin/glslc.exe'), reverse=True)
    for c in candidates:
        if c.is_file():
            return str(c)
    found = shutil.which('glslc')
    if found:
        return found
    sys.exit('glslc not found: install the Vulkan SDK or pass --glslc')


def compile_variant(glslc, shaders, color, depth):
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / 'ubershader.spv'
        subprocess.run([glslc, '-fshader-stage=comp', '--target-env=vulkan1.1', '-O',
                        '-I', str(shaders.parents[1] / 'Granite' / 'assets' / 'shaders' / 'inc'),
                        f'-DFEEDBACK_COLOR={color}', f'-DFEEDBACK_DEPTH={depth}',
                        str(shaders / 'ubershader.comp'), '-o', str(out)], check=True)
        data = out.read_bytes()
    return [int.from_bytes(data[i:i + 4], 'little') for i in range(0, len(data), 4)]


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--checkout', type=Path, default=ROOT / 'work' / 'dependencies' / 'parallel-gs-cc6184af7e0c')
    p.add_argument('--glslc')
    a = p.parse_args()
    shaders = a.checkout / 'gs' / 'shaders'
    header, original = shaders / 'slangmosh.hpp', shaders / 'slangmosh.hpp.orig'
    if not original.exists():
        shutil.copy(header, original)
    text = original.read_text(encoding='utf-8')
    glslc = find_glslc(a.glslc)

    start = text.index('static const uint32_t spirv_bank[] =')
    body_start = text.index('{', start) + 1
    body_end = text.index('};', body_start)
    bank = [int(w, 16) for w in re.findall(r'0x([0-9a-fA-F]{8})u', text[body_start:body_end])]

    requests = [(name.group(1) + name.group(2), int(name.group(4)), int(name.group(5)) // 4) for name in REQUEST.finditer(text)]
    if not requests:
        sys.exit('No programs found in slangmosh.hpp')
    replaced = {}
    for name, offset, _ in requests:
        m = re.fullmatch(r'ubershader\[(\d)\]\[(\d)\]', name)
        if m:  # The first index is the first variant define (FEEDBACK_COLOR), as slangmosh enumerates them.
            replaced[offset] = compile_variant(glslc, shaders, int(m.group(1)), int(m.group(2)))
    if len(replaced) != 4:
        sys.exit(f'Expected four ubershader variants, found {len(replaced)}')

    # The blobs in bank order (programs may share one), each kept or replaced.
    new_bank, moved, at = [], {}, 0
    for offset, size in sorted({(offset, size) for _, offset, size in requests}):
        if offset != at:
            sys.exit(f'Unexpected gap or overlap in spirv_bank at word {offset}')
        words = replaced.get(offset, bank[offset:offset + size])
        moved[offset] = (len(new_bank), len(words))
        new_bank += words
        at = offset + size
    if at != len(bank):
        sys.exit('spirv_bank holds data after its last program')

    lines = ['\t' + ' '.join(f'0x{w:08x}u,' for w in new_bank[i:i + 8]) for i in range(0, len(new_bank), 8)]
    out = text[:body_start] + '\n' + '\n'.join(lines) + '\n' + text[body_end:]

    def rewrite(m):
        offset, size = moved[int(m.group(4))]
        return f'this->{m.group(1)}{m.group(2)} = device.request_{m.group(3)}(spirv_bank + {offset}, {size * 4}, &layout);'
    header.write_text(REQUEST.sub(rewrite, out), encoding='utf-8')
    print(f'ubershader rebuilt with {glslc}: spirv_bank {len(bank)} -> {len(new_bank)} words')


if __name__ == '__main__':
    main()
