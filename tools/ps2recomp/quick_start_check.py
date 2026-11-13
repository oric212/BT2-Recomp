"""Checks the quick start (runtime/probe/quick_start.h) end to end in Release tool runs, each from a fresh copy of a card:

1. An empty card: quick start creates a save file (the title's New Game, Yes to "create a save file") and reaches the
   main menu; the save's files equal those of a save made by hand on an empty card (the same presses, without quick
   start), all but the time stamps.
2. That card (or --card, a card folder holding a save, copied and never written): quick start reaches the main menu with
   the save loaded and leaves the card's files unchanged.
3. The same card pressed through by hand (Start through the logos, the film and the title, then Cross on Continue)
   reaches the main menu with the same progress structure (64 KB at *(0x3876A4)) byte for byte: quick start loads the
   save as a player's Continue does.
4. No card: quick start stops while the game asks about the card.
5. A button pressed midway: quick start stops and the player has the game.

python tools/ps2recomp/quick_start_check.py --disc <iso> [--card <card folder>] [--out <folder>]
"""
import argparse,os,re,shutil,struct,subprocess,sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
PROBE=ROOT/'tools/ps2recomp/run_startup_probe.py'
SAVE='BASLUS-21441DBZT2'
def taps(first,last,button='start',every=30):return [f'{f}:{button},{f+6}:-' for f in range(first,last+1,every)]
# By hand on an empty card: Start every 30 fields from field 400 to 1600 (logos, film, copyright, "Press START button";
# Start does nothing on the title's menu), Cross on New Game, Left to Yes for "create a save file?" (No preselected),
# Cross.
NEW_SAVE_BY_HAND=','.join(taps(400,1600)+['1800:cross,1806:-','2000:left,2006:-','2040:cross,2046:-'])
# By hand with a save: Start every 30 fields from field 1000 to 1900, Cross on Continue at field 2100 (the title's menu
# stands from field 1756 with a card holding a save).
CONTINUE_BY_HAND=','.join(taps(1000,1900)+['2100:cross,2106:-'])
PROGRESS_POINTER,PROGRESS_BYTES=0x3876A4,0x10000

def probe(out,name,frames,pad,env_extra,card):
    env={k:v for k,v in os.environ.items() if not k.startswith('BT2_')}
    env.update(env_extra)
    if card:env['BT2_CARD_FOLDER']=str(card.resolve())
    report=out/f'{name}.json'
    r=subprocess.run([sys.executable,str(PROBE),'--disc',str(args.disc),'--config','Release','--frames',str(frames),
                      '--pad-script',pad,'--report',str(report)],cwd=ROOT,env=env,capture_output=True,text=True,errors='replace')
    log=r.stdout+r.stderr
    (out/f'{name}.log').write_text(log,encoding='utf-8')
    return log,report

def files(folder,stamps=True):
    found={}
    for p in sorted(folder.rglob('*')):
        if not p.is_file():continue
        data=p.read_bytes()
        if not stamps and p.name=='entries.txt':data=b'\n'.join(b' '.join(line.split()[:2]) for line in data.splitlines()) # Name and mode.
        found[p.relative_to(folder).as_posix()]=data
    return found

def progress(report):
    ram=Path(str(report)+'.ee.bin').read_bytes()
    at=struct.unpack_from('<I',ram,PROGRESS_POINTER)[0]&0x1FFFFFF
    return ram[at:at+PROGRESS_BYTES]

def fresh(out,name,source):
    card=out/name
    shutil.rmtree(card,ignore_errors=True)
    if source:shutil.copytree(source,card)
    else:card.mkdir(parents=True)
    return card

def main():
    global args
    p=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--disc',type=Path,required=True);p.add_argument('--card',type=Path,help='a card folder holding a BT2 save (copied, never written)')
    p.add_argument('--out',type=Path,default=ROOT/'work/quickstart-check')
    args=p.parse_args()
    out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
    failures=[]
    def check(ok,what):
        print(('ok   ' if ok else 'FAIL ')+what)
        if not ok:failures.append(what)

    # 1. An empty card: a save created, as by hand.
    created=fresh(out,'created',None)
    log,_=probe(out,'create',1800,'',{'BT2_QUICK_START':'1'},created)
    reached=re.search(r'Quick start: the main menu at field (\d+), a save file created',log)
    check(bool(reached),'an empty card: a save file created and the main menu reached'+(f' (field {reached.group(1)})' if reached else ''))
    by_hand=fresh(out,'by-hand-save',None)
    probe(out,'by-hand-save',2600,NEW_SAVE_BY_HAND,{},by_hand)
    made=files(created,stamps=False)
    check(any(k.endswith(f'{SAVE}/{SAVE}') for k in made) and made==files(by_hand,stamps=False),
          'the save created equals one made by hand (files and modes; time stamps aside)')
    # 2. Quick start with a save.
    source=args.card.resolve() if args.card else created
    before=files(source)
    card=fresh(out,'quick',source)
    log,quick=probe(out,'quick',1500,'',{'BT2_QUICK_START':'1'},card)
    reached=re.search(r'Quick start: the main menu at field (\d+), the save loaded',log)
    check(bool(reached),'with a save: the main menu reached'+(f' (field {reached.group(1)})' if reached else ''))
    check(files(card)==before,'with a save: the card unchanged')
    # 3. By hand, the same card: the same progress structure.
    card=fresh(out,'hand',source)
    _,hand=probe(out,'hand',2300,CONTINUE_BY_HAND,{},card)
    if reached:
        a,b=progress(quick),progress(hand)
        check(a==b,f'the progress structure equal to Continue by hand ({sum(x!=y for x,y in zip(a,b))} of {len(a)} bytes differ)')
    # 4. No card.
    log,_=probe(out,'nocard',1500,'',{'BT2_QUICK_START':'1'},None)
    check('the card stage is still running' in log,'no card: stopped while the game asks about the card')
    # 5. A press midway.
    card=fresh(out,'press',source)
    log,_=probe(out,'press',900,'800:circle,806:-',{'BT2_QUICK_START':'1'},card)
    check('the player pressed a button' in log,'a button pressed midway: the player has the game')
    for dump in out.glob('*.bin'):dump.unlink() # The runs' memory dumps (32 MB each): compared above, not kept.
    print('QUICK START CHECK '+('PASSED' if not failures else f'FAILED ({len(failures)})'))
    return 1 if failures else 0

if __name__=='__main__':sys.exit(main())
