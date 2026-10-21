"""Check out the pinned paraLLEl-GS revision and the submodules a standalone build needs, then apply the
project's patches to it (config/parallel_gs.lock.json, "patches": the texture replacement of
docs/plans/graphics/HD_TEXTURES_PLAN.md and the second renderer of 60 fps battles), then rebuild the patched
ubershader into the shipped shader header (build_parallel_gs_shaders.py; needs the Vulkan SDK's glslc). A
checkout that already carries exactly these patches is verified by putting the shipped shader header back,
taking the patches off, checking the pinned revision and applying them again."""
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
    header = destination / "gs" / "shaders" / "slangmosh.hpp"
    shipped = header.with_name("slangmosh.hpp.orig")
    if shipped.exists():  # The rebuilt shaders go first: the shipped header back.
        shipped.replace(header)
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
    run(sys.executable, ROOT / "tools" / "graphics" / "gpu" / "build_parallel_gs_shaders.py", "--checkout", destination)
    print(destination.relative_to(ROOT).as_posix())


if __name__ == "__main__":
    main()
