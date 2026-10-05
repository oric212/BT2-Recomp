"""Terminal capture of a reached original CDVDFSV import call/return."""
import ctypes,hashlib,json,struct,time
BASE=0x3DD30;SCRATCH=0x1D0000;CODE=SCRATCH+0x100
POINTS={'sc-media':(0x4B68,50),'sc-read2':(0x4B68,50),'sc-key':(0x4B68,50),'sc-dual':(0x4B68,50),'sc-lsn':(0x4B68,50),'sc-timeout':(0x4B68,50),'read':(0x4B78,62),'sync':(0x4AF0,11),'error':(0x4AD8,8)}
def pack(w):return struct.pack('<'+str(len(w))+'I',*w)
def hook(target,kind):
    w=[]
    if kind.startswith('sc-'):w=[0x24180000|({'sc-media':0xFFF4,'sc-read2':0xFFFD,'sc-key':0xFFFC,'sc-dual':0xFFEA,'sc-lsn':0xFFE9,'sc-timeout':0xFFF1}[kind]),0x14980000,0]
    w += [0x3C18001D,0xAF1F000C,0xAF1D001C]
    for n in range(4):w += [0xAF040020|(n<<16)|n*4]
    w += [0x0C000000|(target>>2),0,0x3C18001D,0xAF020008,0x24190001,0xAF190000,0x1000FFFF,0]
    if kind.startswith('sc-'):w[1]|=len(w)-2;w += [0x08000000|(target>>2),0]
    return pack(w)
def capture(reader,ee_pointer,iop_rva,original,folder,address,args):
    from inspect_elf import inspect
    from capture_ee import FRESH_BIND_STUB,texture_stub
    kind=args.capture_disc_api;off,ordinal=POINTS[kind]
    k=reader.kernel;k.OpenProcess.restype=ctypes.c_void_p;h=k.OpenProcess(0x438,False,reader.pid)
    if not h:raise ctypes.WinError(ctypes.get_last_error())
    k.WriteProcessMemory.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
    def write(p,b):
        n=ctypes.c_size_t();v=ctypes.create_string_buffer(b)
        if not k.WriteProcessMemory(h,p,v,len(b),ctypes.byref(n)) or n.value!=len(b):raise ctypes.WinError(ctypes.get_last_error())
    try:
        ip,=struct.unpack('<Q',reader.read(reader.base+iop_rva,8));before=reader.read(ip,2*1024*1024);first,delay=struct.unpack_from('<2I',before,BASE+off)
        if first>>26!=2 or delay!=0x24000000|ordinal or before[SCRATCH:SCRATCH+0x200]!=pack([0x400D]*128):raise ValueError('API import/scratch differs')
        target=(first&0x3FFFFFF)<<2;write(ip+SCRATCH,bytes(0x200));write(ip+CODE,hook(target,kind));write(ip+BASE+off,pack([0x08000000|(CODE>>2),0]))
        r=inspect(original);s=next(s for s in r['load_segments'] if s['virtual_address']<=address<s['virtual_address']+s['file_size']);at=s['file_offset']+address-s['virtual_address'];write(ee_pointer+address,original[at:at+8]);stub=texture_stub(args.capture_point);write(ee_pointer+FRESH_BIND_STUB+(56 if args.capture_point=='rpc-ncmd-call-sema' else len(stub)-8),pack([0x08000000|(address>>2),0]))
        deadline=time.monotonic()+args.timeout
        while time.monotonic()<deadline:
            if reader.read(ip+SCRATCH,4)==pack([1]):break
            time.sleep(.02)
        else:raise TimeoutError('Disc API return not reached')
        ram=reader.read(ip,2*1024*1024);pub=ram[SCRATCH:SCRATCH+48]
        if reader.read(ip+SCRATCH,len(pub))!=pub:raise ValueError('API publication changed')
        (folder/'disc-api-iop.bin').write_bytes(ram)
        meta={'source_elf_sha256':hashlib.sha256(original).hexdigest(),'pcsx2_sha256':hashlib.sha256(args.pcsx2.read_bytes()).hexdigest(),'bios_sha256':hashlib.sha256(args.bios.read_bytes()).hexdigest(),'iop_sha256':hashlib.sha256(ram).hexdigest(),'kind':kind,'target':target,'arguments':list(struct.unpack_from('<4I',pub,32)),'result':struct.unpack_from('<I',pub,8)[0],'return_pc':struct.unpack_from('<I',pub,12)[0],'limitations':'Terminal after first reached original API; no whole request completion inferred.'}
        with args.disc.open('rb') as f:meta['disc_sha256']=hashlib.file_digest(f,'sha256').hexdigest()
        (folder/'disc-api.json').write_text(json.dumps(meta,indent=2)+'\n',encoding='utf-8');print(kind,meta['arguments'],'result',meta['result'])
    finally:k.CloseHandle(h)
