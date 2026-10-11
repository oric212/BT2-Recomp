"""Check out the pinned paraLLEl-GS revision and the submodules a standalone build needs."""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "ps2recomp"))
from build_tools import checkout, run  # noqa: E402


def main():
    lock = json.loads((ROOT / "config" / "parallel_gs.lock.json").read_text(encoding="utf-8"))
    record = lock["parallel-gs"]
    destination = ROOT / "work" / "dependencies" / f"parallel-gs-{record['revision'][:12]}"
    if not destination.exists():
        checkout("parallel-gs", record)
    for submodule in lock["submodules"]:
        parent, _, name = submodule.rpartition("/")
        directory = destination / parent if parent else destination
        run("git", "-C", directory, "submodule", "update", "--init", name)
    checkout("parallel-gs", record)  # Verifies the pinned, clean checkout.
    print(destination.relative_to(ROOT).as_posix())


if __name__ == "__main__":
    main()
