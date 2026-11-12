"""EE RAM of a session's checkpoints, stored as changed pages (docs/plans/validation/PCSX2_COMPARISON_PLAN.md, step 3).

A checkpoint folder holds either the whole RAM (ee-ram.bin) or the 4 KB pages that differ from an earlier checkpoint
of the same session (ram-pages.bin with ram-pages.json: the earlier folder's name and the page numbers). Consecutive
frames of a fight change about a hundred pages of 8,192, so a session of thousands of checkpoints stays small. Every
KEYFRAME-th checkpoint is stored whole, so reading one walks back at most KEYFRAME - 1 folders.
"""
import json
from pathlib import Path

import numpy as np

PAGE = 4096
KEYFRAME = 64


class Writer:
    """Stores a session's checkpoints in the order they are taken."""

    def __init__(self):
        self.previous = None          # (folder name, RAM) of the last checkpoint stored.
        self.count = 0

    def store(self, folder: Path, ram: bytes):
        if self.previous is None or self.count % KEYFRAME == 0:
            (folder / 'ee-ram.bin').write_bytes(ram)
        else:
            name, earlier = self.previous
            now = np.frombuffer(ram, dtype=np.uint8).reshape(-1, PAGE)
            then = np.frombuffer(earlier, dtype=np.uint8).reshape(-1, PAGE)
            pages = np.flatnonzero((now != then).any(axis=1))
            (folder / 'ram-pages.bin').write_bytes(now[pages].tobytes())
            (folder / 'ram-pages.json').write_text(json.dumps({'previous': name, 'pages': pages.tolist()}), encoding='utf-8')
        self.previous = (folder.name, ram)
        self.count += 1


_cache = {}
CACHED = 4


def read(folder: Path) -> bytes:
    """The checkpoint's whole RAM. The last few read are kept, so reading a session in order applies one delta each."""
    folder = Path(folder)
    key = str(folder.resolve())
    if key in _cache:
        return _cache[key]
    if (folder / 'ee-ram.bin').exists():
        ram = (folder / 'ee-ram.bin').read_bytes()
    else:
        index = json.loads((folder / 'ram-pages.json').read_text(encoding='utf-8'))
        pages = bytearray(read(folder.parent / index['previous']))
        data = (folder / 'ram-pages.bin').read_bytes()
        for k, page in enumerate(index['pages']):
            pages[page * PAGE:(page + 1) * PAGE] = data[k * PAGE:(k + 1) * PAGE]
        ram = bytes(pages)
    while len(_cache) >= CACHED:
        _cache.pop(next(iter(_cache)))
    _cache[key] = ram
    return ram


def materialize(folder: Path, out: Path) -> Path:
    """The checkpoint's RAM as a file the probe can load (ee-ram.bin itself when stored whole)."""
    whole = Path(folder) / 'ee-ram.bin'
    if whole.exists():
        return whole
    out.write_bytes(read(folder))
    return out
