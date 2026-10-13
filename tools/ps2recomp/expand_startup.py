"""Link an actually reached, exactly mapped generated entry without renaming it.

Selection, CMake fragment and generated bindings remain under ignored work/.
This does not implement runtime APIs or permit unmapped/interior targets.
"""
import argparse
import collections
import csv
import json
import re
from pathlib import Path
from run_pipeline import ROOT, verify_generated
import overlay_programs

FOLDER = ROOT / 'work/bt2-generation'
OVERLAY_RECEIPT, OVERLAY_CMAKE, OVERLAY_HEADER = (FOLDER / name for name in ('overlays.json', 'overlays.cmake', 'bt2_overlays.h'))


def render(addresses):
    names = [f'FUN_{address:08X}_0x{address:x}' for address in addresses]
    cmake = 'target_sources(bt2-entry-probe PRIVATE\n' + ''.join(
        f'    "${{BT2_GENERATED_DIR}}/../bt2-runtime/{name}.cpp"\n' for name in names) + ')\n'
    # Self-contained: declares only the selected functions (no full generated declaration header), so a
    # new selection recompiles one small binding file.
    header = '#pragma once\n#include <cstdint>\n' + ''.join(
        f'void {name}(uint8_t* rdram, R5900Context* ctx, PS2Runtime* runtime);\n' for name in names) + \
        'inline void registerExtraStartup(EeRuntime& runtime) {\n' \
        '    // One table and one call site: registering in order without inlining the call per entry.\n' \
        '    static constexpr struct {std::uint32_t address; PS2Runtime::RecompiledFunction function;} entries[] = {\n' + ''.join(
        f'        {{0x{address:08X}, {name}}},\n'
        for address, name in zip(addresses, names)) + '    };\n' \
        '    for (const auto& entry : entries) runtime.registerFunction(entry.address, entry.function);\n}\n'
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


def render_overlays(selection):
    """Overlay bindings: each selected function is registered with the bytes it was generated from."""
    cmake = 'target_sources(bt2-entry-probe PRIVATE\n'
    declarations, tables, registrations = [], [], []
    for program, functions in sorted(selection.items()):
        image, _ = overlay_programs.load(program)
        base = overlay_programs.PROGRAMS[program]['base']
        for row in functions:
            start, end = row['start'], row['end']
            if not base <= start < end <= base + len(image):
                raise ValueError('Overlay function outside its image.')
            name = f'FUN_{start:08X}_0x{start:x}'
            cmake += f'    "${{BT2_GENERATED_DIR}}/../{program}-runtime/{name}.cpp"\n'
            declarations.append(f'void {name}(uint8_t* rdram, R5900Context* ctx, PS2Runtime* runtime);\n')
            body = ','.join(map(str, image[start - base:end - base]))
            tables.append(f'inline constexpr std::uint8_t bt2_overlay_{start:08X}[] = {{{body}}};\n')
            registrations.append(f'    runtime.registerOverlayFunction(0x{start:08X}, {name}, bt2_overlay_{start:08X});\n')
    header = ('#pragma once\n#include <cstdint>\n' + ''.join(declarations) + ''.join(tables)
              + 'inline void registerOverlays(EeRuntime& runtime) {\n' + ''.join(registrations) + '}\n')
    return cmake + ')\n', header


def overlay_rows(program):
    with (overlay_programs.paths(program)['generation'] / 'candidates.csv').open(newline='', encoding='utf-8') as stream:
        return list(csv.DictReader(stream))


def verify_overlays():
    """Selected overlay functions: generation provenance, map rows, raw hashes and rendered bindings."""
    if not OVERLAY_RECEIPT.exists():
        if OVERLAY_CMAKE.exists() or OVERLAY_HEADER.exists():
            raise ValueError('Overlay bindings have no receipt.')
        return {}
    receipt = json.loads(OVERLAY_RECEIPT.read_text(encoding='utf-8'))
    selection = {}
    for program, entry in receipt['programs'].items():
        manifest = overlay_programs.verify_generated(program)
        if entry['map_sha256'] != manifest['map_sha256']:
            raise ValueError('Overlay selection/map differs.')
        rows = {int(row['Start'], 0): int(row['End'], 0) for row in overlay_rows(program)}
        for row in entry['functions']:
            name = f"FUN_{row['start']:08X}_0x{row['start']:x}.cpp"
            if rows.get(row['start']) != row['end'] or entry['files'].get(name) != manifest['files'].get(name):
                raise ValueError('Overlay raw source/map provenance differs.')
        selection[program] = entry['functions']
    cmake, header = render_overlays(selection)
    if OVERLAY_CMAKE.read_text(encoding='utf-8') != cmake or OVERLAY_HEADER.read_text(encoding='utf-8') != header:
        raise ValueError('Overlay binding fragment differs.')
    return selection


