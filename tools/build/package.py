"""Package the player's programs as a release folder and zip under dist/ (docs/guides/release/).

    python tools/build/package.py [--config Release] [--out dist]

The folder holds what a player needs besides their own game: the game program, the launcher and the runtime
(build/probe/<config>), the MSVC runtime DLLs they load (Visual Studio's redistributable files, shipped next to the
programs), the launcher's pictures, empty data folders with a note each (the player's disc image, HD texture packs),
the release readme and online setup guide, the license, the third-party notices and where the source is. No disc
image, game executable, texture pack, save, setting or capture of the console goes in.

Releases follow BT3-Recomp's model (docs/project/LEGAL.md, docs/plans/launcher/RELEASE_PLAN.md): the runtime ships
with the game's code compiled in; the game's data stays on the player's disc.
"""
import argparse,json,re,shutil,subprocess,sys,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/build'));sys.path.insert(0,str(ROOT/'tools/ps2recomp'))
PROGRAMS=('bt2-game.exe','bt2-launcher.exe','bt2-entry-probe.exe')
# Left out: the launcher's logo is the game's own title logo (the publishers' artwork); without it the launcher shows its title.
LEFT_OUT={'logo.png'}
CRT=('msvcp140.dll','vcruntime140.dll','vcruntime140_1.dll') # What the programs import (dumpbin /dependents), beyond Windows' own.
FORBIDDEN=re.compile(r'(\.(iso|cue|chd|img|mdf|mds|nrg|bin|pdb|ilk|map)$)|(^SLUS[_-]?214\.?41)',re.I) # .bin: captures, dumps.
DATA_NOTE='Put your own disc image of the game here (Dragon Ball Z: Budokai Tenkaichi 2, USA, SLUS-21441), as an .iso file.\nThe launcher finds it here.\n'
TEXTURE_NOTE='HD textures (optional): put a PCSX2 texture pack for the game here, in a folder of its own:\n  data\\textures\\<pack name>\\replacements\\...\nThen switch on "HD textures" under Settings, Graphics in the launcher.\n'

def redistributable(vs_root):
    """The x64 C runtime folder of Visual Studio's redistributable files (the newest version)."""
    folders=sorted((Path(vs_root)/'VC/Redist/MSVC').glob('*/x64/Microsoft.VC*.CRT'))
    if not folders:raise SystemExit('Visual Studio has no x64 C runtime redistributable files (VC/Redist/MSVC)')
    return folders[-1]

def notices(dependencies):
    """THIRD-PARTY-NOTICES.txt: each component the runtime links, with its license text."""
    lock=json.loads((ROOT/'config/parallel_gs.lock.json').read_text(encoding='utf-8'))['parallel-gs']
    gs=next(dependencies.glob('parallel-gs-*'));xx=next(dependencies.glob('xxhash-*'))
    parts=[('paraLLEl-GS',f"{lock['repository']} at {lock['revision']}, built with this project's patches (see SOURCE.txt). LGPL-3.0.",gs/'COPYING.LGPLv3'),
           ('Granite (part of paraLLEl-GS)','MIT.',gs/'Granite/LICENSE'),
           ('volk (part of Granite)','MIT.',gs/'Granite/third_party/volk/LICENSE.md'),
           ('Vulkan headers (part of Granite)','Apache-2.0 or MIT.',gs/'Granite/third_party/khronos/vulkan-headers/LICENSE.md'),
           ('xxHash','BSD 2-Clause (the library), compiled into the runtime.',xx/'LICENSE'),
           ('Microsoft Visual C++ runtime (msvcp140.dll, vcruntime140.dll, vcruntime140_1.dll)','Distributed under the Visual Studio redistributable terms.',None)]
    text='Third-party components in this release\n=======================================\n'
    for name,about,path in parts:
        text+=f'\n\n{name}\n{"-"*len(name)}\n{about}\n'
        if path:
            if not path.is_file():raise SystemExit(f'License file missing: {path}')
            text+='\n'+path.read_text(encoding='utf-8',errors='replace')
    return text

def git(*args):return subprocess.run(['git',*args],cwd=ROOT,capture_output=True,text=True,check=True).stdout.strip()

def plan(build,crt):
    """(source, path in the package) for every file; text files as (str contents, path)."""
    files=[(build/name,name) for name in PROGRAMS]+[(crt/name,name) for name in CRT]
    files+=[(path,'assets/'+path.relative_to(build/'assets').as_posix()) for path in sorted((build/'assets').rglob('*')) if path.is_file() and path.name not in LEFT_OUT]
    files+=[(ROOT/'docs/guides/release/README.md','README.txt'),(ROOT/'docs/guides/release/ONLINE_SETUP.md','ONLINE-SETUP.txt'),(ROOT/'LICENSE','LICENSE.txt')]
    files+=[(DATA_NOTE,'data/PUT YOUR GAME HERE.txt'),(TEXTURE_NOTE,'data/textures/PUT HD TEXTURE PACKS HERE.txt')]
    for source,target in files:
        if FORBIDDEN.search(Path(target).name):raise SystemExit(f'Refused: {target} looks like game content or a build leftover')
        if isinstance(source,Path) and not source.is_file():raise SystemExit(f'Missing: {source}')
    return files

def main():
    p=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--config',choices=('Release','Debug'),default='Release');p.add_argument('--out',type=Path,default=ROOT/'dist')
    a=p.parse_args()
    from msvc import installation
    version=git('describe','--tags','--always','--dirty')
    name=f'BT2-Recomp-{version}'
    folder=a.out/name;zipped=a.out/(name+'.zip')
    files=plan(ROOT/'build/probe'/a.config,redistributable(installation()))
    extra=[(notices(ROOT/'work/dependencies'),'THIRD-PARTY-NOTICES.txt'),
           (f"Source code: {git('remote','get-url','origin').removesuffix('.git')} at commit {git('rev-parse','HEAD')} (GPL-3.0, LICENSE.txt).\n"
            "paraLLEl-GS (LGPL-3.0) is built from its repository at the revision in config/parallel_gs.lock.json with the patches in\n"
            "tools/graphics/gpu/ of that source; the build scripts there rebuild it.\n",'SOURCE.txt')]
    if folder.exists():shutil.rmtree(folder)
    for source,target in files+extra:
        path=folder/target;path.parent.mkdir(parents=True,exist_ok=True)
        if isinstance(source,Path):shutil.copy2(source,path)
        else:path.write_text(source,encoding='utf-8',newline='\r\n')
    zipped.unlink(missing_ok=True)
    with zipfile.ZipFile(zipped,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for path in sorted(folder.rglob('*')):
            if path.is_file():z.write(path,Path(name)/path.relative_to(folder))
    size=sum(f.stat().st_size for f in folder.rglob('*') if f.is_file())
    print(f'{folder} ({len(files)+len(extra)} files, {size/2**20:.0f} MB)\n{zipped} ({zipped.stat().st_size/2**20:.0f} MB)')

if __name__=='__main__':main()
