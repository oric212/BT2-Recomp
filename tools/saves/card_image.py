"""Reads a raw PS2 memory card image (PCSX2's 8 MB .ps2 files) and brings one of its saves onto a folder card.

Usage: card_image.py IMAGE                      lists the saves on the card
       card_image.py IMAGE SAVE CARD_FOLDER     copies the save's folder SAVE onto the folder card

The folder card's layout is import_save.py's (docs/plans/memory-card/MEMORY_CARD_PLAN.md): one sub-folder per
save, its files as the game wrote them, and entries.txt with each entry's mode and its two time stamps as the card
stores them. An existing save is refused, never overwritten.

The card's file system, as documented by mymc (Ross Ridge, public domain), the behavioural reference; the code is
our own. The superblock on page 0 gives the page size, pages per cluster, the clusters, where allocatable
clusters start (alloc_offset), the root folder's first cluster and the indirect FAT clusters (ifc_list). A raw
image may carry each page's 16 spare bytes (ECC) after its 512, which are skipped. The FAT: allocatable cluster n's
entry is in FAT cluster n / 256 (listed in the indirect clusters), bit 31 set when in use, the rest the next
cluster or 0x7FFFFFFF at the chain's end. A folder is a chain of 512-byte entries: mode, its length (bytes of a
file, entries of a folder), created, first cluster, modified, name at +0x40.
"""
import struct, sys
from pathlib import Path
from import_save import BadSave, write_save

MAGIC = b'Sony PS2 Memory Card Format '
EXISTS, FOLDER, FILE = 0x8000, 0x0020, 0x0010
END = 0x7FFFFFFF


class CardImage:
    def __init__(self, data):
        if not data.startswith(MAGIC): raise BadSave('not a PS2 memory card image (no superblock)')
        self.page_len, self.pages_per_cluster = struct.unpack_from('<HH', data, 0x28)
        self.clusters, self.alloc_offset, _, self.root = struct.unpack_from('<IIII', data, 0x30)
        self.ifc_list = struct.unpack_from('<32I', data, 0x50)
        if self.page_len not in (512, 1024) or self.pages_per_cluster not in (1, 2, 4, 8): raise BadSave('the superblock has an unknown layout')
        pages = self.clusters * self.pages_per_cluster
        spare = self.page_len // 32
        if len(data) == pages * (self.page_len + spare): self.stride = self.page_len + spare
        elif len(data) == pages * self.page_len: self.stride = self.page_len
        else: raise BadSave(f'{len(data)} bytes is not the size of a card of {pages} pages')
        self.data = data
        self.cluster_len = self.page_len * self.pages_per_cluster
        self.per_cluster = self.cluster_len // 4

    def cluster(self, absolute):
        if absolute >= self.clusters: raise BadSave(f'cluster {absolute} is outside the card')
        first = absolute * self.pages_per_cluster
        return b''.join(self.data[(first + p) * self.stride:(first + p) * self.stride + self.page_len] for p in range(self.pages_per_cluster))

    def fat(self, n):
        index = n // self.per_cluster
        indirect = self.ifc_list[index // self.per_cluster]
        fat_cluster = struct.unpack_from('<I', self.cluster(indirect), (index % self.per_cluster) * 4)[0]
        return struct.unpack_from('<I', self.cluster(fat_cluster), (n % self.per_cluster) * 4)[0]

    def chain(self, first):
        n, seen = first, set()
        while True:
            if n in seen: raise BadSave(f'the cluster chain from {first} loops')
            seen.add(n)
            yield self.cluster(self.alloc_offset + n)
            entry = self.fat(n)
            if not entry & 0x80000000: raise BadSave(f'cluster {n} is in a chain but not in use')
            n = entry & 0x7FFFFFFF
            if n == END: return

    def read(self, first, length):
        out = bytearray()
        for block in self.chain(first):
            out += block
            if len(out) >= length: break
        if len(out) < length: raise BadSave(f'the chain from {first} ends before {length} bytes')
        return bytes(out[:length])

    def folder(self, first, count):
        raw = self.read(first, count * 512)
        entries = []
        for i in range(count):
            mode, length, created, cluster, modified = struct.unpack_from('<HxxI8sI4x8s', raw, i * 512)
            name = raw[i * 512 + 0x40:i * 512 + 0x60].split(b'\0', 1)[0].decode('ascii', 'replace')
            entries.append({'mode': mode, 'length': length, 'created': created, 'cluster': cluster, 'modified': modified, 'name': name})
        return entries

    def saves(self):
        root = self.folder(self.root, 1)[0]  # "." holds the root's own length.
        return [e for e in self.folder(self.root, root['length'])[2:] if e['mode'] & EXISTS and e['mode'] & FOLDER]

    def save(self, name):
        """The save's folder as import_save.read_cbs gives one: (folder, title, mode, created, modified, files)."""
        found = [e for e in self.saves() if e['name'] == name]
        if not found: raise BadSave(f'no save {name!r} on the card')
        entry = found[0]
        files = []
        for f in self.folder(entry['cluster'], entry['length'])[2:]:
            if not f['mode'] & EXISTS: continue
            if not f['mode'] & FILE: raise BadSave(f'{name}/{f["name"]} is a folder; saves hold files only')
            files.append((f['name'], f['mode'], f['created'], f['modified'], self.read(f['cluster'], f['length']) if f['length'] else b''))
        return entry['name'], '', entry['mode'], entry['created'], entry['modified'], files


if __name__ == '__main__':
    if len(sys.argv) not in (2, 4): sys.exit(__doc__)
    try:
        card = CardImage(Path(sys.argv[1]).read_bytes())
        if len(sys.argv) == 2:
            for e in card.saves(): print(f"{e['name']}  ({e['length'] - 2} files)")
        else:
            save = card.save(sys.argv[2])
            target = write_save(sys.argv[3], save)
            print(f'{save[0]}: {len(save[5])} files, {sum(len(f[4]) for f in save[5])} bytes -> {target}')
    except BadSave as error:
        sys.exit(f'{sys.argv[1]}: {error}')
