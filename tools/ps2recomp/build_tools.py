"""Fetch pinned open-source build tools and build them separately from bt2-port."""

import json
import hashlib
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def run(*args, cwd=ROOT):
    subprocess.run([str(arg) for arg in args], cwd=cwd, check=True)


def checkout(name, record):
    revision = record["revision"]
    destination = ROOT / "work" / "dependencies" / f"{name}-{revision[:12]}"
    if not destination.exists():
        destination.parent.mkdir(parents=True, exist_ok=True)
        run("git", "clone", "--no-checkout", "--filter=blob:none", record["repository"], destination)
        run("git", "-C", destination, "checkout", "--detach", revision)
    actual = subprocess.check_output(["git", "-C", str(destination), "rev-parse", "HEAD"], text=True).strip()
    dirty = subprocess.check_output(["git", "-C", str(destination), "status", "--porcelain"], text=True).strip()
    if actual != revision or dirty:
        raise RuntimeError(f"{destination} is not a clean checkout of {revision}; preserve local changes and resolve before building.")
    return destination


def main():
    lock = json.loads((ROOT / "config" / "ps2recomp.lock.json").read_text(encoding="utf-8"))
    source = checkout("ps2recomp", lock["ps2recomp"])
    dependencies = {name: checkout(name, record) for name, record in lock["dependencies"].items()}
    build = ROOT / "build" / "ps2recomp-tools"
    options = [f"-DFETCHCONTENT_SOURCE_DIR_{name.upper()}={path}" for name, path in dependencies.items()]
    run("cmake", "-S", source, "-B", build, "-G", "Visual Studio 17 2022", "-A", "x64",
        "-DPS2X_BUILD_RECOMP=ON", "-DPS2X_BUILD_ANALYZER=ON", "-DPS2X_BUILD_RUNTIME=OFF",
        "-DPS2X_BUILD_TEST=OFF", "-DPS2X_BUILD_STUDIO=OFF", *options)
    run("cmake", "--build", build, "--config", "Debug", "--target", "ps2_recomp", "ps2_analyzer", "--parallel", "1", "--", "/nodeReuse:false")
    tool = build / "ps2xRecomp" / "Debug" / "ps2_recomp.exe"
    provenance = {"lock_sha256": hashlib.sha256((ROOT / "config" / "ps2recomp.lock.json").read_bytes()).hexdigest(),
                  "recompiler_sha256": hashlib.sha256(tool.read_bytes()).hexdigest()}
    (build / "bt2-tool-provenance.json").write_text(json.dumps(provenance, indent=2) + "\n", encoding="utf-8")
    print(f"Tools built under {build}; no game input was processed.", flush=True)


if __name__ == "__main__":
    main()
