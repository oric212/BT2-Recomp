"""Discovery loop: run the Release probe; while it stops at an unmapped function, discover it, regenerate, rebuild, rerun.

Each round: run_startup_probe.py -> inspect_blocker.py -> analyze_game.py --discover-blocker ->
prepare_map.py -> generate_game.py --candidate-map -> receipt refresh -> expand_startup.py --all ->
prepare_runtime.py -> build. The loop ends at the frame budget, at a stop that is not a map gap, or
when the map prover rejects the discovered function ("map did not grow"). Progress goes to a
timestamped log (default work/bt2-generation/discovery-loop.log) that can be followed live:
  Get-Content "<log>" -Wait -Tail 30
"""
import argparse,difflib,json,os,re,shutil,subprocess,sys,time
from pathlib import Path
import overlay_programs
ROOT=Path(__file__).resolve().parents[2]
FOLDER=ROOT/'work/bt2-generation'
STEPS={'prepare_map.py':'rebuilding the map','generate_game.py':'regenerating C++','expand_startup.py':'binding new entries','prepare_runtime.py':'preparing sources','msvc.py':'building the Release probe','run_startup_probe.py':'running the game (Release)','inspect_blocker.py':'classifying the stop','analyze_game.py':'Ghidra discovery'}
DIRECT_CALL=re.compile(r'FUN_[0-9A-F]{8}_0x[0-9a-f]+\(rdram, ctx, runtime\); return;')

def unexpected_change(old,new):
    """First changed line that is not a tail jump turning from a dispatch into a direct call, or None.

    When a tail jump's target becomes a function of its own, the generator calls it directly; no
    other difference in an already selected raw source is accepted without a look."""
    for line in difflib.unified_diff(old,new,lineterm='',n=0):
        if line.startswith(('---','+++','@@')):continue
        text=line[1:].strip()
        if text in ('return;','}') or 'dispatchGuestBranch' in text or DIRECT_CALL.fullmatch(text):continue
        return line
    return None

def refresh_receipt(receipt,manifest,read_previous,read_current):
    """Bring the startup receipt up to the new generation; returns the files whose hash was refreshed."""
    changed=[name for name,digest in receipt['files'].items() if manifest['files'].get(name)!=digest]
    for name in changed:
        bad=unexpected_change(read_previous(name),read_current(name))
        if bad is not None:raise ValueError(f'unexpected change in {name}: {bad}')
        receipt['files'][name]=manifest['files'][name]
    receipt['map_sha256']=manifest['map_sha256']
    return changed

def is_map_gap(lines,report):
    """A stop at a branch target with no bound function, as the probe prints and reports it."""
    return bool(report.get('target')) and any(line.startswith('Branch target') for line in lines)

