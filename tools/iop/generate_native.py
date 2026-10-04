"""Authored scoped MIPS-I static translator; output is private local code."""
import argparse,hashlib,json,struct
from pathlib import Path
from inspect_irx import inspect,relocate
from inspect_image import disc_image

def instruction(word,pc,base):
    op=word>>26;rs=(word>>21)&31;rt=(word>>16)&31;rd=(word>>11)&31;sa=(word>>6)&31;fn=word&63
    imm=word&65535;si=imm-65536 if imm&32768 else imm
    prefix=f'[[maybe_unused]] const auto a=c.get({rs}); [[maybe_unused]] const auto b=c.get({rt}); '
    def setreg(n,value):return prefix+f'const auto v=static_cast<std::uint32_t>({value}); c.advance(); c.set({n},v);'
    def branch(condition,target,link=None):
        result=prefix+f'const auto dest=({condition})?({target}):(c.pc+8); [[maybe_unused]] const auto link=c.pc+8; c.advance(); '
        if link is not None:result+=f'c.set({link},link); '
        return result+'c.branch(dest);'
    if op==0:
        if fn in (0,2,3):return setreg(rd,{0:f'b<<{sa}',2:f'b>>{sa}',3:f'iop_signed(b)>>{sa}'}[fn])
        if fn in (4,6,7):return setreg(rd,{4:'b<<(a&31)',6:'b>>(a&31)',7:'iop_signed(b)>>(a&31)'}[fn])
        if fn in (8,9):return branch('true','a',rd if fn==9 else None)
        if fn in (16,18):return setreg(rd,'c.hi' if fn==16 else 'c.lo')
        if fn in (17,19):return prefix+'c.advance(); '+('c.hi=a;' if fn==17 else 'c.lo=a;')
        if fn in (24,25):
            value='static_cast<std::int64_t>(iop_signed(a))*iop_signed(b)' if fn==24 else 'static_cast<std::uint64_t>(a)*b'
            return prefix+f'const auto v=static_cast<std::uint64_t>({value}); c.advance(); c.lo=static_cast<std::uint32_t>(v); c.hi=static_cast<std::uint32_t>(v>>32);'
        if fn in (26,27):return prefix+f'const auto result=iop_div(a,b,{str(fn==26).lower()}); c.advance(); c.lo=result.first;c.hi=result.second;'
        expressions={33:'a+b',35:'a-b',36:'a&b',37:'a|b',38:'a^b',39:'~(a|b)',42:'iop_signed(a)<iop_signed(b)',43:'a<b',32:'iop_add(a,b)',34:'iop_sub(a,b)'}
        if fn in expressions:return setreg(rd,expressions[fn])
    if op in (2,3):return branch('true',str(((pc+4)&0xF0000000)|((word&0x3FFFFFF)<<2))+'u',31 if op==3 else None)
    if op in (4,5,6,7):return branch({4:'a==b',5:'a!=b',6:'iop_signed(a)<=0',7:'iop_signed(a)>0'}[op],f'c.pc+4+static_cast<std::uint32_t>({si*4})')
    if op==1 and rt in (0,1,16,17):return branch('iop_signed(a)<0' if rt in (0,16) else 'iop_signed(a)>=0',f'c.pc+4+static_cast<std::uint32_t>({si*4})',31 if rt>=16 else None)
    if op in (8,9,10,11,12,13,14,15):
        expressions={8:f'iop_add(a,static_cast<std::uint32_t>({si}))',9:f'a+static_cast<std::uint32_t>({si})',10:f'iop_signed(a)<{si}',11:f'a<static_cast<std::uint32_t>({si})',12:f'a&{imm}u',13:f'a|{imm}u',14:f'a^{imm}u',15:f'{imm}u<<16'}
        return setreg(rt,expressions[op])
    address=f'a+static_cast<std::uint32_t>({si})'
    if op in (32,33,35,36,37):
        size={32:1,33:2,35:4,36:1,37:2}[op];value=f'rt.read({address},{size})'
        if op in (32,33):value=f'static_cast<std::int32_t>(static_cast<std::int{size*8}_t>({value}))'
        return prefix+f'const auto v=static_cast<std::uint32_t>({value});c.advance();c.load({rt},v);'
    if op in (40,41,43):return prefix+f'rt.write({address},b,{ {40:1,41:2,43:4}[op] });c.advance();'
    if op in (34,38):
        merge='(old&((shift==0)?0u:((1u<<shift)-1u)))|(mem<<shift)' if op==34 else '(old&((shift==0)?0u:(0xFFFFFFFFu<<(32-shift))))|(mem>>shift)'
        shift='(3-(addr&3))*8' if op==34 else '(addr&3)*8'
        return prefix+f'const auto old=c.merge({rt}),addr={address},mem=rt.read(addr&~3u,4),shift={shift};const auto v={merge};c.advance();c.load({rt},v);'
    if op in (42,46):
        shift='(3-(addr&3))*8' if op==42 else '(addr&3)*8';mask='0xFFFFFFFFu>>shift' if op==42 else '0xFFFFFFFFu<<shift';value='b>>shift' if op==42 else 'b<<shift'
        return prefix+f'const auto addr={address},old=rt.read(addr&~3u,4),shift={shift},mask={mask};rt.write(addr&~3u,(old&~mask)|({value}),4);c.advance();'
    return f'throw std::runtime_error("Unsupported IOP instruction at {pc:#x}");'

