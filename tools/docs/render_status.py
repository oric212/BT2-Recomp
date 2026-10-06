"""Render BT2's authored component scopes as a compact status map."""
import argparse
import html
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
STATES = {"verified": ("#16835d", "Verified scope"), "partial": ("#b77912", "Partial / active"), "pending": ("#8793a5", "Pending")}


def render(data):
    groups = data["groups"]
    if len(groups) != 3 or any(len(g["items"]) != 5 for g in groups):
        raise ValueError("Status map expects three groups of five bounded scopes")
    esc = html.escape
    parts = ['<svg xmlns="http://www.w3.org/2000/svg" width="960" height="480" viewBox="0 0 960 480" role="img" aria-labelledby="title desc">',
             '<title id="title">BT2 component status map</title>',
             '<desc id="desc">Foundation, runtime and game status. Verified scopes are bounded; startup and game presentation remain incomplete.</desc>',
             '<rect x="1" y="1" width="958" height="478" rx="12" fill="#f8fafc" stroke="#dce3eb"/>',
             '<g font-family="Segoe UI,Arial,sans-serif">',
             '<text x="24" y="34" font-size="21" font-weight="600" fill="#1e293b">BT2 • Port status</text>',
             f'<text x="936" y="33" text-anchor="end" font-size="12" fill="#64748b">{esc(data["updated"])} · last measured checkpoint</text>']
    for i, (color, label) in enumerate(STATES.values()):
        x = 24 + i * 175
        parts += [f'<circle cx="{x+5}" cy="59" r="4" fill="{color}"/>',
                  f'<text x="{x+17}" y="63" font-size="12" fill="#475569">{label}</text>']
    for col, group in enumerate(groups):
        x = 24 + col * 310
        parts += [f'<text x="{x}" y="100" font-size="15" font-weight="600" fill="#334155">{esc(group["name"])}</text>']
        for row, item in enumerate(group["items"]):
            y = 114 + row * 62
            color, label = STATES[item["status"]]
            if len(item["name"]) > 28 or len(item["note"]) > 36:
                raise ValueError("Status map label too long; shorten it for readability")
            parts += [f'<g><title>{esc(item["name"])}: {esc(label)}. {esc(item["note"])}</title>',
                      f'<rect x="{x}" y="{y}" width="292" height="54" rx="6" fill="#fff" stroke="#e2e8f0"/>',
                      f'<rect x="{x}" y="{y+9}" width="3" height="36" rx="1.5" fill="{color}"/>',
                      f'<circle cx="{x+17}" cy="{y+20}" r="3.5" fill="{color}"/>',
                      f'<text x="{x+28}" y="{y+24}" font-size="13" font-weight="600" fill="#1e293b">{esc(item["name"])}</text>',
                      f'<text x="{x+13}" y="{y+42}" font-size="11.5" fill="#64748b">{esc(item["note"])}</text></g>']
    parts += ['<text x="24" y="453" font-size="12" fill="#64748b">Verified scope means the stated bounded behavior works. It does not imply the entire subsystem is complete.</text>', '</g></svg>']
    return "\n".join(parts) + "\n"


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--check", action="store_true")
    args = p.parse_args()
    source = ROOT / "docs/progress/components.json"
    path = ROOT / "docs/progress/dashboard.svg"
    content = render(json.loads(source.read_text(encoding="utf-8")))
    if args.check:
        if path.read_text(encoding="utf-8") != content:
            raise SystemExit("Component map is stale; run python tools/docs/render_status.py")
    else:
        with path.open("w", encoding="utf-8", newline="\n") as output:
            output.write(content)
    print("Component map checked." if args.check else "Component map rendered.")


if __name__ == "__main__":
    main()