def select_overlay(program, target):
    rows = [row for row in overlay_rows(program) if int(row['Start'], 0) == target]
    if len(rows) != 1:
        raise ValueError('Reached overlay target is not an exact, unique mapped entry.')
    selection = verify_overlays()
    if any(row['start'] == target for row in selection.get(program, [])):
        raise ValueError('Reached overlay entry was already selected; inspect actual dispatch failure.')
    selection.setdefault(program, []).append({'start': target, 'end': int(rows[0]['End'], 0)})
    write_overlays(selection)
    print(f'Selected neutral overlay entry FUN_{target:08X} ({program}); bindings remain ignored.')


def write_overlays(selection):
    receipt = {'programs': {}, 'limitations': 'Mapped overlay entries (reached ones first); registered with the bytes they were generated from.'}
    for name, functions in selection.items():
        manifest = overlay_programs.verify_generated(name)
        files = {f"FUN_{row['start']:08X}_0x{row['start']:x}.cpp": manifest['files'][f"FUN_{row['start']:08X}_0x{row['start']:x}.cpp"] for row in functions}
        receipt['programs'][name] = {'functions': functions, 'files': files, 'map_sha256': manifest['map_sha256']}
    cmake, header = render_overlays(selection)
    OVERLAY_CMAKE.write_text(cmake, encoding='utf-8')
    OVERLAY_HEADER.write_text(header, encoding='utf-8')
    OVERLAY_RECEIPT.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')


def remaining_overlay(rows, selected, generated):
    """Exact, unique mapped overlay entries with a generated source that are not yet selected, by address."""
    counts = collections.Counter(int(row['Start'], 0) for row in rows)
    return [{'start': int(row['Start'], 0), 'end': int(row['End'], 0)} for row in sorted(rows, key=lambda r: int(r['Start'], 0))
            if counts[int(row['Start'], 0)] == 1 and int(row['Start'], 0) not in selected
            and f"FUN_{int(row['Start'], 0):08X}_0x{int(row['Start'], 0):x}.cpp" in generated]


def select_all_overlays():
    """Bind every remaining mapped entry of every generated overlay program."""
    selection, added = verify_overlays(), 0
    for program in overlay_programs.PROGRAMS:
        if not (overlay_programs.paths(program)['generation'] / 'candidates.csv').exists():
            continue
        manifest = overlay_programs.verify_generated(program)
        functions = selection.setdefault(program, [])
        new = remaining_overlay(overlay_rows(program), {row['start'] for row in functions}, manifest['files'])
        functions += new; added += len(new)
    if added:
        write_overlays(selection)
    return added


def select_target(target, rows):
    if not isinstance(target, int) or target <= 0 or target >= 0x80000000 or target % 4:
        raise ValueError('Expected an aligned main-ELF target.')
    if sum(int(row['Start'], 0) == target for row in rows) != 1:
        raise ValueError('Reached target is not an exact, unique mapped entry.')
    return target


def static_callees(target, rows, selected, limit):
    """Direct JAL/J targets of `target` and their callees (breadth first, at most `limit`): exact, unique
    mapped main-ELF entries not yet selected. They are bound before they are reached, so the next run
    proceeds through them instead of stopping at each one; reached-entry counts are unchanged."""
    counts = collections.Counter(int(row['Start'], 0) for row in rows) if limit else {}
    unique = {address for address, count in counts.items() if count == 1}
    # Entries the entry probe already links and binds itself must not be added twice.
    bindings = (ROOT / 'runtime/probe/startup_bindings.cpp').read_text(encoding='utf-8')
    linked = {int(address, 16) for address in re.findall(r'registerFunction\(0x([0-9A-Fa-f]+)', bindings)}
    found, queue, seen = [], [target], {target}
    while queue and len(found) < limit:
        source = ROOT / f'game/generated/bt2/FUN_{queue[0]:08X}_0x{queue[0]:x}.cpp'
        queue.pop(0)
        if not source.exists():
            continue
        for match in re.finditer(r'\bj(?:al)?\s+func_([0-9A-F]+)\b',source.read_text(encoding='utf-8')):
            callee = int(match.group(1), 16)
            if callee in seen or callee not in unique or callee in selected or callee in linked or overlay_programs.program_for(callee):
                continue
            seen.add(callee); found.append(callee); queue.append(callee)
            if len(found) >= limit:
                break
    return found