def emit(image,base,ranges,imports=(),seeds=(),symbol='bt2_iop_module'):
    # Bounded native functions keep MSVC CFG/debug generation scalable for IRX.
    bodies={}
    for lo,hi in ranges:
        for address in range(lo,hi,4):
            body=bodies.setdefault(address>>12,[])
            body.append(f'case {address}u: {{')
            if address in seeds:body.append(f'rt.entries.insert({address}u);')
            body.append(instruction(struct.unpack_from('<I',image,address)[0],base+address,base));body.append('break;}')
    for item in imports:
        body=bodies.setdefault(item['address']>>12,[])
        body.extend([f'case {item["address"]}u: {{',f'const auto target=c.get(31);const auto result=rt.import({json.dumps(item["library"])},{item["ordinal"]},c);c.advance();c.set(2,result);c.pc=target;break;}}'])
    lines=['#include "native_runtime.h"']
    for bucket,body in sorted(bodies.items()):
        lines += [f'static void {symbol}_{bucket}(IopRuntime& rt,IopContext& c) {{',f'while(c.pc!=IopRuntime::sentinel && ((IopRuntime::physical(c.pc)-{base}u)>>12)=={bucket}u) {{','rt.tick(c);',f'if(rt.entry_targets.contains(c.pc))rt.entries.insert(IopRuntime::physical(c.pc)-{base}u);',f'switch(IopRuntime::physical(c.pc)-{base}u) {{',*body,'default: throw std::runtime_error("Unmapped native IOP target");','}','}','}']
    lines += [f'void {symbol}(IopRuntime& rt,IopContext& c) {{','while(c.pc!=IopRuntime::sentinel) {',f'switch((IopRuntime::physical(c.pc)-{base}u)>>12) {{']
    for bucket in sorted(bodies):lines.append(f'case {bucket}u: {symbol}_{bucket}(rt,c);break;')
    lines += ['default: rt.tick(c);throw std::runtime_error("Unmapped native IOP target");','}','}','}']
    return '\n'.join(lines)+'\n'

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('module',type=Path);parser.add_argument('--disc',type=Path,required=True);parser.add_argument('--base',type=lambda x:int(x,0),default=0x10000);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--symbol',default='bt2_iop_module');parser.add_argument('--prefix',default='iop_module')
    args=parser.parse_args()
    if not args.symbol.isidentifier() or not args.prefix.isidentifier() or not args.symbol.isascii() or not args.prefix.isascii():raise ValueError('Invalid native symbol/prefix')
    root=Path(__file__).resolve().parents[2];out=args.output.resolve()
    if not out.is_relative_to(root/'work/iop'):raise ValueError('Native IRX output must stay under ignored work/iop/')
    data=args.module.read_bytes()
    if data!=disc_image(args.disc,(args.module.name.upper()+';1').encode()):raise ValueError('IRX/disc mismatch')
    report=inspect(data);image=relocate(data,report,args.base);source=emit(image,args.base,report['code_ranges'],report['imports'],{r['start'] for r in report['provisional_functions']},symbol=args.symbol)
    out.parent.mkdir(parents=True,exist_ok=True);out.write_text(source,encoding='utf-8');out.with_suffix('.bin').write_bytes(image)
    exports='\n'.join(f'rt.export_bounds[{args.base+t["address"]}u]={len(t["slots"])}u;' for t in report['exports'])
    expected=','.join(str(v) for v in image)
    out.with_suffix('.h').write_text(f'#pragma once\nconstexpr unsigned {args.prefix}_base={args.base},{args.prefix}_entry={args.base+report["entry"]},{args.prefix}_gp={args.base+report["gp"]},{args.prefix}_size={len(image)};\ninline constexpr std::array<unsigned char,{len(image)}> {"iop_expected_image" if args.prefix=="iop_module" else args.prefix+"_image"}={{{expected}}};\ninline void {"iop_export_bounds" if args.prefix=="iop_module" else args.prefix+"_export_bounds"}(IopRuntime& rt) {{{exports}}}\n',encoding='utf-8')
    out.with_suffix('.json').write_text(json.dumps({'module_sha256':report['sha256'],'source_sha256':hashlib.sha256(source.encode()).hexdigest(),'image_sha256':hashlib.sha256(image).hexdigest(),'base':args.base,'provisional_functions':len(report['provisional_functions']),'limitations':'Authored standalone MIPS-I backend, not upstream PS2Recomp. No IOP hardware, scheduler or automatic successful imports.'},indent=2)+'\n',encoding='utf-8')
    print('Generated native IOP source/image with explicit import dispatch; output remains ignored.')
if __name__=='__main__':main()
