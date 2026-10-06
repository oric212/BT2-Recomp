"""Prepare a provisional EE map automatically; quarantine ambiguous candidates.

No function purpose or boundary is certified. Raw maps/reports remain ignored.
"""

from collections import Counter
import hashlib
import importlib.util
import json
import struct
from pathlib import Path

from generate_game import ROOT, LOCAL_CANDIDATE_SHA256


def has_tail_nop_gap(candidate, metadata, data):
    """Accept one unreachable NOP between a direct tail jump and alternate JR RA."""
    start, end = candidate["start"], candidate["end_exclusive"]
    ranges = candidate.get("body_ranges", [])
    if start % 4 or end % 4 or end - start > 0x5000 or len(ranges) != 2:
        return False
    try:
        spans = [(int(a, 16), int(b, 16) + 1) for a, b in (r.split("-") for r in ranges)]
    except (ValueError, TypeError):
        return False
    gap = spans[0][1]
    if spans != [(start, gap), (gap + 4, end)] or end != gap + 12 or gap - start < 8:
        return False
    if candidate["body_size"] != end - start - 4 or gap % 4:
        return False
    sections = [row for row in metadata["sections"] if row["name"] == ".text"
                and row["flags"] & 4 and row["occupies_file_bytes"]
                and row["address"] <= start < end <= row["address"] + row["size"]]
    if len(sections) != 1:
        return False
    offset = sections[0]["file_offset"] + start - sections[0]["address"]
    if offset < 0 or offset + end - start > len(data):
        return False
    def word(address):
        return struct.unpack_from("<I", data, offset + address - start)[0]
    jump = word(gap - 8)
    destination = ((gap - 4) & 0xF0000000) | ((jump & 0x03FFFFFF) << 2)
    if word(gap) != 0 or jump >> 26 != 2 or start <= destination < end or word(gap + 4) != 0x03E00008:
        return False
    def control(ins):
        op = ins >> 26
        return op in (1, 2, 3, 4, 5, 6, 7, 20, 21, 22, 23) or (op == 0 and ins & 63 in (8, 9))
    if control(word(gap - 4)) or control(word(gap + 8)):
        return False
    for address in range(start, end, 4):
        ins = word(address)
        op = ins >> 26
        if (op == 0 and ins & 63 in (8, 9) and ins != 0x03E00008) or (op in (16, 17, 18) and (ins >> 21) & 31 >= 8):
            return False
        if op in (1, 4, 5, 6, 7, 20, 21, 22, 23):
            immediate = ins & 0xFFFF
            if immediate & 0x8000:
                immediate -= 0x10000
            if address + 4 + immediate * 4 == gap:
                return False
        elif op in (2, 3) and (((address + 4) & 0xF0000000) | ((ins & 0x03FFFFFF) << 2)) == gap:
            return False
    return True


def bounded_word_switch(address,start,end,spans,metadata,data,word):
    """Prove the reached sltiu/beq/sll/lui/addu/lw/jr table idiom.

    The index is formed by ADDIU v1,rs,imm or moved by DADDU v1,rs,zero. With BEQ, SLL is its
    delay slot. With BEQL, the delay slot runs only on the out-of-range branch, so it may be
    any non-control instruction and SLL follows it."""
    if address-start<28 or address+8>end or word(address)!=0x00600008 or word(address+4)!=0:return False
    tail=[word(address+n) for n in (-16,-12,-8,-4)]
    if tail[0]!=0x00031080 or (tail[1]&0xFFFF0000)!=0x3C030000 or tail[2]!=0x00621821 or (tail[3]&0xFFFF0000)!=0x8C630000:return False
    def control(ins):
        op=ins>>26
        return op in (1,2,3,4,5,6,7,20,21,22,23) or (op==0 and ins&63 in (8,9,12,13)) or (op in (16,17,18) and (ins>>21)&31>=8)
    if (word(address-20)&0xFFFF0000)==0x10400000:branch=address-20
    elif address-start>=32 and (word(address-24)&0xFFFF0000)==0x50400000 and not control(word(address-20)):branch=address-24
    else:return False
    index,bound=word(branch-8),word(branch-4)
    if not((index&0xFC1F0000)==0x24030000 or (index&0xFC1FFFFF)==0x0000182D) or (bound&0xFFFF0000)!=0x2C620000:return False
    count=bound&65535
    if not 1<=count<=256:return False
    immediate=word(branch)&65535;immediate=immediate-65536 if immediate&32768 else immediate
    default=branch+4+immediate*4
    if not any(a<=default<b for a,b in spans):return False
    offset=tail[3]&65535;offset=offset-65536 if offset&32768 else offset
    table=((tail[1]&65535)<<16)+offset
    regions=[x for x in metadata['sections'] if x.get('occupies_file_bytes') and x['address']<=table and table+count*4<=x['address']+x['size']]
    if len(regions)!=1 or table%4:return False
    at=regions[0]['file_offset']+table-regions[0]['address']
    if at<0 or at+count*4>len(data):return False
    targets=struct.unpack_from('<'+str(count)+'I',data,at)
    return all(t%4==0 and any(a<=t<b for a,b in spans) for t in targets)


