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
import overlay_programs


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


def cop_branch(ins):
    """BC0/BC1/BC2 condition branches: an immediate target like other conditional branches."""
    return ins>>26 in (16,17,18) and (ins>>21)&31==8


def cop_control(ins):
    """Coprocessor instructions that change control flow or are not proven safe to bridge over:
    condition branches, and COP0 CO-group operations (ERET and friends). COP1/COP2 arithmetic
    (rs >= 16) is ordinary data flow."""
    op,rs=ins>>26,(ins>>21)&31
    return cop_branch(ins) or (op==16 and rs>=16) or (op in (16,17,18) and 8<rs<16)


def bounded_word_switch(address,start,end,spans,metadata,data,word):
    """Prove the reached sltiu/beq/sll/lui/addu/lw/jr table idiom.

    Safety rests on the SLTIU c,i,N bound and its guard branch on c; the instruction producing the
    index i only has to be non-control. With BEQ, SLL is its delay slot, or a non-control delay slot that
    leaves the index register intact precedes SLL (it runs on both paths). With BEQL, the delay slot
    runs only on the out-of-range branch, so it may be any non-control instruction and SLL follows it."""
    def control(ins):
        op=ins>>26
        return op in (1,2,3,4,5,6,7,20,21,22,23) or (op==0 and ins&63 in (8,9,12,13)) or cop_control(ins)
    sign=lambda v:v-65536 if v&32768 else v
    if address-start<28 or address+8>end or word(address+4)!=0:return False
    jr=word(address)
    tail=[word(address+n) for n in (-16,-12,-8,-4)] if address-start>=16 else [0]*4
    # First layout: SLTIU c,i,N / BEQ(L) c,zero / [delay] / SLL s,i,2 / LUI b / ADDU b,b,s / LW j,d(b) / JR j.
    # Registers are decoded (seen with index v1 -> v0 and index v0 -> v1 roles).
    sll,lui,addu,load=tail
    first=(jr&0xFC1FFFFF==8 and sll&0xFFE0003F==0 and (sll>>6)&31==2 and lui&0xFFE00000==0x3C000000
        and addu&0xFC0007FF==0x21 and (addu>>11)&31==(lui>>16)&31 and {(addu>>21)&31,(addu>>16)&31}=={(lui>>16)&31,(sll>>11)&31}
        and load>>26==35 and (load>>21)&31==(lui>>16)&31 and (load>>16)&31==(jr>>21)&31)
    if first:
        i,sreg,b=(sll>>16)&31,(sll>>11)&31,(lui>>16)&31
        if 0 in (i,sreg,b) or b==sreg:return False
        def keeps_index(ins):
            # A delay slot that runs on both paths must not write the index: I-type ALU/load
            # write rt, R-type ALU writes rd, stores (SB/SH/SW/SD/SQ) write no register. Anything
            # else is not proven.
            op=ins>>26
            if op==0:return ins&63 in (0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x2A,0x2B,0x2C,0x2D,0x00,0x02,0x03) and (ins>>11)&31!=i
            if op in (0x28,0x29,0x2B,0x3F,0x1F):return True
            return op in (8,9,10,11,12,13,14,15,0x20,0x21,0x23,0x24,0x25,0x37) and (ins>>16)&31!=i
        def beq_zero(ins,opcode):return ins>>26==opcode and (ins>>16)&31==0
        if beq_zero(word(address-20),4):branch=address-20
        elif address-start>=32 and beq_zero(word(address-24),20) and not control(word(address-20)):branch=address-24
        elif address-start>=32 and beq_zero(word(address-24),4) and not control(word(address-20)) and keeps_index(word(address-20)):
            branch=address-24 # BEQ whose delay slot (both paths) leaves the index intact; SLL follows it.
        else:return False
        index,bound=word(branch-8),word(branch-4)
        c=(word(branch)>>21)&31
        if control(index) or bound>>26!=11 or (bound>>21)&31!=i or (bound>>16)&31!=c or c in (0,i):return False
        hi,disp=lui&65535,sign(load&65535)
    else:
        # Second layout: SLTIU c,i,N / BEQ c,zero / LUI b (delay) / SLL s,i,2 / ADDIU b,b,lo /
        # ADDU a,s,b / LW j,0(a) / JR j. Registers are decoded and must keep the index intact.
        jr=word(address)
        if address-start<32 or jr&0xFC1FFFFF!=8:return False
        j=(jr>>21)&31;branch=address-24
        bound,beq,lui,sll,addiu,addu,load=(word(address+n) for n in (-28,-24,-20,-16,-12,-8,-4))
        c,i=(bound>>16)&31,(bound>>21)&31;b=(lui>>16)&31;sreg=(sll>>11)&31;a=(addu>>11)&31
        if bound>>26!=11 or beq&0xFFFF0000!=0x10000000|(c<<21) or lui&0xFFE00000!=0x3C000000 or sll&0xFFE0003F!=0 or (sll>>16)&31!=i or (sll>>6)&31!=2:return False
        if addiu>>26!=9 or (addiu>>21)&31!=b or (addiu>>16)&31!=b or addu&0xFC0007FF!=0x21 or {(addu>>21)&31,(addu>>16)&31}!={sreg,b}:return False
        if load&0xFFFF0000!=0x8C000000|(a<<21)|(j<<16) or 0 in (c,i,b,sreg,a,j) or b in (i,sreg) or c==i:return False
        hi,disp=lui&65535,sign(addiu&65535)+sign(load&65535) # LW displacement is part of the table address.
    count=bound&65535
    if not 1<=count<=256:return False
    immediate=word(branch)&65535;immediate=immediate-65536 if immediate&32768 else immediate
    default=branch+4+immediate*4
    if not any(a<=default<b for a,b in spans):return False
    table=(hi<<16)+disp
    regions=[x for x in metadata['sections'] if x.get('occupies_file_bytes') and x['address']<=table and table+count*4<=x['address']+x['size']]
    if len(regions)!=1 or table%4:return False
    at=regions[0]['file_offset']+table-regions[0]['address']
    if at<0 or at+count*4>len(data):return False
    targets=struct.unpack_from('<'+str(count)+'I',data,at)
    return all(t%4==0 and any(a<=t<b for a,b in spans) for t in targets)


