"""Check out the pinned paraLLEl-GS revision and the submodules a standalone build needs, then apply the
project's patches to it (config/parallel_gs.lock.json, "patches": the texture replacement hook of
docs/plans/graphics/HD_TEXTURES_PLAN.md). A checkout that already carries exactly these patches is
verified by taking them off, checking the pinned revision and putting them back."""
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools" / "ps2recomp"))
from build_tools import checkout, run  # noqa: E402


def dirty(directory):
    return bool(subprocess.check_output(["git", "-C", str(directory), "status", "--porcelain", "--untracked-files=no"], text=True).strip())


def main():
    lock = json.loads((ROOT / "config" / "parallel_gs.lock.json").read_text(encoding="utf-8"))
    record = lock["parallel-gs"]
    patches = [ROOT / p for p in lock.get("patches", [])]
    destination = ROOT / "work" / "dependencies" / f"parallel-gs-{record['revision'][:12]}"
    if destination.exists() and dirty(destination):
        # Only the project's own patches may be there: take them off in reverse order.
        for patch in reversed(patches):
            run("git", "-C", destination, "apply", "-R", patch)
    if not destination.exists():
        checkout("parallel-gs", record)
    for submodule in lock["submodules"]:
        parent, _, name = submodule.rpartition("/")
        directory = destination / parent if parent else destination
        run("git", "-C", directory, "submodule", "update", "--init", name)
    checkout("parallel-gs", record)  # Verifies the pinned, clean checkout.
    for patch in patches:
        run("git", "-C", destination, "apply", patch)
    print(destination.relative_to(ROOT).as_posix())


if __name__ == "__main__":
    main()
