"""Import the local candidate into Ghidra and export unreviewed function candidates."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import struct
import time

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ghidra-home", type=Path, default=ROOT.parent / "tools" / "ghidra_12.1.4_PUBLIC")
    parser.add_argument("--java-home", type=Path, default=Path("C:/Program Files/Eclipse Adoptium/jdk-25.0.3.9-hotspot"))
    args = parser.parse_args()
    executable = ROOT / "rom" / "SLUS_214.41"
    if executable.stat().st_size != 2675936:
        parser.error("Input size differs from the inspected local candidate.")
    data = executable.read_bytes()
    expected_hash = "f7e5ba8e74657b8533c3a0fedff45de0d052be3a23fe4c475059ef888a81f03e"
    if hashlib.sha256(data).hexdigest() != expected_hash:
        parser.error("Input differs from the inspected local candidate.")
    entry = struct.unpack_from("<I", data, 24)[0]
    folder = ROOT / "work" / "ghidra"
    for child in ("projects", "exports", "settings", "cache", "temp"):
        (folder / child).mkdir(parents=True, exist_ok=True)
    java = args.java_home / "bin" / "java.exe"
    utility = args.ghidra_home / "Ghidra" / "Framework" / "Utility" / "lib" / "Utility.jar"
    program_options = ["-process", "SLUS_214.41", "-noanalysis"] if (folder / "projects" / "BT2.gpr").exists() else ["-import", str(executable)]
    command = [str(java), "-Xmx2G", "-Xshare:off", "--enable-native-access=ALL-UNNAMED",
               "-Djava.system.class.loader=ghidra.GhidraClassLoader", "-Dfile.encoding=UTF8",
               "-Duser.language=en", "-Duser.country=US", "-Dlog4j.skipJansi=true",
               *[f"-Dapplication.{name}dir={folder / child}" for name, child in (("settings", "settings"), ("cache", "cache"), ("temp", "temp"))],
               f"-Djava.io.tmpdir={folder / 'temp'}", "-cp", str(utility), "ghidra.Ghidra",
               "ghidra.app.util.headless.AnalyzeHeadless", str(folder / "projects"), "BT2",
               *program_options, "-processor", "r5900:LE:32:default", "-cspec", "default",
               "-analysisTimeoutPerFile", "600", "-max-cpu", "4",
               "-scriptPath", str(ROOT / "tools" / "ghidra"), "-postScript", "ExportBT2Candidates.java", str(folder / "exports"), hex(entry),
               "-log", str(folder / "analysis.log"), "-scriptlog", str(folder / "scripts.log")]
    started = time.time()
    subprocess.run(command, cwd=ROOT, check=True)
    report_path = folder / "exports" / "analysis.json"
    if not report_path.exists() or report_path.stat().st_mtime < started:
        raise RuntimeError("Ghidra did not produce a fresh candidate report; inspect the logs.")
    report = json.loads(report_path.read_text(encoding="utf-8"))
    if report["language"] != "r5900:LE:32:default" or report["executable_sha256"].lower() != expected_hash:
        raise RuntimeError("Ghidra report language or executable hash does not match.")
    print(f"Unreviewed candidates exported under {folder / 'exports'}; no BT2 C++ generated.")


if __name__ == "__main__":
    main()