def has_branch_nop_gaps(candidate, metadata, data, external_text=()):
    """Accept isolated unreachable NOPs after an unconditional branch/jump and delay slot.

    external_text: extra (start, end) code ranges a J may target, e.g. the main executable's .text
    for an overlay program (cross-program tail jumps)."""
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
        return op in (1,2,3,4,5,6,7,20,21,22,23) or (op==0 and ins&63 in (8,9)) or cop_control(ins)
    for gap in gaps:
        if gap-start<8 or word(gap)!=0 or control(word(gap-4)):return False
        transfer=word(gap-8)
        if transfer&0xFFFF0000==0x10000000:
            immediate=transfer&65535;immediate=immediate-65536 if immediate&32768 else immediate
            target=gap-4+immediate*4
            if not start<=target<end:return False
        elif transfer>>26==2:
            target=((gap-4)&0xF0000000)|((transfer&0x03FFFFFF)<<2)
            if not (sections[0]['address']<=target<sections[0]['address']+sections[0]['size'] or any(a<=target<b for a,b in external_text)):return False
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
        if (op==0 and ins&63 in (8,9) and ins!=0x03E00008 and not standard_call and not bounded_word_switch(address,start,end,spans,metadata,data,word)) or (cop_control(ins) and not cop_branch(ins)):return False
        if op in (1,4,5,6,7,20,21,22,23) or cop_branch(ins):
            immediate=ins&65535;immediate=immediate-65536 if immediate&32768 else immediate
            if address+4+immediate*4 in gaps:return False
        elif op in (2,3) and (((address+4)&0xF0000000)|((ins&0x03FFFFFF)<<2)) in gaps:return False
    return True


def select_candidates(report, metadata, max_size=0x5000, data=None, external_text=()):
    rows, excluded = [], []
    for candidate in report["functions"]:
        start, end = candidate["start"], candidate["end_exclusive"]
        reason = None
        padding = data is not None and (has_tail_nop_gap(candidate, metadata, data) or has_branch_nop_gaps(candidate, metadata, data, external_text))
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
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--program", choices=sorted(overlay_programs.PROGRAMS), help="Prepare a wrapped overlay's separate map")
    args = parser.parse_args()
    if args.program:
        _, data = overlay_programs.load(args.program)
        files = overlay_programs.paths(args.program)
        identity, export, folder = hashlib.sha256(data).hexdigest(), files["exports"] / "analysis.json", files["generation"]
    else:
        data = (ROOT / "rom/SLUS_214.41").read_bytes()
        identity, export, folder = LOCAL_CANDIDATE_SHA256, ROOT / "work/ghidra/exports/analysis.json", ROOT / "work/bt2-generation"
        if hashlib.sha256(data).hexdigest() != LOCAL_CANDIDATE_SHA256:
            raise ValueError("Local executable hash differs from the inspected candidate.")
    report = json.loads(export.read_text(encoding="utf-8"))
    if report["language"] != "r5900:LE:32:default" or report["executable_sha256"].lower() != identity:
        raise ValueError("Export language/hash differs from the executable.")
    spec = importlib.util.spec_from_file_location("bt2_elf", ROOT / "tools/inspect_elf/inspect_elf.py")
    inspector = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(inspector)
    metadata = inspector.inspect(data)
    external_text=()
    if args.program:
        # Overlay code may tail-jump into the main executable: its .text is an accepted J target.
        main_data=(ROOT / "rom/SLUS_214.41").read_bytes()
        external_text=tuple((x['address'],x['address']+x['size']) for x in inspector.inspect(main_data)['sections'] if x['name']=='.text')
    selected, excluded = select_candidates(report, metadata, data=data, external_text=external_text)
    folder.mkdir(parents=True, exist_ok=True)
    path = folder / "candidates.csv"
    text = "Name,Start,End,Size\n" + "".join(
        f"FUN_{row['start']:08X},0x{row['start']:08X},0x{row['end']:08X},{row['end']-row['start']}\n" for row in selected)
    path.write_text(text, encoding="utf-8")
    manifest = {"status": "automated provisional EE map", "executable_sha256": identity, "program": args.program or "executable",
                "map_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "export_sha256": hashlib.sha256(export.read_bytes()).hexdigest(), "language": report["language"],
                "selected_count": len(selected), "covered_bytes": sum(row["end"]-row["start"] for row in selected),
                "excluded_counts": dict(Counter(row["reason"] for row in excluded)), "excluded": excluded,
                "nop_padding_candidates": [row["start"] for row in report["functions"]
                    if has_tail_nop_gap(row, metadata, data)],
                "branch_nop_padding_candidates": [row["start"] for row in report["functions"] if has_branch_nop_gaps(row,metadata,data,external_text)],
                "max_function_bytes": 0x5000, "limitations": "Boundaries are provisional. Upstream may discover additional functions. No VU map or overlay assumed."}
    path.with_suffix(".manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Prepared {len(selected)} provisional EE functions; quarantined {len(excluded)}. Map: {path}")


if __name__ == "__main__":
    main()