def has_branch_nop_gaps(candidate, metadata, data):
    """Accept isolated unreachable NOPs after an unconditional branch/jump and delay slot."""
    start,end=candidate['start'],candidate['end_exclusive']
    try:spans=[(int(a,16),int(b,16)+1) for a,b in (r.split('-') for r in candidate.get('body_ranges',[]))]
    except (ValueError,TypeError):return False
    if start%4 or end%4 or end-start>0x5000 or len(spans)<2 or spans[0][0]!=start or spans[-1][1]!=end:return False
    if any(a%4 or b%4 or b<=a for a,b in spans):return False
    if any(right[0]!=left[1]+4 for left,right in zip(spans,spans[1:])):return False
    gaps={b for a,b in spans[:-1]}
    if candidate['body_size']!=end-start-4*len(gaps):return False
    sections=[x for x in metadata['sections'] if x['name']=='.text' and x['flags']&4 and x['occupies_file_bytes'] and x['address']<=start<end<=x['address']+x['size']]
    if len(sections)!=1:return False
    offset=sections[0]['file_offset']+start-sections[0]['address']
    if offset<0 or offset+end-start>len(data):return False
    def word(address):return struct.unpack_from('<I',data,offset+address-start)[0]
    def control(ins):
        op=ins>>26
        return op in (1,2,3,4,5,6,7,20,21,22,23) or (op==0 and ins&63 in (8,9)) or (op in (16,17,18) and (ins>>21)&31>=8)
    for gap in gaps:
        if gap-start<8 or word(gap)!=0 or control(word(gap-4)):return False
        transfer=word(gap-8)
        if transfer&0xFFFF0000==0x10000000:
            immediate=transfer&65535;immediate=immediate-65536 if immediate&32768 else immediate
            target=gap-4+immediate*4
            if not start<=target<end:return False
        elif transfer>>26==2:
            target=((gap-4)&0xF0000000)|((transfer&0x03FFFFFF)<<2)
            if not sections[0]['address']<=target<sections[0]['address']+sections[0]['size']:return False
        elif transfer==0x03E00008:
            continue # JR RA returns; the body scan below still rejects any branch into the gap.
        elif bounded_word_switch(gap-8,start,end,spans,metadata,data,word):
            continue # Every proven table target lies inside the body; none is the gap.
        else:return False
        if target in gaps:return False
    for address in range(start,end,4):
        ins=word(address);op=ins>>26
        # Standard JALR ra,rs is a call with a local continuation. Unknown
        # tail jumps/link registers remain quarantined; runtime still checks targets.
        standard_call=op==0 and ins&0x1FFFFF==0xF809 and (ins>>21)&31!=0
        if (op==0 and ins&63 in (8,9) and ins!=0x03E00008 and not standard_call and not bounded_word_switch(address,start,end,spans,metadata,data,word)) or (op in (16,17,18) and (ins>>21)&31>=8):return False
        if op in (1,4,5,6,7,20,21,22,23):
            immediate=ins&65535;immediate=immediate-65536 if immediate&32768 else immediate
            if address+4+immediate*4 in gaps:return False
        elif op in (2,3) and (((address+4)&0xF0000000)|((ins&0x03FFFFFF)<<2)) in gaps:return False
    return True


def select_candidates(report, metadata, max_size=0x5000, data=None):
    rows, excluded = [], []
    for candidate in report["functions"]:
        start, end = candidate["start"], candidate["end_exclusive"]
        reason = None
        padding = data is not None and (has_tail_nop_gap(candidate, metadata, data) or has_branch_nop_gaps(candidate, metadata, data))
        if not padding and (not candidate["contiguous_aligned"] or start % 4 or end % 4 or end <= start or candidate["body_size"] != end - start):
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
    selected, excluded = select_candidates(report, metadata, data=data)
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
                "nop_padding_candidates": [row["start"] for row in report["functions"]
                    if has_tail_nop_gap(row, metadata, data)],
                "branch_nop_padding_candidates": [row["start"] for row in report["functions"] if has_branch_nop_gaps(row,metadata,data)],
                "max_function_bytes": 0x5000, "limitations": "Boundaries are provisional. Upstream may discover additional functions. No VU map or overlay assumed."}
    path.with_suffix(".manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Prepared {len(selected)} provisional EE functions; quarantined {len(excluded)}. Map: {path}")


if __name__ == "__main__":
    main()
