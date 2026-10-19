"""Story sweep (STORY_MODE_PLAN.md): every part's story scene and battle without input.

Runs the Release probe once per part with BT2_START_EPISODE=<episode>,<part>,marker:<m>, one run at a
time: the map reports the player at the part's story marker, the starter answers the waits for a button
(message boxes, the pre-battle screen, the results, the save questions without saving) and ends the run
300 flips after the part's after-battle step. A part's result is 'cleared' or 'lost' (the battle's
outcome; the player's side gets no input, so most battles are lost), or 'stop' (a stop, a stall, a
waiting screen that kept getting presses, or the frame budget ran out first). --jobs runs several
parts at once (each run uses about one CPU core; a stated exception to one run at a time, for sweeps). The parts and their markers come from story_scenes.py's
scenes.json (a part whose first scene goes to its story scene itself starts without a marker); other parts
are listed as skipped. Results go to <out>/summary.tsv
(resumable, as battle_sweep.py), pictures at the chosen flips to <out>/<case>-<flip>.png.
"""
import argparse,concurrent.futures,json,sys,threading
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import battle_sweep
ROOT=battle_sweep.ROOT


def cases(parts,episodes=None):
    """BT2_START_EPISODE texts for the parts with a story marker or whose first scene goes to the story
    scene itself, and the other parts."""
    todo,skipped=[],[]
    for part in parts:
        if episodes is not None and part['episode'] not in episodes:continue
        start=f'{part["episode"]},{part["part"]}'
        if part['story_marker'] is not None:todo.append(f'{start},marker:{part["story_marker"]}')
        elif part.get('direct_story'):todo.append(start)
        else:skipped.append(start)
    return todo,skipped


def name(case):
    episode,part=case.split(',')[:2]
    return f'e{int(episode):02}-p{int(part):02}'


def outcome(report):
    """'cleared' or 'lost' once the part's battle is over, 'stuck: <function>' when a waiting screen kept
    getting presses, '' otherwise."""
    if report.get('starter_stuck'):return f'stuck: {report["starter_stuck"]:08X}'
    if report.get('starter_parts_completed',0)<1:return ''
    return 'cleared' if report.get('starter_battles_cleared') else 'lost'


def run_part(case,disc,out,frames,stall,pictures):
    """One part's run; the row's result is its outcome, or 'stop' / 'failed'."""
    found={}
    def passed(report):
        found['outcome']=outcome(report)
        return found['outcome'] in ('cleared','lost')
    row,interpreted=battle_sweep.run_case(case,disc,out,frames,stall,pictures,'BT2_START_EPISODE',name(case),passed)
    if row['result']=='ok':row['result']=found['outcome']
    elif found.get('outcome'):row['stop']=found['outcome']
    elif row['result']=='stop' and not row['stop']:row['stop']='the part did not end within the frame budget'
    return row,interpreted


def main():
    p=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--disc',type=Path,required=True)
    p.add_argument('--scenes',type=Path,default=ROOT/'work/coverage/story/scenes.json',help='story_scenes.py output.')
    p.add_argument('--episodes',help='Episode numbers, e.g. 0-3,7 (default: all).')
    p.add_argument('--case',action='append',default=[],help='A BT2_START_EPISODE text to run instead (repeatable).')
    p.add_argument('--frames',type=int,default=15000,help='Frame budget per part (default: %(default)s).')
    p.add_argument('--pictures',default='2500,4000,5500',help='Flips at which pictures are saved (default: %(default)s; empty: none).')
    p.add_argument('--stall-fields',type=int,default=600)
    p.add_argument('--out',type=Path,default=ROOT/'work/coverage/story/sweep')
    p.add_argument('--again',action='store_true',help='Rerun cases already in the summary.')
    p.add_argument('--jobs',type=int,default=1,help='Parts run at the same time (default: %(default)s; each run uses about one CPU core).')
    a=p.parse_args()
    parts=json.loads(a.scenes.read_text(encoding='utf-8'))
    todo,skipped=cases(parts,set(battle_sweep.numbers(a.episodes)) if a.episodes else None)
    if a.case:todo,skipped=a.case,[]
    pictures=tuple(battle_sweep.numbers(a.pictures)) if a.pictures else ()
    a.out.mkdir(parents=True,exist_ok=True)
    summary=a.out/'summary.tsv';rows=battle_sweep.read_summary(summary)
    seen_path=a.out/'interpreted.txt'
    seen={int(l,16) for l in seen_path.read_text().split()} if seen_path.exists() else set()
    todo=[case for case in todo if case not in rows or a.again]
    lock=threading.Lock();done=[0]
    def one(case):
        row,interpreted=run_part(case,a.disc.resolve(),a.out,a.frames,a.stall_fields,pictures)
        with lock:
            rows[case]=row;battle_sweep.write_summary(summary,rows)
            seen.update(interpreted);seen_path.write_text(''.join(f'{x:08X}\n' for x in sorted(seen)))
            done[0]+=1
            print(f'[{done[0]}/{len(todo)}] {case}: {row["result"]} flips {row.get("flips","-")} entered {row.get("entered","-")} '
                  f'interpreted {row.get("interpreted","-")} ({row["seconds"]} s) {row.get("stop","")}',flush=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:
        list(pool.map(one,todo))
    results=[r['result'] for r in rows.values()]
    print(f'{results.count("cleared")} cleared, {results.count("lost")} lost, {results.count("stop")} stopped, {results.count("failed")} failed, '
          f'{len(skipped)} parts without a way to their story scene skipped; summary {summary}')


if __name__=='__main__':main()
