"""Link an actually reached, exactly mapped generated entry without renaming it.

Selection, CMake fragment and generated bindings remain under ignored work/.
This does not implement runtime APIs or permit unmapped/interior targets.
"""
import argparse
import csv
import json
from pathlib import Path
from run_pipeline import ROOT, verify_generated

FOLDER = ROOT / 'work/bt2-generation'


def render(addresses):
    names = [f'FUN_{address:08X}_0x{address:x}' for address in addresses]
    cmake = 'target_sources(bt2-entry-probe PRIVATE\n' + ''.join(
        f'    "${{BT2_GENERATED_DIR}}/../bt2-runtime/{name}.cpp"\n' for name in names) + ')\n'
    header = '#pragma once\ninline void registerExtraStartup(PS2Runtime& runtime) {\n' + ''.join(
        f'    runtime.registerFunction(0x{address:08X}, {name});\n'
        for address, name in zip(addresses, names)) + '}\n'
    return cmake, header


def verify_extra():
    path = FOLDER / 'extra-startup.json'
    if not path.exists():
        if any((FOLDER / name).exists() for name in ('extra-startup.cmake', 'bt2_extra_startup.h')):
            raise ValueError('Extra startup bindings have no receipt.')
        return []
    receipt = json.loads(path.read_text(encoding='utf-8'))
    manifest = json.loads((FOLDER / 'generation.manifest.json').read_text(encoding='utf-8'))
    addresses = receipt['addresses']
    if len(addresses) != len(set(addresses)) or receipt['map_sha256'] != manifest['map_sha256']:
        raise ValueError('Extra startup selection/map differs.')
    cmake, header = render(addresses)
    if ((FOLDER / 'extra-startup.cmake').read_text(encoding='utf-8') != cmake
            or (FOLDER / 'bt2_extra_startup.h').read_text(encoding='utf-8') != header):
        raise ValueError('Extra startup binding fragment differs.')
    for address in addresses:
        name = f'FUN_{address:08X}_0x{address:x}.cpp'
        if receipt['files'].get(name) != manifest['files'].get(name):
            raise ValueError('Extra startup raw source provenance differs.')
    return addresses


def select_target(target, rows):
    if not isinstance(target, int) or target <= 0 or target >= 0x80000000 or target % 4:
        raise ValueError('Expected an aligned main-ELF target.')
    if sum(int(row['Start'], 0) == target for row in rows) != 1:
        raise ValueError('Reached target is not an exact, unique mapped entry.')
    return target


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report', type=Path, default=FOLDER / 'entry-stop.json')
    args = parser.parse_args()
    verify_generated()
    addresses = verify_extra()
    stop = json.loads(args.report.read_text(encoding='utf-8'))
    with (FOLDER / 'candidates.csv').open(newline='', encoding='utf-8') as stream:
        target = select_target(stop.get('target'), list(csv.DictReader(stream)))
    if target in addresses:
        raise ValueError('Reached entry was already selected; inspect actual dispatch failure.')
    addresses.append(target)
    manifest = json.loads((FOLDER / 'generation.manifest.json').read_text(encoding='utf-8'))
    files = {f'FUN_{address:08X}_0x{address:x}.cpp':
             manifest['files'][f'FUN_{address:08X}_0x{address:x}.cpp'] for address in addresses}
    cmake, header = render(addresses)
    (FOLDER / 'extra-startup.cmake').write_text(cmake, encoding='utf-8')
    (FOLDER / 'bt2_extra_startup.h').write_text(header, encoding='utf-8')
    (FOLDER / 'extra-startup.json').write_text(json.dumps({
        'addresses': addresses, 'files': files, 'map_sha256': manifest['map_sha256'],
        'limitations': 'Actually reached mapped entries; boundaries remain provisional; no runtime behavior inferred.'
    }, indent=2) + '\n', encoding='utf-8')
    print(f'Selected neutral generated entry FUN_{target:08X}; bindings remain ignored.')


if __name__ == '__main__':
    main()