class Loop:
    def __init__(self,args,log):
        self.a=args;self.log=log
    def say(self,*parts):
        text=' '.join(str(p) for p in parts)
        for line in text.splitlines() or ['']:self.log.write(time.strftime('%H:%M:%S ')+line+'\n')
        self.log.flush();print(text,flush=True)
    def run(self,cmd,env=None,check=True):
        self.say('  ...',STEPS.get(Path(cmd[1]).name,cmd[1]))
        p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,env=env,errors='replace')
        if check and p.returncode:
            self.say(p.stdout[-3000:],p.stderr[-3000:]);raise SystemExit('failed: '+' '.join(cmd[:4]))
        return p.stdout+p.stderr
    def tool(self,*parts):return [sys.executable,str(ROOT/'tools'/parts[0]),*parts[1:]]
    def regenerate(self,program=None):
        """Rebuild the map of the main executable (or of an overlay program), regenerate, rebind and rebuild."""
        folder=FOLDER/program if program else FOLDER;output=ROOT/'game/generated'/(program or 'bt2')
        select=['--program',program] if program else []
        def functions():
            with open(folder/'candidates.csv',encoding='utf-8') as f:return sum(1 for _ in f)-1
        before=functions();self.run(self.tool('ps2recomp/prepare_map.py',*select));after=functions()
        self.run(self.tool('ps2recomp/generate_game.py',*select,'--candidate-map',str(folder/'candidates.csv')))
        path=FOLDER/('overlays.json' if program else 'extra-startup.json')
        whole=json.loads(path.read_text(encoding='utf-8'));receipt=whole['programs'][program] if program else whole
        manifest=json.loads((folder/'generation.manifest.json').read_text(encoding='utf-8'))
        previous=max(folder.glob('previous-output-*'),key=os.path.getmtime,default=None)
        lines=lambda source:lambda name:(source/name).read_text(encoding='utf-8').splitlines()
        if previous is None and any(manifest['files'].get(n)!=h for n,h in receipt['files'].items()):
            raise SystemExit('selected raw sources changed and no previous output is kept to compare with')
        try:changed=refresh_receipt(receipt,manifest,lines(previous) if previous else None,lines(output))
        except ValueError as e:raise SystemExit(str(e))
        if changed:self.say('tail jumps now direct in',changed)
        # Each generation keeps the output it replaced; only the latest is ever compared with.
        for stale in folder.glob('previous-output-*'):
            if stale!=previous:shutil.rmtree(stale,ignore_errors=True)
        path.write_text(json.dumps(whole,indent=2)+'\n',encoding='utf-8')
        self.say(self.run(self.tool('ps2recomp/expand_startup.py','--all')).strip().splitlines()[-1])
        self.run(self.tool('ps2recomp/prepare_runtime.py'))
        self.run(self.tool('build/msvc.py','cmake','--build','build/probe','--config','Release','--target','bt2-entry-probe'))
        self.say((program or 'main')+' map',before,'->',after);return after-before
    def probe(self):
        env=dict(os.environ);env['BT2_STALL_FIELDS']=str(self.a.stall_fields)
        cmd=self.tool('ps2recomp/run_startup_probe.py','--disc',str(self.a.disc),'--config','Release','--frames',str(self.a.frames))
        if self.a.pad is not None:cmd+=['--pad-script',self.a.pad]
        out=self.run(cmd,env=env,check=False)
        lines=[l for l in out.splitlines() if l and not l.startswith('[INFO]') and 'WARN' not in l]
        report=json.loads((FOLDER/'entry-stop.json').read_text(encoding='utf-8'))
        self.say('\n'.join(l[:300] for l in lines if not l.startswith(('Distinct','EE call','CRI calls','Completion','Traceback','  File','    ','subprocess.')))[-900:])
        self.say('flips',report['gs_display_flips'],'fields',report['vblank_deliveries'],'pc',hex(report['pc']),'target',hex(report['target']),
                 'entered',report['distinct_ee_entered'],'completed',report['distinct_ee_completed'],'budget',report['frame_budget_reached'])
        return report,lines
    def player_stop(self):
        """The stop of the last session played with bt2-game.exe, as a run of the probe would leave it."""
        report_path=ROOT/'user/last-run.json'
        if not report_path.exists():raise SystemExit('no stopped play session: user/last-run.json is missing')
        if self.a.build_stamp().stat().st_mtime>report_path.stat().st_mtime:raise SystemExit('the runtime was rebuilt after that session; play again first')
        for suffix in ('','.ee.bin','.gs.bin','.iop.bin'):
            source=Path(str(report_path)+suffix)
            if source.exists():shutil.copyfile(source,str(FOLDER/'entry-stop.json')+suffix)
        lines=(ROOT/'user/last-run.log').read_text(encoding='utf-8',errors='replace').splitlines()
        lines=[l for l in lines if l and not l.startswith('[INFO]')]
        report=json.loads(report_path.read_text(encoding='utf-8'))
        self.say('\n'.join(l[:300] for l in lines[-6:]))
        self.say('flips',report['gs_display_flips'],'pc',hex(report['pc']),'target',hex(report['target']))
        return report,lines
    def loop(self):
        self.say('=== discovery loop started:','the last play session' if self.a.player_stop else f'up to {self.a.rounds} rounds, frame budget {self.a.frames}')
        if self.a.regenerate:self.regenerate(None if self.a.regenerate=='main' else self.a.regenerate)
        for n in range(self.a.rounds):
            self.say('--- round',n+1,'of',self.a.rounds)
            report,lines=self.player_stop() if self.a.player_stop else self.probe()
            if report['frame_budget_reached']:self.say('frame budget reached');break
            if not is_map_gap(lines,report):self.say('not a map gap; stopping: this stop needs a look, not a new function');break
            self.say(self.run(self.tool('ps2recomp/inspect_blocker.py')).strip().splitlines()[-1][:160])
            status=json.loads((FOLDER/'blocker-map.json').read_text(encoding='utf-8'))['map_status']
            if status!='map gap':self.say('status',status);break
            program=overlay_programs.program_for(report['target'])
            found=self.run(self.tool('ghidra/analyze_game.py','--discover-blocker',*(['--program',program] if program else []))).strip().splitlines()
            self.say('\n'.join(l[:200] for l in [l for l in found if not l.startswith('WARNING:')][-2:]))
            if self.regenerate(program)<=0:self.say('map did not grow; needs prover work');break
            if self.a.player_stop:self.say('the function at',hex(report['target']),'is built in; play again')
        self.say('=== discovery loop finished')

def main():
    p=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--disc',type=Path);p.add_argument('--frames',type=int,help='Frame budget of each run (display flips).')
    p.add_argument('--player-stop',action='store_true',help='No run: take the stop of the last session played with bt2-game.exe (user/last-run.json), build the missing function in and end.')
    p.add_argument('--rounds',type=int,default=20,help='Most runs before the loop ends (default: %(default)s).')
    g=p.add_mutually_exclusive_group()
    g.add_argument('--pad-script',help='Scripted controller input (run_startup_probe.py format).');g.add_argument('--pad-script-file',type=Path,help='File holding the pad script.')
    p.add_argument('--regenerate',nargs='?',const='main',metavar='PROGRAM',help='Regenerate and rebuild before the first run (after map or prover changes): the main executable, or the named overlay program.')
    p.add_argument('--stall-fields',type=int,default=300,help='Fields without a display flip that end a run (default: %(default)s).')
    p.add_argument('--log',type=Path,default=FOLDER/'discovery-loop.log',help='Live log (default: %(default)s).')
    a=p.parse_args()
    if a.player_stop:
        if a.disc or a.frames or a.pad_script or a.pad_script_file:p.error('--player-stop takes no run options')
        a.rounds=1
    elif a.rounds and (a.disc is None or a.frames is None):p.error('a run needs --disc and --frames')
    if a.disc:a.disc=a.disc.resolve()
    a.build_stamp=lambda:ROOT/'build/probe/Release/bt2-entry-probe.exe'
    a.pad=a.pad_script_file.read_text(encoding='utf-8').strip() if a.pad_script_file else a.pad_script
    a.log.parent.mkdir(parents=True,exist_ok=True)
    with open(a.log,'a',encoding='utf-8') as log:Loop(a,log).loop()
if __name__=='__main__':main()
