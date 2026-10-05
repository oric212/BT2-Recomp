"""Translate the disc-matched CDVDFSV handler after checking retail code ownership."""
import argparse,hashlib,json,struct
from pathlib import Path
from inspect_image import inspect as inventory,disc_image
from inspect_irx import inspect,relocate
from generate_native import emit

def main():
 p=argparse.ArgumentParser();p.add_argument('--disc',type=Path,required=True);a=p.parse_args();root=Path(__file__).resolve().parents[2]
 image=(root/'rom/IRX/IOPRP300.IMG').read_bytes()
 if image!=disc_image(a.disc):raise ValueError('IOPRP/disc identity differs')
 row=next(r for r in inventory(image)['entries'] if r['name']=='CDVDFSV');data=image[row['offset']:row['offset']+row['size']];r=inspect(data)
 if r['sha256']!='efff610e5865357260d55409025fd0c7d518c62be4c268b2a9923b978a1ab8e6':raise ValueError('CDVDFSV identity differs')
 f=root/'work/pcsx2-capture/rpc-disc-bind-after-absent-cards';m=json.loads((f/'capture.json').read_text(encoding='utf-8'));live=(f/'iop-ram.bin').read_bytes()
 if hashlib.sha256(live).hexdigest()!=m['iop_capture_sha256']:raise ValueError('IOP capture hash differs')
 baseline=json.loads((root/'work/pcsx2-capture/rpc-mc-callback-before-absent-cards/capture.json').read_text(encoding='utf-8'))
 if any(m[k]!=baseline[k] for k in ('source_elf_sha256','pcsx2_sha256','bios_sha256','disc_sha256')):raise ValueError('Disc capture environment differs')
 retail=0x3DD30;actual=relocate(data,r,retail)
 if any(live[retail+lo:retail+hi]!=actual[lo:hi] for lo,hi in r['code_ranges']):raise ValueError('Retail CDVDFSV code differs')
 if struct.unpack_from('<3I',live,0x433D0)!=(0x80000597,retail+0x754,0x441C8) or struct.unpack_from('<I',live,0x3DD14)[0]!=retail+r['gp']:raise ValueError('Retail search handler/GP differs')
 f=root/'work/pcsx2-capture/rpc-ncmd-bind-after-absent-cards';m=json.loads((f/'capture.json').read_text(encoding='utf-8'));live=(f/'iop-ram.bin').read_bytes()
 if hashlib.sha256(live).hexdigest()!=m['iop_capture_sha256'] or any(m[k]!=baseline[k] for k in ('source_elf_sha256','pcsx2_sha256','bios_sha256','disc_sha256')):raise ValueError('NCMD capture identity differs')
 if any(live[retail+lo:retail+hi]!=actual[lo:hi] for lo,hi in r['code_ranges']) or struct.unpack_from('<3I',live,0x43418)!=(0x80000595,retail+0x3ABC,0x43DB8):raise ValueError('Retail NCMD code/handler differs')
 base=0x1A0000;memory=relocate(data,r,base);source=emit(memory,base,r['code_ranges'],r['imports'],{x['start'] for x in r['provisional_functions']}|{0x754,0x3ABC},symbol='bt2_iop_disc')
 out=root/'work/iop/disc_native.cpp';out.write_text(source,encoding='utf-8');out.with_suffix('.bin').write_bytes(memory)
 out.with_suffix('.h').write_text('#pragma once\nconstexpr unsigned iop_disc_base='+str(base)+',iop_disc_gp='+str(base+r['gp'])+';\ninline constexpr std::array<unsigned char,'+str(len(memory))+'> iop_disc_image={'+','.join(map(str,memory))+'};\n',encoding='utf-8')
 out.with_suffix('.json').write_text(json.dumps({'module_sha256':r['sha256'],'source_sha256':hashlib.sha256(source.encode()).hexdigest(),'image_sha256':hashlib.sha256(memory).hexdigest(),'retail_base':retail,'base':base,'handler':base+0x754,'command_handler':base+0x3ABC,'limitations':'Checked translated search and command handlers; boot entry/residency not executed.'},indent=2)+'\n',encoding='utf-8')
 print('Disc-matched search handler generated with independently checked retail code/GP; output ignored.')
if __name__=='__main__':main()
