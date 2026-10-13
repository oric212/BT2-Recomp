"""Brings a save in the CodeBreaker exchange format (.cbs) onto a folder card.

Usage: import_save.py SAVE.cbs CARD_FOLDER

A folder card is a host folder with one sub-folder per save, holding the save's files as the game
wrote them (docs/plans/memory-card/MEMORY_CARD_PLAN.md). The save's folder is created under
CARD_FOLDER (for example user/memcards/Card 1); an existing one is refused, never overwritten.
`entries.txt` in the save's folder keeps what the host file system cannot: each file's mode and its
two time stamps as the card stores them.

The container: a header ("CFU", header length, unpacked and packed sizes, the save's folder name,
its time stamps and mode, its title), then the body, scrambled with RC4 from a fixed starting
permutation and packed with zlib. Unpacked, the body is a run of files, each with a 64-byte header
(time stamps, size, mode, name). The format and the permutation are as documented by mymc
(Ross Ridge, public domain), which is the behavioural reference; the code here is our own.
"""
import struct, sys, zlib
from pathlib import Path

RC4_START = bytes.fromhex(
    '5f1f856f31aa3b1821b9ce1c074c9cb481b8ef9859aef926e380a3292d735162'
    '7c6446f4341af6e1ba3a0d82790a5c1671498eac8c9f351945943f560c91000b'
    'd7b0dd3966a176521357f3bb4ee5dcf06584b2d6df153c631d8914bdd236feb1'
    'ca8ba4c69e674737426d6a039270057d962f4090c4f13e3d01f7681ec3fc72b5'
    '54cfe741e44d835512220978fadea7060823bf0fccc19761c54ae6a011c2ea74'
    '0287d5d19db77e386053958d2577105e9b7fd86edaa22e204fcd8fcbbe5ae0ed'
    '2c9ad4e2afd0a9e8ad7abca8f2eeebf5a69928246c2b755df8d38617fbc07bb3'
    '58dbc74bff0450e98869c92aabfd5b1b8ad9ec27440e33c86b933248b63043a5')


class BadSave(Exception):
    pass


def unscramble(body):
    """RC4 from the fixed permutation; the first step uses index 1."""
    s, out, j = bytearray(RC4_START), bytearray(body), 0
    for n in range(len(out)):
        i = (n + 1) & 255
        j = (j + s[i]) & 255
        s[i], s[j] = s[j], s[i]
        out[n] ^= s[(s[i] + s[j]) & 255]
    return bytes(out)


def text(raw):
    return raw.split(b'\0', 1)[0].decode('ascii')


def read_cbs(data):
    """Returns (folder name, title, folder mode, created, modified, [(name, mode, created, modified, bytes)])."""
    if data[:4] != b'CFU\0': raise BadSave('not a CodeBreaker save (no CFU signature)')
    if len(data) < 12: raise BadSave('the file ends inside its header')
    _, header_length = struct.unpack_from('<II', data, 4)
    if header_length < 124 or header_length > len(data): raise BadSave(f'header length {header_length} is not possible')
    unpacked_length, packed_length = struct.unpack_from('<II', data, 12)
    folder = text(data[20:52]); created, modified = data[52:60], data[60:68]
    mode = struct.unpack_from('<I', data, 76)[0]
    title = text(data[92:header_length])
    # The packed length is either the body's or the whole file's.
    body = data[header_length:]
    if len(body) not in (packed_length, packed_length - header_length): raise BadSave('the body is not as long as the header says')
    try: unpacked = zlib.decompress(unscramble(body))
    except zlib.error as error: raise BadSave(f'the body does not unpack: {error}')
    if len(unpacked) != unpacked_length: raise BadSave('the unpacked body is not as long as the header says')
    files, at = [], 0
    while at < len(unpacked):
        if at + 64 > len(unpacked): raise BadSave('a file header is cut off')
        file_created, file_modified, size, file_mode = struct.unpack_from('<8s8sIH', unpacked, at)
        name = text(unpacked[at + 32:at + 64])
        if at + 64 + size > len(unpacked): raise BadSave(f'{name} is cut off')
        if not name or any(c in name for c in '/\\:*?"<>|') or name in ('.', '..'): raise BadSave(f'file name {name!r} cannot be stored')
        files.append((name, file_mode, file_created, file_modified, unpacked[at + 64:at + 64 + size]))
        at += 64 + size
    if not folder or any(c in folder for c in '/\\:*?"<>|') or folder in ('.', '..'): raise BadSave(f'folder name {folder!r} cannot be stored')
    return folder, title, mode, created, modified, files


def write_save(card, save):
    folder, title, mode, created, modified, files = save
    target = Path(card) / folder
    if target.exists(): raise BadSave(f'{target} exists already; it is not overwritten')
    target.mkdir(parents=True)
    lines = [f'. {mode:04x} {created.hex()} {modified.hex()}']
    for name, file_mode, file_created, file_modified, content in files:
        (target / name).write_bytes(content)
        lines.append(f'{name} {file_mode:04x} {file_created.hex()} {file_modified.hex()}')
    (target / 'entries.txt').write_text('\n'.join(lines) + '\n', encoding='ascii')
    return target


if __name__ == '__main__':
    if len(sys.argv) != 3: sys.exit(__doc__)
    try:
        save = read_cbs(Path(sys.argv[1]).read_bytes())
        target = write_save(sys.argv[2], save)
    except BadSave as error:
        sys.exit(f'{sys.argv[1]}: {error}')
    print(f'{save[1] or save[0]}: {len(save[5])} files, {sum(len(f[4]) for f in save[5])} bytes -> {target}')
