"""Generate the supplied IOPRP FILEIO code for a scoped heap-handler experiment."""
import argparse,hashlib,json
from pathlib import Path
from inspect_image import inspect as inventory,disc_image
from inspect_irx import inspect,relocate
from generate_native import emit

def main():
 p=argparse.ArgumentParser();p.add_argument('--disc',type=Path,required=True);a=p.parse_args();root=Path(__file__).resolve().parents[2]
 image=(root/'rom/IRX/IOPRP300.IMG').read_bytes()
 if image!=disc_image(a.disc,b'IOPRP300.IMG;1'):raise ValueError('Local boot image/disc differs')
 row=next(r for r in inventory(image)['entries'] if r['name']=='FILEIO');data=image[row['offset']:row['offset']+row['size']]
 r=inspect(data)
 if r['name']!='FILEIO_service' or r['sha256']!='270c6408794d7817c5e016dc7f0fba6df4e93db6cb31058dec480fd8270cdb42':raise ValueError('FILEIO identity differs')
 base=0x140000;memory=relocate(data,r,base);symbol='bt2_iop_fileio_boot';prefix='iop_fileio_boot';out=root/'work/iop/fileio_boot_native.cpp'
 source=emit(memory,base,r['code_ranges'],r['imports'],{f['start'] for f in r['provisional_functions']},symbol=symbol)
 out.write_text(source,encoding='utf-8');out.with_suffix('.bin').write_bytes(memory)
 exports='\n'.join(f'rt.export_bounds[{base+t["address"]}u]={len(t["slots"])}u;' for t in r['exports'])
 out.with_suffix('.h').write_text(f'#pragma once\nconstexpr unsigned {prefix}_base={base},{prefix}_gp={base+r["gp"]};\ninline constexpr std::array<unsigned char,{len(memory)}> {prefix}_image={{{",".join(map(str,memory))}}};\ninline void {prefix}_export_bounds(IopRuntime& rt) {{{exports}}}\n',encoding='utf-8')
 out.with_suffix('.json').write_text(json.dumps({'module_sha256':r['sha256'],'image_sha256':hashlib.sha256(memory).hexdigest(),'source_sha256':hashlib.sha256(source.encode()).hexdigest(),'base':base,'provisional_functions':len(r['provisional_functions']),'limitations':'Relocated code only; boot module entry and worker startup not executed.'},indent=2)+'\n',encoding='utf-8')
 print('Generated local boot FILEIO code; output remains ignored.')
if __name__=='__main__':main()
