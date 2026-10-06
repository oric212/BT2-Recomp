"""Validate private translated images against the captured heap and owned stacks."""
import argparse,hashlib,json
from pathlib import Path

def verify(root):
    rows=[]
    for f in (root/'work/iop').glob('*native.json'):
        j=json.loads(f.read_text(encoding='utf-8'));image=f.with_suffix('.bin')
        if 'base' not in j or not image.exists():continue
        data=image.read_bytes();source=f.with_suffix('.cpp').read_text(encoding='utf-8').encode('utf-8')
        if hashlib.sha256(data).hexdigest()!=j['image_sha256'] or hashlib.sha256(source).hexdigest()!=j['source_sha256']:raise ValueError('Generated provenance differs: '+f.name)
        rows.append((j['base'],j['base']+len(data),f.stem))
    if len(rows)!=15:raise ValueError('Expected thirteen residents, boot FILEIO and auxiliary CDVDFSV translations')
    rows.extend([(0xCE500,0x14FD00,'captured second heap'),(0x14FD00,0x150600,'captured CRI heap'),(0x150600,0x191700,'captured fourth heap'),(0x1D4B00,0x1DFF90,'disc request workspace/buffer'),(0x1E0000,0x200000,'reserved worker/call/argument stacks')])
    rows.sort()
    for lo,hi,name in rows:
        if not 0<=lo<hi<=0x200000:raise ValueError('Out-of-RAM placement: '+name)
    for left,right in zip(rows,rows[1:]):
        if left[1]>right[0]:raise ValueError('Overlapping ownership: '+left[2]+' / '+right[2])
    for lo,hi,name in rows:print(f'{name}: [{lo:#x}, {hi:#x})')
    return rows

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path.cwd());a=p.parse_args();verify(a.root)
