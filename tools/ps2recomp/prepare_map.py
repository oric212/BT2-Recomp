"""Prepare a provisional EE map automatically; quarantine ambiguous candidates.

No function purpose or boundary is certified. Raw maps/reports remain ignored.
"""

from collections import Counter
import hashlib
import importlib.util
import json
from pathlib import Path

from generate_game import ROOT, LOCAL_CANDIDATE_SHA256


def select_candidates(report, metadata, max_size=0x5000):
    rows, excluded = [], []
    for candidate in report["functions"]:
        start, end = candidate["start"], candidate["end_exclusive"]
        reason = None
        if not candidate["contiguous_aligned"] or start % 4 or end % 4 or end <= start or candidate["body_size"] != end - start:
            reason = "noncontiguous_or_invalid"
        elif not any(section["name"] == ".text" and section["flags"] & 4 and section["occupies_file_bytes"]
                     and section["address"] <= start < end <= section["address"] + section["size"] for section in metadata["sections"]):
            reason = "outside_file_backed_ee_text"
        elif end - start > max_size:
            reason = "oversized"
        if reason:
            excluded.append({"start": start, "end": end, "reason": reason})
        else:
            rows.append({"start": start, "end": end})
    # Quarantine every member of an overlapping group, rather than choosing a guessed winner.
    rows.sort(key=lambda row: (row["start"], row["end"]))
    selected, group, group_end = [], [], 0
    def finish():
        if len(group) == 1:
            selected.extend(group)
        else:
            excluded.extend({**row, "reason": "overlap_or_duplicate"} for row in group)
    for row in rows:
        if group and row["start"] >= group_end:
            finish()
            group = []
        group.append(row)
        group_end = max(row["end"], group_end) if len(group) > 1 else row["end"]
    if group:
        finish()
    if not selected:
        raise ValueError("No eligible EE candidates.")
    return selected, excluded


def main():
    executable = ROOT / "rom/SLUS_214.41"
    data = executable.read_bytes()
    if hashlib.sha256(data).hexdigest() != LOCAL_CANDIDATE_SHA256:
        raise ValueError("Local executable hash differs from the inspected candidate.")
    export = ROOT / "work/ghidra/exports/analysis.json"
    report = json.loads(export.read_text(encoding="utf-8"))
    if report["language"] != "r5900:LE:32:default" or report["executable_sha256"].lower() != LOCAL_CANDIDATE_SHA256:
        raise ValueError("Export language/hash differs from the executable.")
    spec = importlib.util.spec_from_file_location("bt2_elf", ROOT / "tools/inspect_elf/inspect_elf.py")
    inspector = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(inspector)
    metadata = inspector.inspect(data)
    selected, excluded = select_candidates(report, metadata)
    folder = ROOT / "work/bt2-generation"
    folder.mkdir(parents=True, exist_ok=True)
    path = folder / "candidates.csv"
    text = "Name,Start,End,Size\n" + "".join(
        f"FUN_{row['start']:08X},0x{row['start']:08X},0x{row['end']:08X},{row['end']-row['start']}\n" for row in selected)
    path.write_text(text, encoding="utf-8")
    manifest = {"status": "automated provisional EE map", "executable_sha256": LOCAL_CANDIDATE_SHA256,
                "map_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "export_sha256": hashlib.sha256(export.read_bytes()).hexdigest(), "language": report["language"],
                "selected_count": len(selected), "covered_bytes": sum(row["end"]-row["start"] for row in selected),
                "excluded_counts": dict(Counter(row["reason"] for row in excluded)), "excluded": excluded,
                "max_function_bytes": 0x5000, "limitations": "Boundaries are provisional. Upstream may discover additional functions. No VU map or overlay assumed."}
    path.with_suffix(".manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Prepared {len(selected)} provisional EE functions; quarantined {len(excluded)}. Map: {path}")


if __name__ == "__main__":
    main()
