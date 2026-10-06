"""Run the current bounded native startup checkpoint using ignored local inputs."""
import argparse,json,subprocess,os
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
INPUTS=('guest-memory.bin','kernel-snapshot.bin','timer3-mode.bin','timer2-mode.bin','intc-mask.bin','timer2-readback.bin','osd-config.bin','osd-readback.bin','dma-status.bin','dma5-control.bin','sif-command.bin','sif-dma.bin','sif-rpc-state.bin','sif-bind.bin','cd-init.bin','scmd-bind.bin','heap-bind.bin','heap-allocation.bin','texture-startup.bin','ee-thread-start.bin','disc-bind.bin','ncmd-bind.bin')
def command(root,disc):
    folder=root/'work/bt2-generation';binary=root/'build/probe/Debug/bt2-entry-probe.exe'
    paths=[binary,disc,*[folder/name for name in INPUTS]]
    missing=[str(p) for p in paths if not p.is_file()]
    if missing:raise ValueError('Missing local startup inputs: '+', '.join(missing))
    # Existing C++ CLI places report after memory and disc before the final bind bundle.
    return [str(binary),str(folder/INPUTS[0]),str(folder/'entry-stop.json'),*[str(folder/name) for name in INPUTS[1:-1]],str(disc),str(folder/INPUTS[-1])]
def summary(report):
    for key in ('distinct_ee_entered','distinct_ee_completed','distinct_kernel_entered','distinct_kernel_completed'):
        if key not in report:raise ValueError('Probe needs rebuilding with distinct-entry reporting')
    return (f"Distinct EE entries: {report['distinct_ee_entered']} entered / {report['distinct_ee_completed']} completed; "
            f"kernel entries: {report['distinct_kernel_entered']} / {report['distinct_kernel_completed']}.\n"
            f"EE call/return events: {report['calls']} / {report['returns']}; IOP instructions: {report['native_iop_steps']}.\n"
            f"CRI calls: {report['cri_rpc_calls']}; heaps: {report['heap_calls']}; "
            f"boundary selector={report['selector']:#x}, PC={report['pc']:#x}, target={report['target']:#x}.\n"
            "Completion counts are observed invocation returns, not full gameplay correctness or startup completion.")
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--disc',type=Path,required=True);p.add_argument('--sound-worker',action='store_true',help='Diagnostic retained sound-worker/reverse-bind path; main priority remains pending.');a=p.parse_args()
    cmd=command(ROOT,a.disc.resolve());env=dict(os.environ);env.pop('BT2_EE_SOUND_DISPATCH_PROBE',None)
    report=ROOT/'work/bt2-generation'/('sound-worker-stop.json' if a.sound_worker else 'entry-stop.json');cmd[2]=str(report)
    if a.sound_worker:env['BT2_EE_SOUND_DISPATCH_PROBE']='1'
    subprocess.run(cmd,cwd=ROOT,env=env,check=True)
    print(summary(json.loads(report.read_text(encoding='utf-8'))))
    if a.sound_worker:print('Diagnostic sound-worker scope only: main priority request is pending; the EE stack is cancelled after the checked wait.')
if __name__=='__main__':main()
