"""Code the map does not cover although the programs themselves name it: what a run would stop at.

For the main executable and each overlay program, lists the addresses that are not function starts
of the map but are
- targets of JAL or J from mapped code (in unmapped code, or inside another function),
- data words that point at unmapped code right after a completed function.

`--write` adds them to the program's `entries.json`, which prepare_map.py turns into functions (see
its add_entries); then regenerate and build, for example with
    python tools/ps2recomp/discovery_loop.py --rounds 0 --regenerate
    python tools/ps2recomp/discovery_loop.py --rounds 0 --regenerate dbzp
Without arguments it only reports. Everything it reads and writes is local and ignored.
"""
import argparse, bisect, csv, json, struct, sys
from pathlib import Path
import overlay_programs
ROOT = Path(__file__).resolve().parents[2]
FOLDER = ROOT / 'work/bt2-generation'


def find_gaps(rows, word, code_range, data_ranges):
    """(direct, inside, pointers): sorted addresses. `rows`: (start, end) of the map; `word(address)`
    reads a word or returns None; `code_range`: where code lies; `data_ranges`: where to look for pointers."""
    starts = sorted(s for s, e in rows); ends = dict(rows)
    lo, hi = code_range
    def owner(address):
        at = bisect.bisect_right(starts, address) - 1
        return starts[at] if at >= 0 and address < ends[starts[at]] else None
    direct, inside = set(), set()
    for s, e in rows:
        for at in range(s, e, 4):
            ins = word(at)
            if ins is None or ins >> 26 not in (2, 3):
                continue
            target = ((at + 4) & 0xF0000000) | ((ins & 0x03FFFFFF) << 2)
            if not lo <= target < hi or word(target) is None:
                continue
            holder = owner(target)
            if holder is None:
                direct.add(target)
            elif target != holder and (ins >> 26 == 3 or holder != owner(at)):
                inside.add(target) # A call into a function's middle, or a jump there from another function.
    def after_completed(address):
        # The code before it ended: a transfer and its delay slot, then at most three NOPs.
        at = address
        for _ in range(3):
            if word(at - 4) == 0 and word(at - 8) != 0x03E00008 and (word(at - 8) or 0) >> 26 != 2:
                at -= 4
        transfer = word(at - 8)
        return transfer is not None and (transfer == 0x03E00008 or transfer >> 26 == 2)
    pointers = set()
    for a, b in data_ranges:
        for at in range(a, b, 4):
            value = word(at)
            if value is not None and lo <= value < hi and not value & 3 and owner(value) is None and word(value) and after_completed(value):
                pointers.add(value)
    return sorted(direct), sorted(inside), sorted(pointers - direct)


def program_view(name):
    """(rows, word, code range, data ranges, folder) of the main executable ('main') or an overlay."""
    folder = FOLDER if name == 'main' else FOLDER / name
    rows = sorted((int(r['Start'], 16), int(r['End'], 16)) for r in csv.DictReader(open(folder / 'candidates.csv', encoding='utf-8')))
    if name == 'main':
        elf = (ROOT / 'rom/SLUS_214.41').read_bytes()
        offset, count = struct.unpack_from('<I', elf, 0x1C)[0], struct.unpack_from('<H', elf, 0x2C)[0]
        loads = [struct.unpack_from('<6I', elf, offset + n * 32) for n in range(count)]
        loads = [(va, off, size) for kind, off, va, pa, size, memory in loads if kind == 1]
        def word(address):
            for va, off, size in loads:
                if va <= address <= va + size - 4:
                    return struct.unpack_from('<I', elf, off + address - va)[0]
            return None
        code = (rows[0][0], rows[-1][1])
        return rows, word, code, [(code[1], max(va + size for va, off, size in loads))], folder
    image, _ = overlay_programs.load(name)
    base = overlay_programs.PROGRAMS[name]['base']
    def word(address):
        return struct.unpack_from('<I', image, address - base)[0] if base <= address <= base + len(image) - 4 else None
    code = (rows[0][0], rows[-1][1])
    return rows, word, code, [(code[1], base + len(image))], folder


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--write', action='store_true', help='Add what is found to each program\'s entries.json.')
    parser.add_argument('--add', nargs='+', metavar='ADDRESS', help='Add these addresses (hexadecimal) to the entries of the program they lie in, with or without --write.')
    args = parser.parse_args()
    total = 0
    extra = [int(a, 16) for a in args.add or []]
    for name in ['main', *sorted(overlay_programs.PROGRAMS)]:
        rows, word, code, data_ranges, folder = program_view(name)
        direct, inside, pointers = find_gaps(rows, word, code, data_ranges)
        mine = [a for a in extra if (overlay_programs.program_for(a) or 'main') == name]
        found = sorted(set(direct) | set(inside) | set(pointers))
        total += len(found)
        print(f'{name}: {len(rows)} functions; not covered: {len(direct)} call or jump targets in unmapped code, '
              f'{len(inside)} inside another function, {len(pointers)} pointed at from data')
        for label, addresses in (('targets', direct), ('inside', inside), ('pointers', pointers)):
            if addresses:
                print(f'  {label}: ' + ' '.join(f'{a:08X}' for a in addresses))
        if args.write or mine:
            path = folder / 'entries.json'
            entries = set(json.loads(path.read_text(encoding='utf-8'))) if path.exists() else set()
            before = len(entries)
            entries |= set(mine) | (set(found) if args.write else set())
            path.write_text(json.dumps(sorted(entries)) + '\n', encoding='utf-8')
            print(f'  {path.name}: {before} -> {len(entries)} entries')
    print(f'{total} addresses not covered in all.' + ('' if args.write or not total else ' Run with --write, then regenerate, to map them.'))
    return 0


if __name__ == '__main__':
    sys.exit(main())
