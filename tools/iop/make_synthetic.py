"""Emit authored MIPS-I test programs; contains no retail code."""
import struct
from pathlib import Path
from generate_native import emit
BASE=0x10000

def build():
    image=bytearray(0x1020);ranges=[]
    def program(offset,words):
        for n,word in enumerate(words):struct.pack_into('<I',image,offset+n*4,word)
        ranges.append((offset,offset+len(words)*4))
    program(0,[0x2401000A,0x10210002,0x24020007,0x24020063,0x03E00008,0x24030009])
    program(0x40,[0x8C820000,0x24430001,0x24450001,0x03E00008,0])
    program(0x80,[0x8C820000,0x24020009,0x24430000,0x03E00008,0])
    program(0xA0,[0x00850018,0x00001012,0x00001810,0x0085001A,0x00003012,0x00003810,0x03E00008,0])
    program(0xC0,[0x88820003,0x98820000,0,0x03E00008,0])
    program(0x100,[0xA8820003,0xB8820000,0x03E00008,0])
    program(0x140,[0x0C000000|((BASE+0x180)>>2),0,0x01000008,0])
    program(0x160,[0x8C820000,0x10400002,0x24430000,0x24030064,0x03E00008,0])
    program(0x1C0,[0x03E08021,0x24050004,0x24060011,0x3C070002,0x0C000000|((BASE+0x200)>>2),0,0x24030009,0x02000008,0])
    program(0xFF8,[0x10010005,0x24030009])
    program(0x1000,[0x24020007,0x03E00008,0])
    program(0x1010,[0x24020063,0x03E00008,0])
    return emit(image,BASE,ranges,[{'address':0x180,'library':'synthetic','ordinal':4},{'address':0x200,'library':'thevent','ordinal':10}],symbol='bt2_iop_synthetic')
if __name__=='__main__':
    out=Path(__file__).resolve().parents[2]/'work/iop/synthetic_native.cpp';out.parent.mkdir(parents=True,exist_ok=True);out.write_text(build(),encoding='utf-8')
