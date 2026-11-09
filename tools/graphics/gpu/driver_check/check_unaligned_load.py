"""Checks whether this PC's Vulkan driver reads a 3-byte vector at a 3-byte stride through a buffer device address
correctly: the read paraLLEl-GS's 24-bit uploads did before the project's patch (docs/plans/graphics/GPU_COMPATIBILITY.md).

Builds load3.cpp with MSVC and three shaders with the Vulkan SDK's glslc into work/driver_check/, then runs:
  u8vec3             the original read (a u8vec3 through a buffer device address);
  bytes              the patched read (three byte loads);
  u8vec3-descriptor  the original read through a storage buffer descriptor.
Each run prints the wrong words and writes the driver's compiled code next to it (<variant>-exec0-ISA_cs.txt).
The AMD RX 9070 XT with driver 26.10.2 got 86,016 of 114,688 words wrong in u8vec3 only (2026-11-09).

    python tools/graphics/gpu/driver_check/check_unaligned_load.py
"""
import os
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
OUT = ROOT / 'work' / 'driver_check'
VARIANTS = {'u8vec3': [], 'bytes': ['-DBYTES=1'], 'u8vec3-descriptor': ['-DDESCRIPTOR=1']}


def main():
    sdk = os.environ.get('VULKAN_SDK')
    if not sdk:
        raise SystemExit('VULKAN_SDK is not set: install the Vulkan SDK')
    sdk = Path(sdk)
    OUT.mkdir(parents=True, exist_ok=True)
    for name, defines in VARIANTS.items():
        subprocess.run([str(sdk / 'Bin' / 'glslc'), '--target-env=vulkan1.2', *defines, str(HERE / 'load3.comp'),
                        '-o', str(OUT / f'{name}.spv')], check=True)
    sys.path.insert(0, str(ROOT / 'tools' / 'build'))
    import msvc
    env = msvc.environment()
    cl = shutil.which('cl', path=env.get('PATH') or env.get('Path'))
    subprocess.run([cl, '/nologo', '/EHsc', '/O2', '/std:c++17', f'/I{sdk / "Include"}', f'/Fe{OUT / "load3.exe"}',
                    f'/Fo{OUT / "load3.obj"}', str(HERE / 'load3.cpp'), '/link', str(sdk / 'Lib' / 'vulkan-1.lib')],
                   check=True, cwd=OUT, env=env, stdout=subprocess.DEVNULL)
    results = {}
    for name in VARIANTS:
        print(f'=== {name}', flush=True)
        results[name] = subprocess.run([str(OUT / 'load3.exe'), str(OUT / f'{name}.spv'), str(OUT / name)]).returncode
    print('Driver compiled code and results in', OUT)
    if results['bytes'] or results['u8vec3-descriptor']:
        raise SystemExit('A read that should always work came out wrong: look at the GPU and driver')
    print('The original read is', 'wrong on this driver (the patch is needed)' if results['u8vec3'] else 'right on this driver')


if __name__ == '__main__':
    main()