def neighbours(target, rows, selected, span):
    """Exact, unique mapped main-ELF entries within `span` bytes of `target` that are not yet selected.
    Code of one source file sits together, so entries next to a reached one are likely reached soon;
    binding them ahead saves a rebuild and rerun per entry. Reached-entry counts are unchanged."""
    if not span:
        return []
    counts = collections.Counter(int(row['Start'], 0) for row in rows)
    bindings = (ROOT / 'runtime/probe/startup_bindings.cpp').read_text(encoding='utf-8')
    linked = {int(address, 16) for address in re.findall(r'registerFunction\(0x([0-9A-Fa-f]+)', bindings)}
    return sorted(address for address, count in counts.items()
                  if count == 1 and address != target and abs(address - target) <= span and address not in selected
                  and address not in linked and not overlay_programs.program_for(address)
                  and (ROOT / f'game/generated/bt2/FUN_{address:08X}_0x{address:x}.cpp').exists())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report', type=Path, default=FOLDER / 'entry-stop.json')
    parser.add_argument('--callees', type=int, default=0, help='Also bind up to this many static callees of the reached entry.')
    parser.add_argument('--neighbours', type=lambda text: int(text, 0), default=0, help='Also bind every mapped entry within this many bytes of the reached entry.')
    parser.add_argument('--all', action='store_true', help='Bind every mapped main-ELF and overlay entry not yet selected (no reached entry needed).')
    args = parser.parse_args()
    verify_generated()
    addresses = verify_extra()
    stop = json.loads(args.report.read_text(encoding='utf-8'))
    program = None if args.all else overlay_programs.program_for(stop.get('target'))
    if program:
        select_overlay(program, stop['target'])
        return
    with (FOLDER / 'candidates.csv').open(newline='', encoding='utf-8') as stream:
        rows = list(csv.DictReader(stream))
    previous = json.loads((FOLDER / 'extra-startup.json').read_text(encoding='utf-8')) if (FOLDER / 'extra-startup.json').exists() else {}
    if args.all:
        # Every remaining mapped entry: after this, a stop at an unbound entry means an unmapped one.
        target = None
        callees = neighbours(0, rows, set(addresses), 0xFFFFFFFF)
        static = previous.get('static_callees', []) + callees
        addresses += callees
    else:
        target = select_target(stop.get('target'), rows)
        if target in addresses:
            raise ValueError('Reached entry was already selected; inspect actual dispatch failure.')
        callees = static_callees(target, rows, set(addresses), args.callees)
        callees += neighbours(target, rows, set(addresses) | set(callees), args.neighbours)
        static = [a for a in previous.get('static_callees', []) if a != target] + callees
        addresses += [target] + callees
    manifest = json.loads((FOLDER / 'generation.manifest.json').read_text(encoding='utf-8'))
    files = {f'FUN_{address:08X}_0x{address:x}.cpp':
             manifest['files'][f'FUN_{address:08X}_0x{address:x}.cpp'] for address in addresses}
    cmake, header = render(addresses)
    (FOLDER / 'extra-startup.cmake').write_text(cmake, encoding='utf-8')
    (FOLDER / 'bt2_extra_startup.h').write_text(header, encoding='utf-8')
    (FOLDER / 'extra-startup.json').write_text(json.dumps({
        'addresses': addresses, 'files': files, 'map_sha256': manifest['map_sha256'], 'static_callees': static,
        'limitations': 'Actually reached mapped entries plus static_callees (direct calls of reached entries, bound before '
                       'they are reached, or mapped entries next to one); boundaries remain provisional; no runtime behavior inferred.'
    }, indent=2) + '\n', encoding='utf-8')
    if target is None:
        print(f'Selected all {len(callees)} remaining mapped entries and {select_all_overlays()} overlay entries; bindings remain ignored.')
        return
    print(f'Selected neutral generated entry FUN_{target:08X} and {len(callees)} static callees or neighbours; bindings remain ignored.')


if __name__ == '__main__':
    main()
