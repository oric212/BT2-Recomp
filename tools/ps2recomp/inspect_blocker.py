"""Classify reached targets against provisional maps without guessing boundaries."""
import csv
import json
from run_pipeline import ROOT, verify_generated


def classify(address, rows, functions, excluded):
    containing = [row for row in rows if row["start"] <= address < row["end"]]
    exact = any(row["start"] == address for row in containing)
    candidates = [row for row in functions if row["start"] <= address < row["end_exclusive"]]
    return {"address": address, "neutral_name": f"FUN_{containing[0]['start'] if len(containing) == 1 else address:08X}",
            "map_status": "mapped entry" if exact else "inside mapped function" if containing else "map gap",
            "mapped_ranges": containing, "analysis_candidates": candidates,
            "exclusions": [row for row in excluded if row["start"] <= address < row["end"]]}


def main():
    verify_generated()
    folder = ROOT / "work/bt2-generation"
    stop = json.loads((folder / "entry-stop.json").read_text(encoding="utf-8"))
    address = stop.get("target") or (stop.get("pc") if "memory" in stop.get("status", "") else stop.get("handler") or stop.get("pc"))
    if address is None:
        print("Syscall blocker has no registered handler to map.")
        return
    with (folder / "candidates.csv").open(newline="") as stream:
        rows = [{"start": int(row["Start"], 0), "end": int(row["End"], 0)} for row in csv.DictReader(stream)]
    export = json.loads((ROOT / "work/ghidra/exports/analysis.json").read_text(encoding="utf-8"))
    manifest = json.loads((folder / "candidates.manifest.json").read_text(encoding="utf-8"))
    result = classify(address, rows, export["functions"], manifest["excluded"])
    result["limitations"] = "Range membership only; no boundary, handler semantics or execution is inferred."
    (folder / "blocker-map.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"Reached {result['neutral_name']}: {result['map_status']}; see {folder / 'blocker-map.json'}.")


if __name__ == "__main__":
    main()
