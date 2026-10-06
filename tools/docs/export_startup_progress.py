"""Publish allowlisted startup counters and a static SVG; never copy raw reports."""
import argparse
from datetime import date
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FIELDS = (
    "distinct_ee_entered", "distinct_ee_completed", "distinct_kernel_entered",
    "distinct_kernel_completed", "calls", "returns", "native_iop_steps",
    "native_module_loads", "ee_thread_count", "cri_rpc_calls", "heap_calls",
    "disc_read_calls", "disc_read_callbacks", "disc_rpc_calls",
    "disc_dma_submissions", "disc_read_dma_submissions", "gs_packets",
    "gs_sprites", "gs_pixels",
)


def snapshot(raw, measured_on):
    date.fromisoformat(measured_on)
    report = json.loads(raw)
    metrics = {}
    for key in (*FIELDS, "selector", "pc", "target"):
        value = report[key]
        if type(value) is not int or not 0 <= value <= 0xFFFFFFFFFFFFFFFF:
            raise ValueError(f"Invalid numeric metric: {key}")
        metrics[key] = value
    for entered, completed in (("distinct_ee_entered", "distinct_ee_completed"),
                               ("distinct_kernel_entered", "distinct_kernel_completed")):
        if metrics[completed] > metrics[entered]:
            raise ValueError("Completion count exceeds entered count")
    return {
        "schema_version": 1,
        "measured_on": measured_on,
        "scope": "bounded native startup probe with local ISO and prepared inputs",
        "code_state": "local implementation; final pass verification incomplete",
        "source_report_sha256": hashlib.sha256(raw).hexdigest(),
        "metrics": {key: metrics[key] for key in FIELDS},
        "boundary": {key: f"0x{metrics[key]:X}" for key in ("selector", "pc", "target")},
    }


def dashboard(data):
    m = data["metrics"]
    cards = (
        ("Distinct EE entries", f'{m["distinct_ee_entered"]:,} / {m["distinct_ee_completed"]:,}', "entered / completed"),
        ("IOP execution", f'{m["native_iop_steps"]:,}', "instructions executed"),
        ("Sound setup", f'{m["cri_rpc_calls"]:,} CRI calls', f'{m["heap_calls"]:,} heap allocations'),
        ("EE execution events", f'{m["calls"]:,} / {m["returns"]:,}', "calls / returns"),
        ("Disc startup", f'{m["disc_read_calls"]:,} read / {m["disc_rpc_calls"]:,} RPC', "bounded metadata path"),
        ("GS startup sink", f'{m["gs_packets"]:,} packets / {m["gs_sprites"]:,} draws', "no presented game frame"),
    )
    parts = ['<svg xmlns="http://www.w3.org/2000/svg" width="900" height="420" viewBox="0 0 900 420" role="img" aria-labelledby="title desc">',
             '<title id="title">BT2 measured startup dashboard</title>',
             '<desc id="desc">Bounded startup counters. Worker changes remain unverified; there is no presented game frame.</desc>',
             '<rect width="900" height="420" rx="14" fill="#111827"/>',
             '<g font-family="Segoe UI,Arial,sans-serif" fill="#f1f5f9">',
             '<text x="24" y="36" font-size="22" font-weight="700">BT2 • Measured native startup</text>',
             f'<text x="24" y="62" font-size="14" fill="#cbd5e1">{data["measured_on"]} • Observed execution; complete startup remains unfinished</text>']
    for i, (label, value, scope) in enumerate(cards):
        x, y = 24 + (i % 3) * 288, 84 + (i // 3) * 112
        parts += [f'<rect x="{x}" y="{y}" width="272" height="96" rx="9" fill="#1e293b"/>',
                  f'<text x="{x+14}" y="{y+25}" font-size="14" fill="#cbd5e1">{label}</text>',
                  f'<text x="{x+14}" y="{y+55}" font-size="24" font-weight="700">{value}</text>',
                  f'<text x="{x+14}" y="{y+78}" font-size="13" fill="#94a3b8">{scope}</text>']
    b = data["boundary"]
    parts += ['<rect x="24" y="318" width="848" height="76" rx="9" fill="#40351a"/>',
              f'<text x="38" y="345" font-size="16" fill="#fde68a">Measured boundary: selector {b["selector"]} • EE PC {b["pc"]}</text>',
              '<text x="38" y="370" font-size="14">Active: worker status/start/priority changes • final verification pending</text>',
              '</g></svg>']
    return "\n".join(parts) + "\n"


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--report", type=Path, required=True)
    p.add_argument("--measured-on", required=True, help="YYYY-MM-DD: actual measurement date")
    p.add_argument("--check", action="store_true")
    args = p.parse_args()
    data = snapshot(args.report.read_bytes(), args.measured_on)
    outputs = {
        ROOT / "docs/progress/latest.json": json.dumps(data, indent=2) + "\n",
        ROOT / "docs/progress/dashboard.svg": dashboard(data),
    }
    for path, content in outputs.items():
        if args.check:
            if not path.is_file() or path.read_text(encoding="utf-8") != content:
                raise SystemExit(f"Stale generated progress: {path.name}")
        else:
            with path.open("w", encoding="utf-8", newline="\n") as output:
                output.write(content)
    print("Startup snapshot and SVG checked." if args.check else "Published numeric startup snapshot and SVG.")


if __name__ == "__main__":
    main()