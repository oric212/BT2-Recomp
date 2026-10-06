"""Capture EE RAM at the observed BT2 kernel scan using an isolated PCSX2 process.

Only local ELF/isolated diagnostic process copies are instrumented. Captures,
BIOS copies and patched ELF output stay under ignored work/pcsx2-capture/.
"""
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/inspect_elf'))
from inspect_elf import inspect

HANDLER = 0x00338388
MARKER = 0x00387E80
EXPECTED_SHA = 'f7e5ba8e74657b8533c3a0fedff45de0d052be3a23fe4c475059ef888a81f03e'
TRAP = struct.pack('<6I', 0x3C180038, 0x37187E80, 0x3419B702,
                   0xAF190000, 0x08000000 | ((HANDLER + 16) >> 2), 0)


LOADFILE_POINTS={'loadfile-bind-after':(0x337264,0xB737),'loadfile-version-after':(0x3372A8,0xB738),'loadfile-module-before':(0x337BDC,0xB739),'loadfile-module-after':(0x337BE4,0xB73A),'mcman-module-after':(0x100434,0xB73B),'mcserv-module-after':(0x100454,0xB73C),'dbcman-module-after':(0x100474,0xB73D),'sio2d-module-after':(0x100494,0xB73E),'ds2ud-module-after':(0x1004B4,0xB73F),'libsd-module-after':(0x1004D4,0xB740),'sdrdrv-module-after':(0x1004F4,0xB741),'modhsyn-module-after':(0x100514,0xB742),'modsesq2-module-after':(0x100534,0xB743),'cdvdstm-module-after':(0x100554,0xB744),'sounds-module-after':(0x100574,0xB745),'cri-adxi-module-after':(0x100594,0xB746),'fileio-bind-after':(0x1005A4,0xB747),'heap-alloc-after':(0x3205D4,0xB748),'heap-copy-after':(0x320620,0xB749),'module-buffer-after':(0x320630,0xB74A),'heap-free-after':(0x320650,0xB74B),'dmac-startup-after':(0x1005B4,0xB74C)}
FRESH_BIND_STUB=0x01000000
CRI_CALL_INDEX=0

TEXTURE_POINTS={'texture-idle': (2668844, 46928), 'texture-path-idle': (2668872, 46929), 'texture-poll': (2668956, 46930), 'texture-after': (1050072, 46931)}

TEXTURE_POINTS['texture-complete']=(0x28B99C,0xB754)
TEXTURE_POINTS['main-priority-after']=(0x1005EC,0xB755)
TEXTURE_POINTS['ee-arena-after']=(0x1005F4,0xB756)
TEXTURE_POINTS['ee-pools-after']=(0x1005FC,0xB757)
TEXTURE_POINTS['heap-second-after']=(0x297938,0xB758)
TEXTURE_POINTS['sound-bind-after']=(0x314CA4,0xB759)
TEXTURE_POINTS['sound-init-after']=(0x3151BC,0xB75A)
TEXTURE_POINTS['sound-init-before']=(0x3151B4,0xB75B)
TEXTURE_POINTS['deci2-after']=(0x32F098,0xB75C)
TEXTURE_POINTS['sound-service-bind-after']=(0x297608,0xB75D)
TEXTURE_POINTS['sound-ee-thread-after']=(0x315218,0xB75E)
TEXTURE_POINTS['sound-ee-thread-before']=(0x315210,0xB75F)
TEXTURE_POINTS['sound-ee-status-after']=(0x32FDE0,0xB760)
TEXTURE_POINTS['sound-service-call-after']=(0x297544,0xB761)
TEXTURE_POINTS['rpc-dbc-bind-after']=(0x313254,0xB762)
TEXTURE_POINTS['rpc-dbc-second-bind-after']=(0x3132F4,0xB763)
TEXTURE_POINTS['rpc-dbc-command-before']=(0x313198,0xB764)
TEXTURE_POINTS['rpc-dbc-command-after']=(0x3131A0,0xB765)
TEXTURE_POINTS['rpc-dbc-binds-after']=(0x31338C,0xB766)
TEXTURE_POINTS['rpc-dbc-sema-after']=(0x3133B0,0xB767)
TEXTURE_POINTS['rpc-dbc-init-after']=(0x31341C,0xB768)
TEXTURE_POINTS['rpc-dbc-setup-before']=(0x31353C,0xB769)
TEXTURE_POINTS['rpc-dbc-setup-after']=(0x313544,0xB76A)
TEXTURE_POINTS['rpc-dbc-next-before']=(0x313648,0xB76B)
TEXTURE_POINTS['rpc-dbc-next-after']=(0x313650,0xB76C)
TEXTURE_POINTS['rpc-dbc-port1-before']=(0x313648,0xB76D)
TEXTURE_POINTS['rpc-dbc-slots-after']=(0x28BD88,0xB76E)
TEXTURE_POINTS['rpc-mc-bind-after']=(0x31D360,0xB770)
TEXTURE_POINTS['rpc-mc-command-before']=(0x31D3CC,0xB771)
TEXTURE_POINTS['rpc-mc-command-after']=(0x31D3D4,0xB772)
TEXTURE_POINTS['rpc-mc-query-before']=(0x31DEDC,0xB773)
TEXTURE_POINTS['rpc-mc-callback-before']=(0x31DD60,0xB774)
TEXTURE_POINTS['rpc-disc-init-after']=(0x31F104,0xB775)
TEXTURE_POINTS['rpc-disc-bind-sema']=(0x332814,0xB776)
TEXTURE_POINTS['rpc-disc-bind-after']=(0x31F15C,0xB777)
TEXTURE_POINTS['rpc-disc-command-before']=(0x31F278,0xB778)
TEXTURE_POINTS['rpc-disc-command-after']=(0x31F280,0xB779)
TEXTURE_POINTS['rpc-ncmd-bind-after']=(0x31F48C,0xB77A)
TEXTURE_POINTS['rpc-ncmd-before']=(0x320148,0xB77B)
TEXTURE_POINTS['rpc-ncmd-after']=(0x320150,0xB77C)
TEXTURE_POINTS['rpc-ncmd-bind-sema']=(0x332814,0xB77D)
TEXTURE_POINTS['rpc-ncmd-call-sema']=(0x332AD0,0xB77E)
TEXTURE_POINTS['rpc-cri-bind-before']=(0x2FD02C,0xB780)
TEXTURE_POINTS['rpc-cri-bind-after']=(0x2FD034,0xB781)
TEXTURE_POINTS['rpc-cri-bind-sema']=(0x332814,0xB782)
TEXTURE_POINTS['rpc-cri-command-before']=(0x332970,0xB783)
TEXTURE_POINTS['rpc-cri-command-after']=(0x2FCB20,0xB784)
TEXTURE_POINTS['rpc-cri-second-before']=(0x332970,0xB785)
TEXTURE_POINTS['rpc-cri-second-after']=(0x2FD1F8,0xB786)
TEXTURE_POINTS['rpc-cri-call-sema']=(0x332AD0,0xB787)
TEXTURE_POINTS['rpc-cri-third-before']=(0x332970,0xB788)
TEXTURE_POINTS['rpc-cri-fourth-before']=(0x332970,0xB78A)
TEXTURE_POINTS['rpc-cri-fifth-before']=(0x332970,0xB78B)
TEXTURE_POINTS['rpc-cri-sixth-before']=(0x332970,0xB78C)
TEXTURE_POINTS['rpc-cri-nth-before']=(0x332970,0xB78D)
TEXTURE_POINTS['rpc-cache-after']=(0x2FCD8C,0xB789)
TEXTURE_POINTS['rpc-cache-invalidate-after']=(0x2FCDA4,0xB78B)

def cache_capture_segment(terminal_words,point):
    """Observe original TagLo on both ways; keep MFC0/AND and resume at ADDU.

    Diagnostic data lives in the added segment, away from retail globals.
    k0/k1 are diagnostic scratch; interrupts are disabled by the original wrapper.
    No cache result or RPC completion is injected.
    """
    payload=bytearray(0xA00)
    struct.pack_into('<128I',payload,0x800,*([0xDEADBEEF]*128))
    struct.pack_into('<'+str(len(terminal_words))+'I',payload,0,*terminal_words)
    resumes=(0x32F4D4,0x32F510) if point=='rpc-cache-after' else (0x32F614,0x32F650)
    for stub_offset,way,resume in ((0x100,0,resumes[0]),(0x180,1,resumes[1])):
        # MFC0 v0,TagLo; index byte offset=(a2>>4); SW raw tag; original AND.
        words=[0x4002E000,0x3C1A0100,0x375A0800,0x0006D902,0x035BD021,
               0xAF420000|(way*0x100),0x00471024,0x08000000|(resume>>2),0]
        struct.pack_into('<9I',payload,stub_offset,*words)
    return bytes(payload)

def texture_stub(point):
    if point.startswith(('sound-','rpc-')) or point=='deci2-after':
        words=[0x3C180038,0x37187E80,0xFF020008,0xAF1D0010,0xAF040018,0xAF05001C,0xAF060020,0xAF070024,0x34190000|TEXTURE_POINTS[point][1],0xAF190000]
        if point.startswith('sound-ee-') or point in ('rpc-cache-after','rpc-cache-invalidate-after'):
            words[-2:-2]=[0x40196000,0xAF190028] # MFC0 Status; publish before marker.
        loop=len(words);words += [0x08000000|((FRESH_BIND_STUB+loop*4)>>2),0]
        if point=='rpc-dbc-port1-before':
            prefix=[0x3C18003A,0x8F1904C8,0,0,0]
            words=prefix+words;words[-2]=0x08000000|((FRESH_BIND_STUB+(loop+5)*4)>>2)
            early=len(words);words += [0x3C1F0031,0x37FF3650,0x08000000|(0x332970>>2),0x0000582D]
            words[3]=0x13200000|(early-4)
        if point=='sound-ee-status-after':
            words=[0x24180003,0,0]+words
            words[-2]=0x08000000|((FRESH_BIND_STUB+(loop+3)*4)>>2)
            early=len(words);words += [0x0040802D,0x06000003,0x8FA30000,0x08000000|(0x32FDFC>>2),0,0x08000000|(0x32FDEC>>2),0]
            words[1]=0x16380000|(early-2) # Preserve earlier ID-2 StartThread.
        if point in ('rpc-disc-bind-sema','rpc-ncmd-bind-sema','rpc-ncmd-call-sema','rpc-cri-bind-sema','rpc-cri-call-sema'):
            client=0x355C70 if point in ('rpc-cri-bind-sema','rpc-cri-call-sema','rpc-cri-third-before') else 0x3A8780 if point=='rpc-disc-bind-sema' else 0x369590
            words=[0x3C180000|(client>>16),0x37180000|(client&65535),0,0]+words
            words[-2]=0x08000000|((FRESH_BIND_STUB+(loop+4)*4)>>2)
            early=len(words);fall,taken=(0x332AD8,0x332AE8) if point in ('rpc-ncmd-call-sema','rpc-cri-call-sema','rpc-cri-third-before') else (0x33281C,0x33282C)
            words += [0x04410003,0xAE220008,0x08000000|(fall>>2),0,0x08000000|(taken>>2),0]
            words[2]=0x16380000|(early-3) # Earlier clients execute the original branch/delay slot.
        if point in ('rpc-cri-command-before','rpc-cri-second-before','rpc-cri-third-before','rpc-cri-fourth-before','rpc-cri-fifth-before','rpc-cri-sixth-before','rpc-cri-nth-before'):
            # Other clients execute the displaced prologue exactly once.
            words[8:8]=[0xAF080028,0xAF09002C,0xAF0A0030,0xAF0B0034,0xAF1F0038]
            loop+=5
            words=[0x3C180035,0x37185C70,0,0]+words
            words[-2]=0x08000000|((FRESH_BIND_STUB+(loop+4)*4)>>2)
            early=len(words)
            words += [0x27BDFF40,0xFFB10030,0x08000000|(0x332978>>2),0]
            words[2]=0x14980000|(early-3)
            if point in ('rpc-cri-second-before','rpc-cri-third-before','rpc-cri-fourth-before','rpc-cri-fifth-before','rpc-cri-sixth-before','rpc-cri-nth-before'):
                # Count only CRI calls in cleared EE diagnostic scratch.
                # First call continues through the original prologue.
                words[4:4]=[0x3C180038,0x37187E80,0x8F19003C,0x27390001,0xAF19003C,0x24180000|(CRI_CALL_INDEX if point=='rpc-cri-nth-before' else 6 if point=='rpc-cri-sixth-before' else 5 if point=='rpc-cri-fifth-before' else 4 if point=='rpc-cri-fourth-before' else 3 if point=='rpc-cri-third-before' else 2),0,0]
                early+=8
                words[2]=0x14980000|(early-3)
                words[10]=0x17380000|(early-11)
                halt=len(words)-6
                words[halt]=0x08000000|((FRESH_BIND_STUB+halt*4)>>2)
        if point in ('rpc-cache-after','rpc-cache-invalidate-after'):
            return cache_capture_segment(words,point)
        return struct.pack('<'+str(len(words))+'I',*words)
    words=([0x30580100,0,0] if point=="texture-complete" else [])+[0x3C180038,0x37187E80,0xFF020008,0xAF1D0010]
    for address,offset in [(0x1000A000,24),(0x1000A010,28),(0x1000A020,32),(0x10003020,36)]:
        words += [0x3C190000|(address>>16),0x37390000|(address&65535),0x8F220000,0xAF020000|offset]
    words += [0x34190000|TEXTURE_POINTS[point][1],0xAF190000];loop=len(words);words += [0x08000000|((FRESH_BIND_STUB+loop*4)>>2),0]
    if point=="texture-complete":
        early=len(words);words += [0x30420100,0x08000000|(0x28B9A0>>2),0];words[1]=0x17000000|(early-2)
    return struct.pack("<"+str(len(words))+"I",*words)

def vif_startup_stub():
    words=[0x3C180038,0x37187E80,0xFF020008,0xAF1D0010]
    for n,address in enumerate([0x10003C00+i*16 for i in range(15)]+[0x10003000,0x10003020]):
        words += [0x3C190000|(address>>16),0x37390000|(address&65535),0x8F220000,0xAF020000|(24+n*4)]
    words += [0x4842E000,0xAF02005C,0x3419B74D,0xAF190000]
    loop=len(words);words += [0x08000000|((FRESH_BIND_STUB+loop*4)>>2),0]
    return struct.pack('<'+str(len(words))+'I',*words)

def graph_csr_stub():
    words=[0x3C1801FF,0x3718ECF0,0,0,0x3C180038,0x37187E80,0xFF020008,0xAF1D0010,0x3419B74F,0xAF190000]
    loop=len(words);words += [0x08000000|((FRESH_BIND_STUB+loop*4)>>2),0]
    early=len(words);words += [0xA6120002,0x08000000|(0x30FE58>>2),0]
    words[2]=0x17B80000|(early-3)
    return struct.pack('<'+str(len(words))+'I',*words)

def graph_startup_stub():
    words=[0x3C180038,0x37187E80,0xFF020008,0xAF1D0010]
    for address,offset in [(0x12001000,24),(0x12001010,32)]:
        words += [0x3C190000|(address>>16),0x37390000|(address&65535),0xDF220000,0xFF020000|offset]
    words += [0x3419B74E,0xAF190000];loop=len(words);words += [0x08000000|((FRESH_BIND_STUB+loop*4)>>2),0]
    return struct.pack('<'+str(len(words))+'I',*words)

def fresh_bind_stub(success=False,scmd=False):
    # Earlier invocation resumes displaced BGEZL with its conditional delay slot.
    words=[0x3C1A003A,0x375A95A0,0x8F5A0000,0x3C1B0001,0x377BE640,0,0]
    if success:
        words += [0x3C1A0036 if scmd else 0x3C1A003A,0x375A9F64 if scmd else 0x375A87CC,0x8F5A0000,0,0]
    words += [0x3C180038,0x37187E80,0xFF020008,0xAF1D0010,0x34190000|(0xB736 if scmd else (0xB734 if success else 0xB733)),0xAF190000]
    loop=len(words); words += [0x08000000|((FRESH_BIND_STUB+loop*4)>>2),0]
    early=len(words)
    words += [0x04430003 if scmd else 0x04A30003,0x8E020024 if scmd else 0x8E220024,0x08000000|((0x31F794 if scmd else 0x31F990)>>2),0,0x08000000|((0x31F7DC if scmd else 0x31F9D4)>>2),0]
    words[5]=0x175B0000|(early-6)
    if success:words[10]=0x13400000|(early-11)
    return struct.pack('<'+str(len(words))+'I',*words)

def capture_trap(point):
    if point in TEXTURE_POINTS:return TEXTURE_POINTS[point][0],struct.pack("<2I",0x08000000|(FRESH_BIND_STUB>>2),0),TEXTURE_POINTS[point][1]
    if point=='graph-csr-after':return 0x30FE54,struct.pack('<2I',0x08000000|(FRESH_BIND_STUB>>2),0),0xB74F
    if point=='graph-startup-after':return 0x1005D0,struct.pack('<2I',0x08000000|(FRESH_BIND_STUB>>2),0),0xB74E
    if point=='vif-startup-after':return 0x1005BC,struct.pack('<2I',0x08000000|(FRESH_BIND_STUB>>2),0),0xB74D
    if point in LOADFILE_POINTS:
        address,marker=LOADFILE_POINTS[point]
        words=[0x3C180038,0x37187E80,0xFF020008,0xAF1D0010,0x34190000|marker,0xAF190000,0x08000000|((address+24)>>2),0]
        return address,struct.pack('<8I',*words),marker
    if point=='post-scmd-bind-success':return 0x31F78C,struct.pack('<2I',0x08000000|(FRESH_BIND_STUB>>2),0),0xB736
    if point in ('post-cd-bind-after','post-cd-bind-success','post-scmd-bind-success','vif-startup-after','graph-startup-after','graph-csr-after','texture-idle','texture-path-idle','texture-poll','texture-after','texture-complete','main-priority-after','ee-arena-after','ee-pools-after','heap-second-after','sound-bind-after','sound-init-after','sound-init-before','deci2-after','sound-service-bind-after','sound-ee-thread-after','sound-ee-thread-before','sound-ee-status-after','sound-service-call-after','rpc-dbc-bind-after','rpc-dbc-second-bind-after','rpc-dbc-command-before','rpc-dbc-command-after','rpc-dbc-binds-after','rpc-dbc-sema-after','rpc-dbc-init-after','rpc-dbc-setup-before','rpc-dbc-setup-after','rpc-dbc-next-before','rpc-dbc-next-after','rpc-dbc-port1-before','rpc-dbc-slots-after','rpc-mc-bind-after','rpc-mc-command-before','rpc-mc-command-after','rpc-mc-query-before','rpc-mc-callback-before','rpc-disc-init-after','rpc-disc-bind-sema','rpc-disc-bind-after','rpc-disc-command-before','rpc-disc-command-after','rpc-ncmd-bind-after','rpc-ncmd-before','rpc-ncmd-after','rpc-ncmd-bind-sema','rpc-ncmd-call-sema','rpc-cri-bind-before','rpc-cri-bind-after','rpc-cri-bind-sema','rpc-cri-command-before','rpc-cri-command-after','rpc-cri-second-before','rpc-cri-second-after','rpc-cri-call-sema','rpc-cri-third-before','rpc-cri-fourth-before','rpc-cri-fifth-before','rpc-cri-sixth-before','rpc-cri-nth-before','rpc-cache-after','rpc-cache-invalidate-after'):
        return 0x31F984,struct.pack('<2I',0x08000000|(FRESH_BIND_STUB>>2),0x0040282D),(0xB734 if point.endswith('success') else 0xB733)
    if point in ('post-sif-before','post-sif-after','post-mmode-after','post-cd-after'):
        address,marker={'post-sif-before':(0x1003E0,0xB730),'post-sif-after':(0x1003E8,0xB731),'post-cd-after':(0x1003F0,0xB732),'post-mmode-after':(0x1003F8,0xB735)}[point]
        # Unique caller after the GS helper; following startup is intentionally
        # replaced by the halt. Earlier shared SIF initialization remains intact.
        words=[0x3C180038,0x37187E80,0xFF020008,0xAF1D0010]
        for register,offset in ((0x1000E010,20),(0x1000C000,24),(0x1000C010,28),
                                (0x1000F230,32),(0x1000F200,36),(0x1000F210,40)):
            words += [0x3C190000|(register>>16),0x37390000|(register&65535),0x8F220000,0xAF020000|offset]
        words += [0x34190000|marker,0xAF190000,0x08000000|((address+120)>>2),0]
        return address,struct.pack('<32I',*words),marker

    if point in ('gif-sync-before','gif-sync-after','gif-sync-poll'):
        address,marker={'gif-sync-before':(0x310564,0xB72B),'gif-sync-after':(0x3105DC,0xB72C),'gif-sync-poll':(0x3105D0,0xB72D)}[point]
        words=[0x3C180038,0x37187E80,0xFF030008,0xAF1D0010]
        if point=='gif-sync-poll':words[2]=0xFF020008
        for register,offset in ((0x10009000,20),(0x1000A000,24),(0x1000A020,28),
                                (0x1000A010,32),(0x10003C00,36),(0x10003020,40)):
            words += [0x3C190000|(register>>16),0x37390000|(register&65535),0x8F220000,0xAF020000|offset]
        words += [0x34190000|marker,0xAF190000,0x08000000|((address+120)>>2),0]
        return address,struct.pack('<32I',*words),marker

    if point in ('gif-vu-idle','gif-path-after'):
        address,marker={'gif-vu-idle':(0x310630,0xB72E),'gif-path-after':(0x1002B8,0xB72F)}[point]
        words=[0x3C180038,0x37187E80,0xFF060008 if point=='gif-vu-idle' else 0xFF020008,
               0xAF1D0010,0x34190000|marker,0xAF190000,0x08000000|((address+24)>>2),0]
        return address,struct.pack('<8I',*words),marker
    if point=='gif-idle':
        address,marker=0x3100E8,0xB728
        words=[0x3C180038,0x37187E80,0xFF020008,0xAF1D0010,
               0x3C191000,0x3739A000,0x8F220000,0xAF020014,
               0x8F220020,0xAF020018,0x8F220010,0xAF02001C,
               0x3C191000,0x3739E000,0x8F220000,0xAF020020,
               0x8F220010,0xAF020024,0x3C191000,0x3739F520,
               0x8F220000,0xAF020028,0x34190000|marker,0xAF190000,
               0x08000000|((address+96)>>2),0]
        return address,struct.pack('<26I',*words),marker
    if point in ('gif-qwc','gif-after'):
        address,marker={'gif-qwc':(0x31014C,0xB729),'gif-after':(0x1002AC,0xB72A)}[point]
        words=[0x3C180038,0x37187E80,0xFF020008,0xAF1D0010,
               0x34190000|marker,0xAF190000,0x08000000|((address+24)>>2),0]
        return address,struct.pack('<8I',*words),marker
    if point in ('gs-intc-before','gs-intc-cleared','gs-intc-event','gs-intc-acknowledged'):
        # Halt before later paths execute; the event trap preserves the preceding polling loop.
        address,marker={'gs-intc-before':(0x32F10C,0xB724),
                        'gs-intc-cleared':(0x32F114,0xB725),
                        'gs-intc-event':(0x32F14C,0xB726),
                        'gs-intc-acknowledged':(0x32F168,0xB727)}[point]
        # Publish INTC_STAT, INTC_MASK, SP and the preceding v0 independently.
        words=[0x3C180038,0x37187E80,0xAF020014,0x3C191000,
               0x3739F000,0x8F220000,0xAF020008,0x8F220010,0xAF02000C,
               0xAF1D0010,0x34190000|marker,0xAF190000,
               0x08000000|((address+48)>>2),0]
        return address,struct.pack('<14I',*words),marker
    if point=='gs-intc-after':
        address,marker=0x3104DC,0xB722
        words=[0x3C191000,0x3739F000,0x8F390000,0x3C180038,0x37187E80,0xAF190008,0xAF00000C,0xAF1D0010,0x3419B722,0xAF190000,0x08000000|((address+40)>>2),0]
        return address,struct.pack('<12I',*words),marker
    if point in ('gs-csr-reset','gs-imr-after','gs-crt-after','gs-csr-field'):
        address,marker={'gs-csr-reset':(0x30FE54,0xB71F),'gs-imr-after':(0x30FE6C,0xB720),'gs-crt-after':(0x100274,0xB721),'gs-csr-field':(0x3104F8,0xB723)}[point]
        words=[0x3C180038,0x37187E80,0xFF020008,0xAF1D0010,
               0x34190000|marker,0xAF190000,0x08000000|((address+24)>>2),0]
        return address,struct.pack('<8I',*words),marker
    if point == 'reset-ready':
        # Ready branch only; eight words preserve the earlier not-ready return at 0x33814C.
        # v1 is the 0x40000 mask used by the preceding conditional, not full SMFLAG.
        address,marker=0x338128,0xB71E
        words=[0x3C180038,0x37187E80,0xAF030004,0xAF1D0008,
               0x34190000|marker,0xAF190000,0x08000000|((address+24)>>2),0]
        return address,struct.pack('<8I',*words),marker
    if point in ('reset-smflag-before','reset-smflag-after','reset-flags-after','reset-rpc-clear','reset-subaddr-clear','reset-cmd-clear'):
        address, marker = {'reset-smflag-before':(0x33806C,0xB717),'reset-smflag-after':(0x338078,0xB718),
                           'reset-flags-after':(0x3380A0,0xB71A),'reset-rpc-clear':(0x3380B0,0xB71B),
                           'reset-subaddr-clear':(0x3380BC,0xB71C),'reset-cmd-clear':(0x338094,0xB71D)}[point]
        words = [0x3C180038,0x37187E80,0xAF020004,0xAF1D0008,
                 0x3C19B000,0x3739F230,0x8F390000,0xAF19000C,
                 0x34190000|marker,0xAF190000,0x08000000|((address+40)>>2),0]
        return address,struct.pack('<12I',*words),marker
    if point == 'kernel-handler':
        return HANDLER, TRAP, 0xB702
    if point in ('sif-dma-before', 'sif-dma-after', 'sif-bind-after', 'rpc-call-sema', 'cd-init-after', 'cd-semaphores', 'scmd-bind-after', 'scmd-mmode-after', 'sif-stop-after', 'sif-reset-subaddr', 'reset-submit-after'):
        # SifSetDma boundary or caller resumed after first/change-address or bind submission.
        address, marker = {'sif-dma-before': (0x331D44, 0xB70C),
                           'sif-dma-after': (0x331D4C, 0xB70D),
                           'sif-bind-after': (0x33284C, 0xB70F),
                           'rpc-call-sema': (0x332AD0, 0xB710),
                           'cd-init-after': (0x332B0C, 0xB711),
                           'cd-semaphores': (0x31EE6C, 0xB712),
                           'scmd-bind-after': (0x31F78C, 0xB713),
                           'scmd-mmode-after': (0x32050C, 0xB714),
                           'sif-stop-after': (0x337FC0, 0xB715),
                           'sif-reset-subaddr': (0x337FC8, 0xB716),
                           'reset-submit-after': (0x338084, 0xB719)}[point]
        words = [0x3C180038, 0x37187E80, 0xAF020004, 0xAF1D0008,
                 0x34190000 | marker, 0xAF190000,
                 0x08000000 | ((address + 24) >> 2), 0]
        return address, struct.pack('<8I', *words), marker
    if point in ('sif-command', 'sif-rpc-state'):
        # Caller resumed after the first SifGetReg; publish return and selector.
        address, marker = (0x00331A34, 0xB70B) if point == 'sif-command' else (0x00332110, 0xB70E)
        words = [0x3C180038, 0x37187E80, 0xAF020004, 0xAF040008,
                 0x34190000 | marker, 0xAF190000,
                 0x08000000 | ((address + 24) >> 2), 0]
        return address, struct.pack('<8I', *words), marker
    if point == 'osd-readback':
        # First setter and second getter returned; capture original and readback.
        address, marker = 0x00338620, 0xB708
        words = [0x3C180038, 0x37187E80, 0x8FB90004, 0xAF190004,
                 0x8FB90000, 0xAF190008, 0x34190000 | marker, 0xAF190000,
                 0x08000000 | ((address + 32) >> 2), 0]
        return address, struct.pack('<10I', *words), marker
    if point == 'osd-config':
        # First GetOsdConfigParam has returned; publish its stack output word.
        address, marker = 0x003385F4, 0xB707
        words = [0x8FB90000, 0x3C180038, 0x37187E80, 0xAF190004,
                 0x34190000 | marker, 0xAF190000,
                 0x08000000 | ((address + 24) >> 2), 0]
        return address, struct.pack('<8I', *words), marker
    points = {'timer3-mode': (0x00338A1C, 0x1810, 0xB703),
              'timer2-mode': (0x00338C58, 0x1010, 0xB704),
              'intc-mask': (0x0032E9A4, 0xF010, 0xB705),
              'timer2-readback': (0x00338DD8, 0x1010, 0xB706),
              'dma-status': (0x003319CC, 0xE010, 0xB709),
              'dma5-control': (0x003319EC, 0xC000, 0xB70A)}
    if point not in points:
        raise ValueError('Unknown capture point.')
    address, register, marker = points[point]
    # Read the observed MMIO word once, publish it beside the marker, then spin.
    words = [0x3C181000, 0x37180000 | register, 0x8F190000, 0x3C180038, 0x37187E80,
             0xAF190004, 0x34190000 | marker, 0xAF190000,
             0x08000000 | ((address + 32) >> 2), 0]
    return address, struct.pack('<10I', *words), marker


def instrument(data, point='kernel-handler'):
    address, trap, marker = capture_trap(point)
    metadata = inspect(data)
    if metadata['sha256'] != EXPECTED_SHA:
        raise ValueError('Instrumentation requires the validated local BT2 executable hash.')
    matches = [seg for seg in metadata['load_segments']
               if seg['flags'] & 1 and seg['virtual_address'] <= address
               and address + len(trap) <= seg['virtual_address'] + seg['file_size']]
    if len(matches) != 1:
        raise ValueError('Handler must lie in exactly one executable file-backed segment.')
    seg = matches[0]
    offset = seg['file_offset'] + address - seg['virtual_address']
    if point=='sound-ee-status-after' and struct.unpack_from('<3I',data,offset)!=(0x0040802D,0x06010005,0x8FA30000):raise ValueError('EE thread query displaced control flow differs')
    if point=='rpc-disc-init-after' and struct.unpack_from('<2I',data,offset)!=(0x8E22840C,0x0441002C):raise ValueError('Disc initialization checkpoint differs')
    if point=='rpc-disc-bind-sema' and struct.unpack_from('<2I',data,offset)!=(0x04410005,0xAE220008):raise ValueError('Disc bind semaphore branch differs')
    if point=='rpc-ncmd-bind-sema' and struct.unpack_from('<2I',data,offset)!=(0x04410005,0xAE220008):raise ValueError('Command bind semaphore branch differs')
    if point=='rpc-ncmd-call-sema' and struct.unpack_from('<2I',data,offset)!=(0x4410005,0xae220008):raise ValueError('Command call semaphore branch differs')
    if point=='rpc-ncmd-bind-after' and struct.unpack_from('<2I',data,offset)!=(0x04430013,0x8E020024):raise ValueError('Command bind return differs')
    if point=='rpc-ncmd-before' and struct.unpack_from('<2I',data,offset)!=(0x0C0CCA5C,0x0000502D):raise ValueError('Command call differs')
    if point=='rpc-ncmd-after' and struct.unpack_from('<2I',data,offset)!=(0x04430008,0x8E8283D0):raise ValueError('Command return differs')
    if point=='rpc-disc-command-before' and struct.unpack_from('<2I',data,offset)!=(0x0C0CCA5C,0x0000582D):raise ValueError('Disc request call differs')
    if point=='rpc-disc-command-after' and struct.unpack_from('<2I',data,offset)!=(0x04410006,0x3C022000):raise ValueError('Disc request return differs')
    if point=='rpc-disc-bind-after' and struct.unpack_from('<2I',data,offset)!=(0x04430013,0x8E020024):raise ValueError('Disc bind return branch differs')
    cri_expected={'rpc-cri-nth-before':(0x27BDFF40,0xFFB10030),'rpc-cri-sixth-before':(0x27BDFF40,0xFFB10030),'rpc-cri-fifth-before':(0x27BDFF40,0xFFB10030),'rpc-cri-fourth-before':(0x27BDFF40,0xFFB10030),'rpc-cri-third-before':(0x27BDFF40,0xFFB10030),'rpc-cri-bind-before':(0x0C0CC9E4,0x0000302D),'rpc-cri-bind-after':(0x0443000C,0x8E220024),'rpc-cri-bind-sema':(0x04410005,0xAE220008),'rpc-cri-second-before':(0x27BDFF40,0xFFB10030),'rpc-cri-command-before':(0x27BDFF40,0xFFB10030),'rpc-cri-command-after':(0xDFBF0018,0x8E020000)}
    if point in cri_expected and struct.unpack_from('<2I',data,offset)!=cri_expected[point]:raise ValueError('CRI displaced instructions differ')
    if point=='rpc-cache-after' and struct.unpack_from('<2I',data,offset)!=(2382692364, 2382495760):raise ValueError('Cache caller differs')
    if point=='rpc-cache-invalidate-after' and struct.unpack_from('<2I',data,offset)!=(1006764080, 1006829616):raise ValueError('Cache invalidate caller differs')
    patched = bytearray(data)
    patched[offset:offset + len(trap)] = trap
    if point in ('post-cd-bind-after','post-cd-bind-success','post-scmd-bind-success','vif-startup-after','graph-startup-after','graph-csr-after','texture-idle','texture-path-idle','texture-poll','texture-after','texture-complete','main-priority-after','ee-arena-after','ee-pools-after','heap-second-after','sound-bind-after','sound-init-after','sound-init-before','deci2-after','sound-service-bind-after','sound-ee-thread-after','sound-ee-thread-before','sound-ee-status-after','sound-service-call-after','rpc-dbc-bind-after','rpc-dbc-second-bind-after','rpc-dbc-command-before','rpc-dbc-command-after','rpc-dbc-binds-after','rpc-dbc-sema-after','rpc-dbc-init-after','rpc-dbc-setup-before','rpc-dbc-setup-after','rpc-dbc-next-before','rpc-dbc-next-after','rpc-dbc-port1-before','rpc-dbc-slots-after','rpc-mc-bind-after','rpc-mc-command-before','rpc-mc-command-after','rpc-mc-query-before','rpc-mc-callback-before','rpc-disc-init-after','rpc-disc-bind-sema','rpc-disc-bind-after','rpc-disc-command-before','rpc-disc-command-after','rpc-ncmd-bind-after','rpc-ncmd-before','rpc-ncmd-after','rpc-ncmd-bind-sema','rpc-ncmd-call-sema','rpc-cri-bind-before','rpc-cri-bind-after','rpc-cri-bind-sema','rpc-cri-command-before','rpc-cri-command-after','rpc-cri-second-before','rpc-cri-second-after','rpc-cri-call-sema','rpc-cri-third-before','rpc-cri-fourth-before','rpc-cri-fifth-before','rpc-cri-sixth-before','rpc-cri-nth-before','rpc-cache-after','rpc-cache-invalidate-after'):
        expected=struct.pack('<2I',0x04430013,0x8E020024) if point=='post-scmd-bind-success' else struct.pack('<3I',0x0040282D,0x04A30012,0x8E220024)
        if point not in ('vif-startup-after','graph-startup-after','graph-csr-after','texture-idle','texture-path-idle','texture-poll','texture-after','texture-complete','main-priority-after','ee-arena-after','ee-pools-after','heap-second-after','sound-bind-after','sound-init-after','sound-init-before','deci2-after','sound-service-bind-after','sound-ee-thread-after','sound-ee-thread-before','sound-ee-status-after','sound-service-call-after','rpc-dbc-bind-after','rpc-dbc-second-bind-after','rpc-dbc-command-before','rpc-dbc-command-after','rpc-dbc-binds-after','rpc-dbc-sema-after','rpc-dbc-init-after','rpc-dbc-setup-before','rpc-dbc-setup-after','rpc-dbc-next-before','rpc-dbc-next-after','rpc-dbc-port1-before','rpc-dbc-slots-after','rpc-mc-bind-after','rpc-mc-command-before','rpc-mc-command-after','rpc-mc-query-before','rpc-mc-callback-before','rpc-disc-init-after','rpc-disc-bind-sema','rpc-disc-bind-after','rpc-disc-command-before','rpc-disc-command-after','rpc-ncmd-bind-after','rpc-ncmd-before','rpc-ncmd-after','rpc-ncmd-bind-sema','rpc-ncmd-call-sema','rpc-cri-bind-before','rpc-cri-bind-after','rpc-cri-bind-sema','rpc-cri-command-before','rpc-cri-command-after','rpc-cri-second-before','rpc-cri-second-after','rpc-cri-call-sema','rpc-cri-third-before','rpc-cri-fourth-before','rpc-cri-fifth-before','rpc-cri-sixth-before','rpc-cri-nth-before','rpc-cache-after','rpc-cache-invalidate-after') and data[offset:offset+len(expected)]!=expected:raise ValueError('Displaced bind branch differs')
        phoff=struct.unpack_from('<I',data,28)[0]; phsize,count=struct.unpack_from('<HH',data,42)
        empty=[phoff+n*phsize for n in range(count) if struct.unpack_from('<8I',data,phoff+n*phsize)[0]==1 and struct.unpack_from('<8I',data,phoff+n*phsize)[4:6]==(0,0)]
        if len(empty)!=1:raise ValueError('Expected one unused ELF load record')
        if any(x['virtual_address']<=FRESH_BIND_STUB<x['virtual_address']+x['memory_size'] for x in metadata['load_segments']):raise ValueError('Authored stub overlaps retail load')
        file_offset=(len(patched)+4095)&~4095; stub=texture_stub(point) if point in TEXTURE_POINTS else graph_csr_stub() if point=='graph-csr-after' else vif_startup_stub() if point=='vif-startup-after' else graph_startup_stub() if point=='graph-startup-after' else fresh_bind_stub(point.endswith('success'),point=='post-scmd-bind-success')
        if point in ('rpc-cache-after','rpc-cache-invalidate-after'):
            hooks=(0x32F4CC,0x32F508) if point=='rpc-cache-after' else (0x32F60C,0x32F648)
            for hook,target in ((hooks[0],0x100),(hooks[1],0x180)):
                hook_offset=seg['file_offset']+hook-seg['virtual_address']
                if struct.unpack_from('<2I',data,hook_offset)!=(0x4002E000,0x00471024):raise ValueError('Original TagLo/AND differs')
                struct.pack_into('<2I',patched,hook_offset,0x08000000|((FRESH_BIND_STUB+target)>>2),0)
        patched.extend(bytes(file_offset-len(patched))+stub)
        struct.pack_into('<8I',patched,empty[0],1,file_offset,FRESH_BIND_STUB,FRESH_BIND_STUB,len(stub),len(stub),7 if point in ('rpc-cache-after','rpc-cache-invalidate-after') else 5,4096)
    return bytes(patched), offset


def export_rva(data, symbol):
    """Read a named, non-forwarded PE32+ export without loading executable code."""
    def unpack(fmt, offset):
        if offset < 0 or offset + struct.calcsize(fmt) > len(data):
            raise ValueError('Truncated PE metadata.')
        return struct.unpack_from(fmt, data, offset)
    if data[:2] != b'MZ':
        raise ValueError('Expected PE executable.')
    pe, = unpack('<I', 0x3C)
    if data[pe:pe + 4] != b'PE\0\0':
        raise ValueError('Invalid PE signature.')
    machine, count = unpack('<HH', pe + 4)
    optsize, = unpack('<H', pe + 20)
    opt = pe + 24
    magic, = unpack('<H', opt)
    if machine != 0x8664 or magic != 0x20B or optsize < 120:
        raise ValueError('Expected Windows x64 PE32+ executable.')
    sections = [unpack('<IIII', opt + optsize + i * 40 + 8) for i in range(count)]
    def offset(rva, size=1):
        for virtual_size, va, raw_size, raw in sections:
            if va <= rva and rva + size <= va + raw_size:
                result = raw + rva - va
                if result + size <= len(data):
                    return result
        raise ValueError('PE RVA has no file-backed range.')
    er, es = unpack('<II', opt + 112)
    _, functions, names, fr, nr, ordinal_r = unpack('<6I', offset(er, 40) + 16)
    for i in range(names):
        name_r, = unpack('<I', offset(nr + i * 4, 4))
        start = offset(name_r)
        end = data.find(b'\0', start, start + 256)
        if end < 0:
            raise ValueError('Invalid export name.')
        if data[start:end] != symbol.encode('ascii'):
            continue
        ordinal, = unpack('<H', offset(ordinal_r + i * 2, 2))
        if ordinal >= functions:
            raise ValueError('Invalid export ordinal.')
        result, = unpack('<I', offset(fr + ordinal * 4, 4))
        if er <= result < er + es:
            raise ValueError('Forwarded export unsupported.')
        return result
    raise ValueError(f'PCSX2 does not export {symbol}.')


class ProcessReader:
    def __init__(self, pid, executable):
        self.kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        self.psapi = ctypes.WinDLL('psapi', use_last_error=True)
        self.kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        self.kernel.OpenProcess.restype = wintypes.HANDLE
        self.kernel.ReadProcessMemory.argtypes = [wintypes.HANDLE, ctypes.c_void_p,
                                                  ctypes.c_void_p, ctypes.c_size_t,
                                                  ctypes.POINTER(ctypes.c_size_t)]
        self.kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        self.psapi.EnumProcessModulesEx.argtypes = [wintypes.HANDLE, ctypes.c_void_p,
                                                    wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD]
        self.psapi.GetModuleFileNameExW.argtypes = [wintypes.HANDLE, wintypes.HMODULE,
                                                   wintypes.LPWSTR, wintypes.DWORD]
        self.pid=pid
        self.handle = self.kernel.OpenProcess(0x400 | 0x10, False, pid)
        if not self.handle:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            modules = (wintypes.HMODULE * 1024)()
            needed = wintypes.DWORD()
            if not self.psapi.EnumProcessModulesEx(self.handle, modules, ctypes.sizeof(modules),
                                                   ctypes.byref(needed), 3):
                raise ctypes.WinError(ctypes.get_last_error())
            path = ctypes.create_unicode_buffer(32768)
            if not self.psapi.GetModuleFileNameExW(self.handle, modules[0], path, len(path)):
                raise ctypes.WinError(ctypes.get_last_error())
            if Path(path.value).resolve() != executable.resolve():
                raise ValueError('Launched process module differs from requested PCSX2.')
            if not modules[0]:
                raise OSError("Main module is still initializing.")
            self.base = modules[0]
        except Exception:
            self.close()
            raise

    def read(self, address, size):
        buffer = ctypes.create_string_buffer(size)
        got = ctypes.c_size_t()
        if not self.kernel.ReadProcessMemory(self.handle, address, buffer, size, ctypes.byref(got)):
            raise ctypes.WinError(ctypes.get_last_error())
        if got.value != size:
            raise ValueError('Incomplete process read.')
        return buffer.raw

    def close(self):
        if self.handle:
            self.kernel.CloseHandle(self.handle)
            self.handle = None


def validate_disc_observer_modes(args):
    if not (args.capture_disc_api or args.capture_disc_type):return
    modes=[name for name,value in vars(args).items() if name.startswith('capture_') and name!='capture_point' and value]
    if len(modes)!=1:raise ValueError('Choose one isolated disc observer mode')
    if not args.disc or not args.absent_cards or args.capture_point not in ('rpc-ncmd-before','rpc-ncmd-call-sema','rpc-cri-bind-before','rpc-cri-bind-after','rpc-cri-bind-sema','rpc-cri-command-before','rpc-cri-command-after','rpc-cri-second-before','rpc-cri-second-after','rpc-cri-call-sema','rpc-cri-third-before','rpc-cri-fourth-before','rpc-cri-fifth-before','rpc-cri-sixth-before','rpc-cri-nth-before','rpc-cache-after','rpc-cache-invalidate-after'):
        raise ValueError('Disc observer requires an isolated local-disc command checkpoint')
    if args.capture_disc_api and args.capture_point=='rpc-ncmd-call-sema' and args.capture_disc_api not in ('sc-media','sc-read2'):
        raise ValueError('Readiness checkpoint supports only reached readiness API observers')

def main():
    global CRI_CALL_INDEX
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pcsx2', type=Path, required=True)
    parser.add_argument('--bios', type=Path, required=True)
    parser.add_argument('--capture-point', choices=['kernel-handler', 'timer3-mode', 'timer2-mode', 'intc-mask', 'timer2-readback', 'osd-config', 'osd-readback', 'dma-status', 'dma5-control', 'sif-command', 'sif-dma-before', 'sif-dma-after', 'sif-rpc-state', 'sif-bind-after', 'rpc-call-sema', 'cd-init-after', 'cd-semaphores', 'scmd-bind-after', 'scmd-mmode-after', 'sif-stop-after', 'sif-reset-subaddr', 'reset-smflag-before', 'reset-smflag-after', 'reset-submit-after', 'reset-flags-after', 'reset-rpc-clear', 'reset-subaddr-clear', 'reset-cmd-clear', 'reset-ready', 'gs-csr-reset', 'gs-imr-after', 'gs-crt-after', 'gs-intc-after', 'gs-csr-field', 'gs-intc-before', 'gs-intc-cleared', 'gs-intc-event', 'gs-intc-acknowledged','gif-idle','gif-qwc','gif-after','gif-sync-before','gif-sync-after','gif-sync-poll','gif-vu-idle','gif-path-after','post-sif-before','post-sif-after','post-scmd-bind-success','post-mmode-after','post-cd-after','post-cd-bind-after','post-cd-bind-success','loadfile-bind-after','loadfile-version-after','loadfile-module-before','loadfile-module-after','mcman-module-after','mcserv-module-after','dbcman-module-after','sio2d-module-after','ds2ud-module-after','libsd-module-after','sdrdrv-module-after','modhsyn-module-after','modsesq2-module-after','cdvdstm-module-after','sounds-module-after','cri-adxi-module-after','fileio-bind-after','heap-alloc-after','heap-copy-after','module-buffer-after','heap-free-after','dmac-startup-after','vif-startup-after','graph-startup-after','graph-csr-after','texture-idle','texture-path-idle','texture-poll','texture-after','texture-complete','main-priority-after','ee-arena-after','ee-pools-after','heap-second-after','sound-bind-after','sound-init-after','sound-init-before','deci2-after','sound-service-bind-after','sound-ee-thread-after','sound-ee-thread-before','sound-ee-status-after','sound-service-call-after','rpc-dbc-bind-after','rpc-dbc-second-bind-after','rpc-dbc-command-before','rpc-dbc-command-after','rpc-dbc-binds-after','rpc-dbc-sema-after','rpc-dbc-init-after','rpc-dbc-setup-before','rpc-dbc-setup-after','rpc-dbc-next-before','rpc-dbc-next-after','rpc-dbc-port1-before','rpc-dbc-slots-after','rpc-mc-bind-after','rpc-mc-command-before','rpc-mc-command-after','rpc-mc-query-before','rpc-mc-callback-before','rpc-disc-init-after','rpc-disc-bind-sema','rpc-disc-bind-after','rpc-disc-command-before','rpc-disc-command-after','rpc-ncmd-bind-after','rpc-ncmd-before','rpc-ncmd-after','rpc-ncmd-bind-sema','rpc-ncmd-call-sema','rpc-cri-bind-before','rpc-cri-bind-after','rpc-cri-bind-sema','rpc-cri-command-before','rpc-cri-command-after','rpc-cri-second-before','rpc-cri-second-after','rpc-cri-call-sema','rpc-cri-third-before','rpc-cri-fourth-before','rpc-cri-fifth-before','rpc-cri-sixth-before','rpc-cri-nth-before','rpc-cache-after','rpc-cache-invalidate-after'], default='kernel-handler')
    parser.add_argument('--disc', type=Path, help='Explicit user-local ISO mounted alongside the instrumented ELF.')
    parser.add_argument('--pad-poll-read-count',type=int,choices=[5,10],default=5)
    parser.add_argument('--capture-first-pad-poll',action='store_true',help='Instrument the first IOP FIFO receive after SIO2D return; isolated interpreter profile only.')
    parser.add_argument('--capture-spu2-key-readback',choices=['on','off'],help='Terminal first LIBSD key-on readback after SOUNDS return.')
    parser.add_argument('--capture-spu2-initialized',action='store_true',help='Terminal LIBSD sceSdInit return and register snapshot after SOUNDS return.')
    parser.add_argument('--cri-call-index',type=int,help='Terminal nth CRI request, 1..64, at rpc-cri-nth-before.')
    parser.add_argument('--capture-cri-thread',choices=['start','dma','adma','program','transfer','irq','volume'],nargs='?',const='start',help='Observe CRI StartThread and first WaitSema at third command.')
    parser.add_argument('--capture-cri-low-read',action='store_true',help='Terminal reached CRI low-memory argument read after SOUNDS return.')
    parser.add_argument('--capture-iop-card-alarm',action='store_true',help='Capture initial real card SetAlarm setup/return.')
    parser.add_argument('--capture-iop-card-fifo',action='store_true',help='Capture the first absent-card byte FIFO packet.')
    parser.add_argument('--capture-iop-ee-bind',action='store_true',help='Diagnostic actual bind to the registered EE sound callback server.')
    parser.add_argument('--capture-iop-delay',action='store_true',help='Capture actual initial MCMAN timed wait and return.')
    parser.add_argument('--capture-mc-sio2-flow',action='store_true',help='Capture absent-card control writes and secondary SIF descriptor.')
    parser.add_argument('--capture-mc-sio2-start',action='store_true',help='Capture actual first card send/receive DMA START readbacks.')
    parser.add_argument('--capture-mc-sio2-slice',action='store_true',help='Capture actual initial memory-card slice setup before DMA starts.')
    parser.add_argument('--capture-disc-api',choices=['sc-media','sc-read2','sc-key','sc-dual','sc-lsn','sc-timeout','search-afs','search-afs2','search-op','search-ed','search-logo','read-final','read-completion','read-tail','read-disable','read-alarm-cancel','read-alarm','read-transfer-poll','read-transfer','read-callback-after','read-callback','read-rv','read','sync-after','sync','error'],help='Observe a reached original CDVDFSV API return.')
    parser.add_argument('--capture-disc-type',action='store_true',help='Observe first original CDVDFSV disc-type register read.')
    parser.add_argument('--capture-disc-dma',action='store_true',help='Capture actual first CDVDFSV metadata DMA/poll.')
    parser.add_argument('--capture-dbc-dma',action='store_true',help='Capture the first actual DBCMAN reverse DMA and its completion poll.')
    parser.add_argument('--capture-spu2-transfer',action='store_true',help='Terminal first LIBSD transfer poll and sound RAM readback after SOUNDS return.')
    parser.add_argument('--capture-spu2-interrupts',action='store_true',help='Record three linked LIBSD interrupt disables after SOUNDS return.')
    parser.add_argument('--capture-spu2-status',action='store_true',help='Terminal first LIBSD status read capture after SOUNDS return.')
    parser.add_argument('--absent-cards',action='store_true',help='Disable both cards in a separate isolated capture profile.')
    parser.add_argument('--timeout', type=float, default=40)
    args = parser.parse_args()
    if args.capture_point=='rpc-cri-nth-before':
        if args.cri_call_index is None or not 1<=args.cri_call_index<=64 or not args.disc or not args.absent_cards:raise ValueError('Nth CRI capture requires 1..64, local disc and absent cards')
        CRI_CALL_INDEX=args.cri_call_index
    elif args.cri_call_index is not None:raise ValueError('CRI call index requires nth checkpoint')
    validate_disc_observer_modes(args)
    if sys.platform != 'win32' or struct.calcsize('P') != 8:
        parser.error('Requires 64-bit Windows Python.')
    executable = args.pcsx2.resolve()
    exe_data = executable.read_bytes()
    rva = export_rva(exe_data, 'EEmem')
    capture_iop = args.capture_point in ('sif-dma-before', 'sif-dma-after', 'sif-bind-after', 'cd-init-after', 'scmd-bind-after', 'scmd-mmode-after')
    capture_boot_iop = args.capture_point in ('reset-ready','post-sif-before','post-sif-after','post-scmd-bind-success','post-mmode-after','post-cd-after','post-cd-bind-after','post-cd-bind-success','loadfile-bind-after','loadfile-version-after','loadfile-module-before','loadfile-module-after','mcman-module-after','mcserv-module-after','dbcman-module-after','sio2d-module-after','ds2ud-module-after','libsd-module-after','sdrdrv-module-after','modhsyn-module-after','modsesq2-module-after','cdvdstm-module-after','sounds-module-after','cri-adxi-module-after','fileio-bind-after','heap-alloc-after','heap-copy-after','module-buffer-after','heap-free-after','dmac-startup-after','vif-startup-after','graph-startup-after','graph-csr-after','texture-idle','texture-path-idle','texture-poll','texture-after','texture-complete','main-priority-after','ee-arena-after','ee-pools-after','heap-second-after','sound-bind-after','sound-init-after','sound-init-before','deci2-after','sound-service-bind-after','sound-ee-thread-after','sound-ee-thread-before','sound-ee-status-after','sound-service-call-after','rpc-dbc-bind-after','rpc-dbc-second-bind-after','rpc-dbc-command-before','rpc-dbc-command-after','rpc-dbc-binds-after','rpc-dbc-sema-after','rpc-dbc-init-after','rpc-dbc-setup-before','rpc-dbc-setup-after','rpc-dbc-next-before','rpc-dbc-next-after','rpc-dbc-port1-before','rpc-dbc-slots-after','rpc-mc-bind-after','rpc-mc-command-before','rpc-mc-command-after','rpc-mc-query-before','rpc-mc-callback-before','rpc-disc-init-after','rpc-disc-bind-sema','rpc-disc-bind-after','rpc-disc-command-before','rpc-disc-command-after','rpc-ncmd-bind-after','rpc-ncmd-before','rpc-ncmd-after','rpc-ncmd-bind-sema','rpc-ncmd-call-sema','rpc-cri-bind-before','rpc-cri-bind-after','rpc-cri-bind-sema','rpc-cri-command-before','rpc-cri-command-after','rpc-cri-second-before','rpc-cri-second-after','rpc-cri-call-sema','rpc-cri-third-before','rpc-cri-fourth-before','rpc-cri-fifth-before','rpc-cri-sixth-before','rpc-cri-nth-before','rpc-cache-after','rpc-cache-invalidate-after')
    if (capture_boot_iop or args.capture_point in ('gs-csr-reset','gs-imr-after','gs-crt-after','gs-intc-after','gs-csr-field','gs-intc-before','gs-intc-cleared','gs-intc-event','gs-intc-acknowledged','gif-idle','gif-qwc','gif-after','gif-sync-before','gif-sync-after','gif-sync-poll','gif-vu-idle','gif-path-after','post-sif-before','post-sif-after','post-scmd-bind-success','post-mmode-after','post-cd-after','post-cd-bind-after','post-cd-bind-success','loadfile-bind-after','loadfile-version-after','loadfile-module-before','loadfile-module-after','mcman-module-after','mcserv-module-after','dbcman-module-after','sio2d-module-after','ds2ud-module-after','libsd-module-after','sdrdrv-module-after','modhsyn-module-after','modsesq2-module-after','cdvdstm-module-after','sounds-module-after','cri-adxi-module-after','fileio-bind-after','heap-alloc-after','heap-copy-after','module-buffer-after','heap-free-after','dmac-startup-after','vif-startup-after','graph-startup-after','graph-csr-after','texture-idle','texture-path-idle','texture-poll','texture-after','texture-complete','main-priority-after','ee-arena-after','ee-pools-after','heap-second-after','sound-bind-after','sound-init-after','sound-init-before','deci2-after','sound-service-bind-after','sound-ee-thread-after','sound-ee-thread-before','sound-ee-status-after','sound-service-call-after','rpc-dbc-bind-after','rpc-dbc-second-bind-after','rpc-dbc-command-before','rpc-dbc-command-after','rpc-dbc-binds-after','rpc-dbc-sema-after','rpc-dbc-init-after','rpc-dbc-setup-before','rpc-dbc-setup-after','rpc-dbc-next-before','rpc-dbc-next-after','rpc-dbc-port1-before','rpc-dbc-slots-after','rpc-mc-bind-after','rpc-mc-command-before','rpc-mc-command-after','rpc-mc-query-before','rpc-mc-callback-before','rpc-disc-init-after','rpc-disc-bind-sema','rpc-disc-bind-after','rpc-disc-command-before','rpc-disc-command-after','rpc-ncmd-bind-after','rpc-ncmd-before','rpc-ncmd-after','rpc-ncmd-bind-sema','rpc-ncmd-call-sema','rpc-cri-bind-before','rpc-cri-bind-after','rpc-cri-bind-sema','rpc-cri-command-before','rpc-cri-command-after','rpc-cri-second-before','rpc-cri-second-after','rpc-cri-call-sema','rpc-cri-third-before','rpc-cri-fourth-before','rpc-cri-fifth-before','rpc-cri-sixth-before','rpc-cri-nth-before','rpc-cache-after','rpc-cache-invalidate-after')) and not args.disc:parser.error('This capture point requires an explicit local disc')
    iop_rva = export_rva(exe_data, 'IOPmem') if capture_iop or capture_boot_iop else None
    source = ROOT / 'rom/SLUS_214.41'
    original = source.read_bytes()
    address, trap, marker = capture_trap(args.capture_point)
    patched, offset = instrument(original, args.capture_point)
    folder = ROOT / 'work/pcsx2-capture'
    if args.capture_point != 'kernel-handler':
        folder = folder / (args.capture_point+"-pad-poll-"+str(args.pad_poll_read_count) if args.capture_first_pad_poll else args.capture_point)
    if args.absent_cards:folder=folder.with_name(folder.name+'-absent-cards')
    if args.capture_spu2_key_readback:folder=folder.with_name(folder.name+'-spu2-key-readback-'+args.capture_spu2_key_readback)
    if args.capture_spu2_initialized:folder=folder.with_name(folder.name+'-spu2-initialized')
    if args.capture_iop_card_alarm:folder=folder.with_name(folder.name+'-card-alarm')
    if args.capture_iop_card_fifo:folder=folder.with_name(folder.name+'-card-fifo')
    if args.capture_iop_ee_bind:folder=folder.with_name(folder.name+'-iop-ee-bind')
    if args.capture_iop_delay:folder=folder.with_name(folder.name+'-iop-delay')
    if args.capture_mc_sio2_flow:folder=folder.with_name(folder.name+'-sio2-flow')
    if args.capture_mc_sio2_start:folder=folder.with_name(folder.name+'-sio2-start')
    if args.capture_mc_sio2_slice:folder=folder.with_name(folder.name+'-sio2-slice')
    if args.capture_disc_api:folder=folder.with_name(folder.name+'-disc-api-'+args.capture_disc_api)
    if args.capture_disc_type:folder=folder.with_name(folder.name+'-disc-type')
    if args.capture_disc_dma:folder=folder.with_name(folder.name+'-disc-dma')
    if args.capture_dbc_dma:folder=folder.with_name(folder.name+'-dbc-dma')
    if args.cri_call_index:folder=folder.with_name(folder.name+'-'+str(args.cri_call_index))
    if args.capture_cri_thread:folder=folder.with_name(folder.name+'-cri-thread-'+args.capture_cri_thread)
    if args.capture_cri_low_read:folder=folder.with_name(folder.name+'-cri-low-read')
    if args.capture_spu2_transfer:folder=folder.with_name(folder.name+'-spu2-transfer')
    if args.capture_spu2_interrupts:folder=folder.with_name(folder.name+'-spu2-interrupts')
    if args.capture_spu2_status:folder=folder.with_name(folder.name+'-spu2-status')
    # The user's portable.ini overrides -datapath. Launch a local package copy
    # without that marker; never change the user's installation/profile.
    package = folder / 'package'
    package.mkdir(parents=True, exist_ok=True)
    for dependency in [executable, *executable.parent.glob('*.dll')]:
        target = package / dependency.name
        if not target.exists() or hashlib.sha256(target.read_bytes()).digest() != hashlib.sha256(dependency.read_bytes()).digest():
            shutil.copyfile(dependency, target)
    for name in ('resources', 'QtPlugins'):
        shutil.copytree(executable.parent / name, package / name, dirs_exist_ok=True)
    qt_config = executable.parent / 'qt.conf'
    if qt_config.exists():
        shutil.copyfile(qt_config, package / 'qt.conf')
    executable = package / executable.name
    profile_root = folder / 'profile'
    profile = profile_root / 'PCSX2'
    for directory in ('inis', 'bios', 'logs', 'memcards', 'cache'):
        (profile / directory).mkdir(parents=True, exist_ok=True)
    bios_copy = profile / 'bios' / args.bios.name
    shutil.copyfile(args.bios, bios_copy)
    # PCSX2 may update NVM; any companion copies also stay in the isolated profile.
    for suffix in ('.NVM', '.MEC'):
        companion = args.bios.with_suffix(suffix)
        if companion.exists():
            shutil.copyfile(companion, bios_copy.with_suffix(suffix))
    settings = ('[UI]\nSettingsVersion = 1\nSetupWizardIncomplete = false\n'
                'StartPaused = false\nPauseOnFocusLoss = false\nConfirmShutdown = false\n'
                '[Folders]\nBios = bios\nLogs = logs\nMemoryCards = memcards\nCache = cache\n'
                f'[Filenames]\nBIOS = {bios_copy.name}\n'
                '[EmuCore]\nEnablePatches = false\nEnableCheats = false\n'
                'EnableWideScreenPatches = false\nEnableNoInterlacingPatches = false\n'
                'SaveStateOnShutdown = false\nEnableDiscordPresence = false\n'
                '[EmuCore/GS]\nRenderer = 11\n'
                '[SPU2/Output]\nOutputModule = nullout\n')
    if args.capture_cri_thread:
        if args.capture_point!='rpc-cri-third-before' or not args.disc or not args.absent_cards or any(v for k,v in vars(args).items() if k.startswith('capture_') and k not in ('capture_point','capture_cri_thread')):raise ValueError('CRI thread capture requires isolated third command')
        settings+='[EmuCore/CPU/Recompiler]\nEnableEE = false\nEnableIOP = false\n'
    if args.capture_spu2_status or args.capture_spu2_interrupts or args.capture_spu2_transfer or args.capture_spu2_key_readback or args.capture_spu2_initialized or args.capture_cri_low_read:
        if args.capture_point!='sounds-module-after' or args.capture_first_pad_poll or (sum(map(bool,[args.capture_spu2_status,args.capture_spu2_interrupts,args.capture_spu2_transfer,args.capture_spu2_key_readback,args.capture_spu2_initialized,args.capture_cri_low_read]))>1):raise ValueError('SPU2 status capture requires SOUNDS checkpoint without FIFO capture')
        settings+='[EmuCore/CPU/Recompiler]\nEnableEE = false\nEnableIOP = false\n'
    if args.absent_cards or args.capture_mc_sio2_start or args.capture_mc_sio2_flow or args.capture_iop_delay or args.capture_iop_ee_bind or args.capture_iop_card_fifo or args.capture_iop_card_alarm:
        settings+='[MemoryCards]\nSlot1_Enable = false\nSlot2_Enable = false\n'
    if args.capture_mc_sio2_slice or args.capture_mc_sio2_start or args.capture_mc_sio2_flow or args.capture_iop_delay or args.capture_iop_ee_bind or args.capture_iop_card_fifo or args.capture_iop_card_alarm:
        if sum(map(bool,[args.capture_mc_sio2_slice,args.capture_mc_sio2_start,args.capture_mc_sio2_flow,args.capture_iop_delay,args.capture_iop_ee_bind,args.capture_iop_card_fifo,args.capture_iop_card_alarm]))!=1:raise ValueError('Choose one card DMA capture mode')
        if args.capture_point!='rpc-mc-query-before' or args.capture_dbc_dma or args.capture_first_pad_poll:raise ValueError('Card slice capture requires query before checkpoint only')
        settings+='[EmuCore/CPU/Recompiler]\nEnableEE = false\nEnableIOP = false\n'
    if args.capture_disc_api:
        if args.capture_point not in ('rpc-ncmd-before','rpc-ncmd-call-sema','rpc-cri-bind-before','rpc-cri-bind-after','rpc-cri-bind-sema','rpc-cri-command-before','rpc-cri-command-after','rpc-cri-second-before','rpc-cri-second-after','rpc-cri-call-sema','rpc-cri-third-before','rpc-cri-fourth-before','rpc-cri-fifth-before','rpc-cri-sixth-before','rpc-cri-nth-before','rpc-cache-after','rpc-cache-invalidate-after') or not args.disc or not args.absent_cards:raise ValueError('Disc API requires isolated command profile')
        settings+='[EmuCore/CPU/Recompiler]\nEnableEE = false\nEnableIOP = false\n'
    if args.capture_disc_type:
        if args.capture_point not in ('rpc-ncmd-before','rpc-ncmd-call-sema','rpc-cri-bind-before','rpc-cri-bind-after','rpc-cri-bind-sema','rpc-cri-command-before','rpc-cri-command-after','rpc-cri-second-before','rpc-cri-second-after','rpc-cri-call-sema','rpc-cri-third-before','rpc-cri-fourth-before','rpc-cri-fifth-before','rpc-cri-sixth-before','rpc-cri-nth-before','rpc-cache-after','rpc-cache-invalidate-after') or not args.disc or not args.absent_cards:raise ValueError('Disc type requires isolated first command profile')
        settings+='[EmuCore/CPU/Recompiler]\nEnableEE = false\nEnableIOP = false\n'
    if args.capture_disc_dma:
        if not args.absent_cards or not args.disc or any(v for k,v in vars(args).items() if k.startswith('capture_') and k not in ('capture_point','capture_disc_dma')):raise ValueError('Disc DMA requires an isolated absent-card local disc profile')
        if args.capture_point!='rpc-disc-command-before':raise ValueError('Disc DMA requires first command before checkpoint')
        settings+='[EmuCore/CPU/Recompiler]\nEnableEE = false\nEnableIOP = false\n'
    if args.capture_dbc_dma:
        if args.capture_point!='rpc-dbc-setup-before':raise ValueError('DBCMAN DMA requires second-command before checkpoint')
        settings+='[EmuCore/CPU/Recompiler]\nEnableEE = false\nEnableIOP = false\n'
    if args.capture_first_pad_poll:
        if args.capture_point!='sio2d-module-after':raise ValueError('First poll capture requires SIO2D return checkpoint')
        settings+='[EmuCore/CPU/Recompiler]\nEnableEE = false\nEnableIOP = false\n'
    (profile / 'inis/PCSX2.ini').write_text(settings, encoding='utf-8')
    elf = folder / 'capture.elf'
    elf.write_bytes(patched)
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    command = [str(executable), '-datapath', str(profile_root), '-nogui', '-fastboot',
               '-nofullscreen', '-logfile', str(folder / 'pcsx2.log'), '-elf', str(elf)]
    if args.disc:
        command.append(str(args.disc.resolve()))
    process = subprocess.Popen(command, cwd=folder, startupinfo=startup,
                               creationflags=subprocess.CREATE_NO_WINDOW)
    reader = None
    try:
        deadline = time.monotonic() + args.timeout
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise RuntimeError(f'PCSX2 exited ({process.returncode}); inspect ignored pcsx2.log.')
            try:
                if reader is None:
                    reader = ProcessReader(process.pid, executable)
                pointer, = struct.unpack('<Q', reader.read(reader.base + rva, 8))
                if pointer and reader.read(pointer + MARKER, 4) == struct.pack('<I', marker):
                    break
            except OSError:
                pass  # Module/EE allocation can still be initializing.
            time.sleep(0.1)
        else:
            raise TimeoutError('Handler marker not reached; inspect ignored pcsx2.log.')
        log = (folder / 'pcsx2.log').read_text(encoding='utf-8', errors='replace')
        if f'DataRoot Directory: {profile}' not in log:
            raise ValueError('PCSX2 did not use the isolated data profile.')
        if args.capture_cri_thread:
            from capture_cri_thread import capture
            capture(reader,pointer,iop_rva,original,folder,address,args)
            return
        if args.capture_iop_card_alarm:
            from capture_card_alarm import capture
            capture(reader,pointer,iop_rva,original,folder,address,args)
            return
        if args.capture_iop_card_fifo:
            from capture_card_fifo import capture
            capture(reader,pointer,iop_rva,original,folder,address,args)
            return
        if args.capture_iop_ee_bind:
            from capture_iop_ee_bind import capture
            capture(reader,pointer,iop_rva,original,folder,address,args)
            return
        if args.capture_iop_delay:
            from capture_iop_delay import capture
            capture(reader,pointer,iop_rva,original,folder,address,args)
            return
        if args.capture_mc_sio2_flow:
            from capture_sio2_flow import capture
            capture(reader,pointer,iop_rva,original,folder,address,args)
            return
        if args.capture_mc_sio2_start:
            from capture_sio2_start import capture
            capture(reader,pointer,iop_rva,original,folder,address,args)
            return
        if args.capture_mc_sio2_slice:
            from capture_sio2_dma import capture
            capture(reader,pointer,iop_rva,original,folder,address,args)
            return
        if args.capture_disc_api:
            from capture_disc_api import capture
            capture(reader,pointer,iop_rva,original,folder,address,args)
            return
        if args.capture_disc_type:
            from capture_disc_type import capture
            capture(reader,pointer,iop_rva,original,folder,address,args)
            return
        if args.capture_disc_dma:
            from capture_disc_dma import capture
            capture(reader,pointer,iop_rva,original,folder,address,args)
            return
        if args.capture_dbc_dma:
            from capture_dbc_dma import capture
            capture(reader,pointer,iop_rva,original,folder,address,args)
            return
        if args.capture_spu2_key_readback:
            from capture_spu2_key_readback import capture
            capture(reader,pointer,iop_rva,original,folder,address,args)
            return
        if args.capture_spu2_initialized:
            from capture_spu2_initialized import capture
            capture(reader,pointer,iop_rva,original,folder,address,args)
            return
        if args.capture_cri_low_read:
            from capture_cri_low_read import capture
            capture(reader,pointer,iop_rva,original,folder,address,args)
            return
        if args.capture_spu2_transfer:
            from capture_spu2_transfer import capture
            capture(reader,pointer,iop_rva,original,folder,address,args)
            return
        if args.capture_spu2_interrupts:
            from capture_spu2_interrupts import capture
            capture(reader,pointer,iop_rva,original,folder,address,args)
            return
        if args.capture_spu2_status:
            from capture_spu2_status import capture
            capture(reader,pointer,iop_rva,original,folder,address,args)
            return
        if args.capture_first_pad_poll:
            from capture_pad_poll import capture
            capture(reader,pointer,iop_rva,original,folder,address,args)
            return
        if args.capture_point=='sound-init-after':
            completion_deadline=time.monotonic()+args.timeout;previous=None
            while True:
                current=tuple(reader.read(pointer+start,size) for start,size in [(0x3A5C80,40),(0x3A5C40,64),(0x368190,64)])
                if current==previous and struct.unpack_from('<I',current[0])[0]==0:break
                if time.monotonic()>completion_deadline:raise TimeoutError('Sound RPC remains pending/unstable after published caller return')
                previous=current;time.sleep(0.2)
        data = reader.read(pointer, 32 * 1024 * 1024)
        if data[address:address + len(trap)] != trap:
            raise ValueError('EE RAM instrumentation does not match.')
        time.sleep(0.1)
        if not capture_iop and args.capture_point not in ('sif-rpc-state', 'rpc-call-sema', 'cd-semaphores', 'sif-stop-after', 'sif-reset-subaddr', 'reset-smflag-before', 'reset-smflag-after', 'reset-submit-after', 'reset-flags-after', 'reset-rpc-clear', 'reset-subaddr-clear', 'reset-cmd-clear', 'reset-ready', 'gs-csr-reset', 'gs-imr-after', 'gs-crt-after', 'gs-intc-after', 'gs-csr-field', 'gs-intc-before', 'gs-intc-cleared', 'gs-intc-event', 'gs-intc-acknowledged','gif-idle','gif-qwc','gif-after','gif-sync-before','gif-sync-after','gif-sync-poll','gif-vu-idle','gif-path-after','post-sif-before','post-sif-after','post-scmd-bind-success','post-mmode-after','post-cd-after','post-cd-bind-after','post-cd-bind-success','loadfile-bind-after','loadfile-version-after','loadfile-module-before','loadfile-module-after','mcman-module-after','mcserv-module-after','dbcman-module-after','sio2d-module-after','ds2ud-module-after','libsd-module-after','sdrdrv-module-after','modhsyn-module-after','modsesq2-module-after','cdvdstm-module-after','sounds-module-after','cri-adxi-module-after','fileio-bind-after','heap-alloc-after','heap-copy-after','module-buffer-after','heap-free-after','dmac-startup-after','vif-startup-after','graph-startup-after','graph-csr-after','texture-idle','texture-path-idle','texture-poll','texture-after','texture-complete','main-priority-after','ee-arena-after','ee-pools-after','heap-second-after','sound-bind-after','sound-init-after','sound-init-before','deci2-after','sound-service-bind-after','sound-ee-thread-after','sound-ee-thread-before','sound-ee-status-after','sound-service-call-after','rpc-dbc-bind-after','rpc-dbc-second-bind-after','rpc-dbc-command-before','rpc-dbc-command-after','rpc-dbc-binds-after','rpc-dbc-sema-after','rpc-dbc-init-after','rpc-dbc-setup-before','rpc-dbc-setup-after','rpc-dbc-next-before','rpc-dbc-next-after','rpc-dbc-port1-before','rpc-dbc-slots-after','rpc-mc-bind-after','rpc-mc-command-before','rpc-mc-command-after','rpc-mc-query-before','rpc-mc-callback-before','rpc-disc-init-after','rpc-disc-bind-sema','rpc-disc-bind-after','rpc-disc-command-before','rpc-disc-command-after','rpc-ncmd-bind-after','rpc-ncmd-before','rpc-ncmd-after','rpc-ncmd-bind-sema','rpc-ncmd-call-sema','rpc-cri-bind-before','rpc-cri-bind-after','rpc-cri-bind-sema','rpc-cri-command-before','rpc-cri-command-after','rpc-cri-second-before','rpc-cri-second-after','rpc-cri-call-sema','rpc-cri-third-before','rpc-cri-fourth-before','rpc-cri-fifth-before','rpc-cri-sixth-before','rpc-cri-nth-before','rpc-cache-after','rpc-cache-invalidate-after') and reader.read(pointer, 0x80004) != data[:0x80004]:
            raise ValueError('Kernel prefix changed during capture.')
        if capture_iop:
            sp, = struct.unpack_from('<I', data, MARKER + 8)
            if sp > len(data) - 16 or reader.read(pointer + sp, 16) != data[sp:sp + 16]:
                raise ValueError('SIF DMA descriptor changed during capture.')
            packet_address, packet_size = (0x3A97C0, 64) if args.capture_point in ('sif-bind-after', 'cd-init-after', 'scmd-bind-after', 'scmd-mmode-after') else (0x3A9580, 20)
            if reader.read(pointer + packet_address, packet_size) != data[packet_address:packet_address + packet_size]:
                raise ValueError('SIF packet changed during capture.')
        if args.capture_point in ('rpc-call-sema', 'cd-semaphores'):
            sp, = struct.unpack_from('<I', data, MARKER + 8)
            if sp > len(data) - 24 or reader.read(pointer + sp, 24) != data[sp:sp + 24]:
                raise ValueError('RPC call semaphore descriptor changed during capture.')
            if reader.read(pointer + MARKER, 12) != data[MARKER:MARKER + 12]:
                raise ValueError('RPC call semaphore publication changed during capture.')
        if args.capture_point == 'cd-semaphores':
            if reader.read(pointer + 0x3683E8, 12) != data[0x3683E8:0x3683F4]:
                raise ValueError('CD semaphore IDs changed during capture.')
        if args.capture_point in ('sif-bind-after', 'cd-init-after', 'scmd-bind-after', 'scmd-mmode-after'):
            ranges = ((MARKER, 12), (0x3A94C0, 64), (0x3A87A8, 40), (0x3A95C0, 13 * 12))
            if args.capture_point in ('scmd-bind-after', 'scmd-mmode-after'):
                ranges += ((0x369F40, 40), (0x3695C0,4), (0x369A00,4))
            if args.capture_point == 'cd-init-after':
                ranges += ((0x3695C0, 16), (0x3A8800, 4))
            for start, size in ranges:
                if reader.read(pointer + start, size) != data[start:start + size]:
                    raise ValueError('RPC response/client/handler table changed during capture.')
        if args.capture_point == 'sif-rpc-state' and reader.read(pointer + MARKER, 12) != data[MARKER:MARKER + 12]:
            raise ValueError('SIF query publication changed during capture.')
        iop_data = None
        if capture_boot_iop:
            iop_pointer, = struct.unpack('<Q', reader.read(reader.base + iop_rva, 8))
            if not iop_pointer:raise ValueError('IOP memory is not allocated.')
            iop_data=reader.read(iop_pointer,2*1024*1024)
            if not args.capture_point.startswith(('sound-','rpc-')) and args.capture_point in ('post-sif-before','post-sif-after','post-scmd-bind-success','post-mmode-after','post-cd-after','post-cd-bind-after','post-cd-bind-success','loadfile-bind-after','loadfile-version-after','loadfile-module-before','loadfile-module-after','mcman-module-after','mcserv-module-after','dbcman-module-after','sio2d-module-after','ds2ud-module-after','libsd-module-after','sdrdrv-module-after','modhsyn-module-after','modsesq2-module-after','cdvdstm-module-after','sounds-module-after','cri-adxi-module-after','fileio-bind-after','heap-alloc-after','heap-copy-after','module-buffer-after','heap-free-after','dmac-startup-after','vif-startup-after','graph-startup-after','graph-csr-after','texture-idle','texture-path-idle','texture-poll','texture-after','texture-complete','main-priority-after','ee-arena-after','ee-pools-after','heap-second-after','sound-bind-after','sound-init-after','sound-init-before','deci2-after','sound-service-bind-after','sound-ee-thread-after','sound-ee-thread-before','sound-ee-status-after','sound-service-call-after','rpc-dbc-bind-after','rpc-dbc-second-bind-after','rpc-dbc-command-before','rpc-dbc-command-after','rpc-dbc-binds-after','rpc-dbc-sema-after','rpc-dbc-init-after','rpc-dbc-setup-before','rpc-dbc-setup-after','rpc-dbc-next-before','rpc-dbc-next-after','rpc-dbc-port1-before','rpc-dbc-slots-after','rpc-mc-bind-after','rpc-mc-command-before','rpc-mc-command-after','rpc-mc-query-before','rpc-mc-callback-before','rpc-disc-init-after','rpc-disc-bind-sema','rpc-disc-bind-after','rpc-disc-command-before','rpc-disc-command-after','rpc-ncmd-bind-after','rpc-ncmd-before','rpc-ncmd-after','rpc-ncmd-bind-sema','rpc-ncmd-call-sema','rpc-cri-bind-before','rpc-cri-bind-after','rpc-cri-bind-sema','rpc-cri-command-before','rpc-cri-command-after','rpc-cri-second-before','rpc-cri-second-after','rpc-cri-call-sema','rpc-cri-third-before','rpc-cri-fourth-before','rpc-cri-fifth-before','rpc-cri-sixth-before','rpc-cri-nth-before','rpc-cache-after','rpc-cache-invalidate-after'):
                for start,size in ((0x1E640,20),(0x1E700,16)):
                    if reader.read(iop_pointer+start,size)!=iop_data[start:start+size]:
                        raise ValueError('Fresh SIF IOP mailbox/ownership changed.')
            if args.capture_point in ('post-cd-after','post-cd-bind-after','post-cd-bind-success','post-mmode-after','post-scmd-bind-success'):
                for start,size in ((0x1E640,64),(0x1E700,16),(0x43388,68),(0x43358,12),(0x441B8,16)):
                    if reader.read(iop_pointer+start,size)!=iop_data[start:start+size]:raise ValueError('Fresh CD service state changed.')
                for start,size in ((MARKER,44),(0x3A94C0,64),(0x3A97C0,64),(0x3A87A8,40),(0x3A8800,4),(0x3695C0,16)):
                    if reader.read(pointer+start,size)!=data[start:start+size]:raise ValueError('Fresh CD response/client changed.')
            if args.capture_point in ('post-mmode-after','post-scmd-bind-success'):
                for start,size in ((0x369F40,40),(0x369A00,4)):
                    if reader.read(pointer+start,size)!=data[start:start+size]:raise ValueError('Fresh SCMD client changed.')
                server=struct.unpack_from('<I',data,0x369F64)[0]
                if not server or server>len(iop_data)-68:raise ValueError('Fresh SCMD server bounds differ')
                for start,size in ((server,68),(struct.unpack_from('<I',iop_data,server+8)[0],4)):
                    if start>len(iop_data)-size or reader.read(iop_pointer+start,size)!=iop_data[start:start+size]:raise ValueError('Fresh SCMD ownership changed.')
            if args.capture_point in LOADFILE_POINTS:
                client_address=0x3AC840 if args.capture_point in ('fileio-bind-after','heap-alloc-after','heap-copy-after','heap-free-after') else 0x3ACC00
                for start,size in ((MARKER,44),(client_address,40),(0x3ACA00,512),(0x3A94C0,64),(0x3A97C0,64)):
                    if reader.read(pointer+start,size)!=data[start:start+size]:raise ValueError('LOADFILE client/request changed.')
                server=struct.unpack_from('<I',data,client_address+36)[0]
                if not server or server>len(iop_data)-68:raise ValueError('LOADFILE server bounds differ')
                buffer=struct.unpack_from('<I',iop_data,server+8)[0]
                for start,size in ((server,68),(buffer,256 if args.capture_point in ('fileio-bind-after','heap-alloc-after','heap-copy-after','heap-free-after') else 512)):
                    if start>len(iop_data)-size or reader.read(iop_pointer+start,size)!=iop_data[start:start+size]:raise ValueError('LOADFILE service changed.')
            (folder / 'iop-ram.bin').write_bytes(iop_data)
            if reader.read(pointer+MARKER,12)!=data[MARKER:MARKER+12]:raise ValueError('Boot-ready publication changed.')
        if capture_iop:
            # Pinned PCSX2 MemoryTypes.h: Main is the first member of IOPmem.
            iop_pointer, = struct.unpack('<Q', reader.read(reader.base + iop_rva, 8))
            if not iop_pointer:
                raise ValueError('IOP memory is not allocated.')
            iop_data = reader.read(iop_pointer, 2 * 1024 * 1024)
            time.sleep(0.1)
            receive_size = 64 if args.capture_point in ('sif-bind-after', 'cd-init-after', 'scmd-bind-after', 'scmd-mmode-after') else 20
            for start, size in ((0x19600, receive_size), (0x196C0, 12)) + (((0x3F570, 68), (0x3F558, 12)) if args.capture_point in ('sif-bind-after', 'cd-init-after', 'scmd-bind-after', 'scmd-mmode-after') else ()) + (((0x3FB20, 16),) if args.capture_point == 'cd-init-after' else ()):
                if reader.read(iop_pointer + start, size) != iop_data[start:start + size]:
                    raise ValueError('IOP receive packet/table changed during capture.')
            if args.capture_point in ('scmd-bind-after','scmd-mmode-after'):
                server = struct.unpack_from('<I', data, 0x369F64)[0]
                if not server or server > len(iop_data) - 68 or reader.read(iop_pointer + server, 68) != iop_data[server:server + 68]:
                    raise ValueError('SCMD service changed during capture.')
                buffer = struct.unpack_from('<I', iop_data, server + 8)[0]
                if buffer > len(iop_data) - 4 or reader.read(iop_pointer + buffer,4) != iop_data[buffer:buffer+4] or reader.read(iop_pointer+0x3F648,68) != iop_data[0x3F648:0x3F68C]:
                    raise ValueError('SCMD buffer/queue changed during capture.')
            (folder / 'iop-ram.bin').write_bytes(iop_data)
        if args.capture_point in ('sif-stop-after','sif-reset-subaddr'):
            for start, size in ((MARKER,12),(0x6380,44),(0x14EAC,4)):
                if reader.read(pointer+start,size) != data[start:start+size]:
                    raise ValueError('SIF stop publication/kernel handler changed.')
        if args.capture_point in ('reset-smflag-before','reset-smflag-after','reset-flags-after','reset-rpc-clear','reset-subaddr-clear','reset-cmd-clear'):
            if reader.read(pointer+MARKER,16) != data[MARKER:MARKER+16]:
                raise ValueError('Reset SMFLAG publication changed.')
        if args.capture_point == 'reset-submit-after':
            for start,size in ((MARKER,12),(0x3ACC40,104)):
                if reader.read(pointer+start,size) != data[start:start+size]:
                    raise ValueError('Reboot submission publication/packet changed.')
        if args.capture_point in ('gs-csr-reset','gs-imr-after','gs-crt-after','gs-intc-after','gs-csr-field','gs-intc-before','gs-intc-cleared','gs-intc-event','gs-intc-acknowledged','gif-idle','gif-qwc','gif-after','gif-sync-before','gif-sync-after','gif-sync-poll','gif-vu-idle','gif-path-after','post-sif-before','post-sif-after','post-scmd-bind-success','post-mmode-after','post-cd-after','post-cd-bind-after','post-cd-bind-success','loadfile-bind-after','loadfile-version-after','loadfile-module-before','loadfile-module-after','mcman-module-after','mcserv-module-after','dbcman-module-after','sio2d-module-after','ds2ud-module-after','libsd-module-after','sdrdrv-module-after','modhsyn-module-after','modsesq2-module-after','cdvdstm-module-after','sounds-module-after','cri-adxi-module-after','fileio-bind-after','heap-alloc-after','heap-copy-after','module-buffer-after','heap-free-after','dmac-startup-after','vif-startup-after','graph-startup-after','graph-csr-after','texture-idle','texture-path-idle','texture-poll','texture-after','texture-complete','main-priority-after','ee-arena-after','ee-pools-after','heap-second-after','sound-bind-after','sound-init-after','sound-init-before','deci2-after','sound-service-bind-after','sound-ee-thread-after','sound-ee-thread-before','sound-ee-status-after','sound-service-call-after','rpc-dbc-bind-after','rpc-dbc-second-bind-after','rpc-dbc-command-before','rpc-dbc-command-after','rpc-dbc-binds-after','rpc-dbc-sema-after','rpc-dbc-init-after','rpc-dbc-setup-before','rpc-dbc-setup-after','rpc-dbc-next-before','rpc-dbc-next-after','rpc-dbc-port1-before','rpc-dbc-slots-after','rpc-mc-bind-after','rpc-mc-command-before','rpc-mc-command-after','rpc-mc-query-before','rpc-mc-callback-before','rpc-disc-init-after','rpc-disc-bind-sema','rpc-disc-bind-after','rpc-disc-command-before','rpc-disc-command-after','rpc-ncmd-bind-after','rpc-ncmd-before','rpc-ncmd-after','rpc-ncmd-bind-sema','rpc-ncmd-call-sema','rpc-cri-bind-before','rpc-cri-bind-after','rpc-cri-bind-sema','rpc-cri-command-before','rpc-cri-command-after','rpc-cri-second-before','rpc-cri-second-after','rpc-cri-call-sema','rpc-cri-third-before','rpc-cri-fourth-before','rpc-cri-fifth-before','rpc-cri-sixth-before','rpc-cri-nth-before','rpc-cache-after','rpc-cache-invalidate-after') and reader.read(pointer+MARKER,24)!=data[MARKER:MARKER+24]:raise ValueError('GS CSR publication changed.')
        if args.capture_point in ('gif-sync-before','gif-sync-after','gif-sync-poll'):
            if reader.read(pointer+MARKER,44)!=data[MARKER:MARKER+44]:
                raise ValueError('GIF synchronization publication changed.')
        if not args.capture_point.startswith(('sound-','rpc-')) and args.capture_point in ('post-sif-before','post-sif-after','post-scmd-bind-success','post-mmode-after','post-cd-after','post-cd-bind-after','post-cd-bind-success','loadfile-bind-after','loadfile-version-after','loadfile-module-before','loadfile-module-after','mcman-module-after','mcserv-module-after','dbcman-module-after','sio2d-module-after','ds2ud-module-after','libsd-module-after','sdrdrv-module-after','modhsyn-module-after','modsesq2-module-after','cdvdstm-module-after','sounds-module-after','cri-adxi-module-after','fileio-bind-after','heap-alloc-after','heap-copy-after','module-buffer-after','heap-free-after','dmac-startup-after','vif-startup-after','graph-startup-after','graph-csr-after','texture-idle','texture-path-idle','texture-poll','texture-after','texture-complete','main-priority-after','ee-arena-after','ee-pools-after','heap-second-after','sound-bind-after','sound-init-after','sound-init-before','deci2-after','sound-service-bind-after','sound-ee-thread-after','sound-ee-thread-before','sound-ee-status-after','sound-service-call-after','rpc-dbc-bind-after','rpc-dbc-second-bind-after','rpc-dbc-command-before','rpc-dbc-command-after','rpc-dbc-binds-after','rpc-dbc-sema-after','rpc-dbc-init-after','rpc-dbc-setup-before','rpc-dbc-setup-after','rpc-dbc-next-before','rpc-dbc-next-after','rpc-dbc-port1-before','rpc-dbc-slots-after','rpc-mc-bind-after','rpc-mc-command-before','rpc-mc-command-after','rpc-mc-query-before','rpc-mc-callback-before','rpc-disc-init-after','rpc-disc-bind-sema','rpc-disc-bind-after','rpc-disc-command-before','rpc-disc-command-after','rpc-ncmd-bind-after','rpc-ncmd-before','rpc-ncmd-after','rpc-ncmd-bind-sema','rpc-ncmd-call-sema','rpc-cri-bind-before','rpc-cri-bind-after','rpc-cri-bind-sema','rpc-cri-command-before','rpc-cri-command-after','rpc-cri-second-before','rpc-cri-second-after','rpc-cri-call-sema','rpc-cri-third-before','rpc-cri-fourth-before','rpc-cri-fifth-before','rpc-cri-sixth-before','rpc-cri-nth-before','rpc-cache-after','rpc-cache-invalidate-after'):
            for start,size in ((MARKER,44),(0x3A87A8,40),(0x3A94C0,64),(0x3A95C0,156),(0x3A9740,4)):
                if reader.read(pointer+start,size)!=data[start:start+size]:raise ValueError(f'Post-reset SIF publication/table changed at {start:#x}.')
        if args.capture_point=='gif-path-after':
            sp=struct.unpack_from('<I',data,MARKER+16)[0]
            if reader.read(pointer+sp+320,240)!=data[sp+320:sp+560]:
                raise ValueError('Second GIF packet changed.')
        if args.capture_point=='gif-idle':
            sp=struct.unpack_from('<I',data,MARKER+16)[0]
            packet=sp+96
            count=1+(struct.unpack_from('<Q',data,packet)[0]&0x7fff)
            if count!=15 or packet+count*16>len(data):raise ValueError('First GIF packet bounds/count differ.')
            for start,size in ((MARKER,44),(packet,count*16)):
                if reader.read(pointer+start,size)!=data[start:start+size]:raise ValueError('GIF publication/packet changed.')
        if args.capture_point=='ee-pools-after' and reader.read(pointer+0x38A628,16)!=data[0x38A628:0x38A638]:raise ValueError('EE pool pointers changed')
        if args.capture_point in ('rpc-cache-after','rpc-cache-invalidate-after'):
            for start,size in ((MARKER,44),(FRESH_BIND_STUB+0x800,512),(0x365BC0,0x880)):
                if reader.read(pointer+start,size)!=data[start:start+size]:raise ValueError('Cache observations changed')
        if args.capture_point in TEXTURE_POINTS:
            for start,size in ((MARKER,40),(0x1FFECF0,64)):
                if reader.read(pointer+start,size)!=data[start:start+size]:raise ValueError('Texture startup publication/source changed')
        if args.capture_point=='vif-startup-after' and reader.read(pointer+MARKER,96)!=data[MARKER:MARKER+96]:raise ValueError('VIF publication changed')
        if args.capture_point.startswith(('sound-','rpc-')):
            for start,size in [(0x3A5C80,40),(0x3A5C40,64),(0x368190,64)]:
                if reader.read(pointer+start,size)!=data[start:start+size]:raise ValueError('Sound RPC client/packet/result changed')
        if args.capture_point.startswith('rpc-disc-'):
            for start,size in [(0x3A8780,40),(0x3683D0,80)]:
                if reader.read(pointer+start,size)!=data[start:start+size]:raise ValueError('Disc client/state changed')
            if args.capture_point=='rpc-disc-bind-sema':
                sp=struct.unpack_from('<I',data,MARKER+16)[0]
                if reader.read(pointer+sp,24)!=data[sp:sp+24]:raise ValueError('Disc semaphore descriptor changed')
        if args.capture_point=='sound-ee-status-after':
            param=struct.unpack_from('<I',data,MARKER+16)[0]
            if reader.read(pointer+param,48)!=data[param:param+48]:raise ValueError('EE thread-status output changed')
        if args.capture_point.startswith('sound-ee-') and reader.read(pointer+MARKER,44)!=data[MARKER:MARKER+44]:raise ValueError('EE thread return/status changed')
        if args.capture_point=='sound-service-call-after':
            for start,size in ((0x6351C0,40),(0x635180,64),(0x3B0E00,256)):
                if reader.read(pointer+start,size)!=data[start:start+size]:raise ValueError('SOUNDS completed call client/result/input changed')
        if args.capture_point in ('rpc-dbc-bind-after','rpc-dbc-second-bind-after','rpc-dbc-command-before','rpc-dbc-command-after','rpc-dbc-binds-after','rpc-dbc-sema-after','rpc-dbc-init-after','rpc-dbc-setup-before','rpc-dbc-setup-after','rpc-dbc-next-before','rpc-dbc-next-after','rpc-dbc-port1-before','rpc-dbc-slots-after','rpc-mc-bind-after','rpc-mc-command-before','rpc-mc-command-after','rpc-mc-query-before','rpc-mc-callback-before','rpc-disc-init-after','rpc-disc-bind-sema','rpc-disc-bind-after','rpc-disc-command-before','rpc-disc-command-after','rpc-ncmd-bind-after','rpc-ncmd-before','rpc-ncmd-after','rpc-ncmd-bind-sema','rpc-ncmd-call-sema','rpc-cri-bind-before','rpc-cri-bind-after','rpc-cri-bind-sema','rpc-cri-command-before','rpc-cri-command-after','rpc-cri-second-before','rpc-cri-second-after','rpc-cri-call-sema','rpc-cri-third-before','rpc-cri-fourth-before','rpc-cri-fifth-before','rpc-cri-sixth-before','rpc-cri-nth-before','rpc-cache-after','rpc-cache-invalidate-after'):
            start=0x3A03C0 if args.capture_point=='rpc-dbc-bind-after' else 0x3A0410
            if reader.read(pointer+start,40)!=data[start:start+40]:raise ValueError('DBCMAN completed bind client changed')
        if args.capture_point.startswith('rpc-dbc-'):
            for start,size in ((0x3A03C0,160),(0x3A04C0,144)):
                if reader.read(pointer+start,size)!=data[start:start+size]:raise ValueError('DBCMAN client/result publication changed')
        if args.capture_point=='sound-service-bind-after':
            if reader.read(pointer+0x6351C0,40)!=data[0x6351C0:0x6351C0+40]:raise ValueError('SOUNDS completed bind client changed')
        if args.capture_point=='deci2-after':
            param=struct.unpack_from('<I',data,MARKER+28)[0];text=struct.unpack_from('<I',data,param)[0]
            if param>len(data)-16 or text>=len(data):raise ValueError('DECI2 parameter/string bounds differ')
            end=data.find(b'\0',text,min(text+4096,len(data)))
            if end<0 or reader.read(pointer+param,16)!=data[param:param+16] or reader.read(pointer+text,end-text+1)!=data[text:end+1]:raise ValueError('DECI2 parameter/string changed')
        dump = folder / 'ee-ram.bin'
        dump.write_bytes(data)
        disc_sha = None
        if args.disc:
            with args.disc.open('rb') as disc_stream:
                disc_sha = hashlib.file_digest(disc_stream,'sha256').hexdigest()
        manifest = {'disc_sha256':disc_sha, 'pcsx2_sha256': hashlib.sha256(exe_data).hexdigest(),
                    'bios_sha256': hashlib.sha256(bios_copy.read_bytes()).hexdigest(),
                    'source_elf_sha256': hashlib.sha256(original).hexdigest(),
                    'instrumented_elf_sha256': hashlib.sha256(patched).hexdigest(),
                    'capture_sha256': hashlib.sha256(data).hexdigest(),
                    'capture_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
                    'authored_extra_segment': {'address':FRESH_BIND_STUB,'words':list(struct.unpack('<'+str(len(fresh_bind_stub(args.capture_point.endswith('success'),args.capture_point=='post-scmd-bind-success'))//4)+'I',fresh_bind_stub(args.capture_point.endswith('success'),args.capture_point=='post-scmd-bind-success')))} if args.capture_point in ('post-cd-bind-after','post-cd-bind-success','post-scmd-bind-success') else {'address':FRESH_BIND_STUB,'sha256':hashlib.sha256((texture_stub(args.capture_point) if args.capture_point in TEXTURE_POINTS else {'vif-startup-after':vif_startup_stub,'graph-startup-after':graph_startup_stub,'graph-csr-after':graph_csr_stub}[args.capture_point]())).hexdigest()} if args.capture_point in ('vif-startup-after','graph-startup-after','graph-csr-after','texture-idle','texture-path-idle','texture-poll','texture-after','texture-complete','main-priority-after','ee-arena-after','ee-pools-after','heap-second-after','sound-bind-after','sound-init-after','sound-init-before','deci2-after','sound-service-bind-after','sound-ee-thread-after','sound-ee-thread-before','sound-ee-status-after','sound-service-call-after','rpc-dbc-bind-after','rpc-dbc-second-bind-after','rpc-dbc-command-before','rpc-dbc-command-after','rpc-dbc-binds-after','rpc-dbc-sema-after','rpc-dbc-init-after','rpc-dbc-setup-before','rpc-dbc-setup-after','rpc-dbc-next-before','rpc-dbc-next-after','rpc-dbc-port1-before','rpc-dbc-slots-after','rpc-mc-bind-after','rpc-mc-command-before','rpc-mc-command-after','rpc-mc-query-before','rpc-mc-callback-before','rpc-disc-init-after','rpc-disc-bind-sema','rpc-disc-bind-after','rpc-disc-command-before','rpc-disc-command-after','rpc-ncmd-bind-after','rpc-ncmd-before','rpc-ncmd-after','rpc-ncmd-bind-sema','rpc-ncmd-call-sema','rpc-cri-bind-before','rpc-cri-bind-after','rpc-cri-bind-sema','rpc-cri-command-before','rpc-cri-command-after','rpc-cri-second-before','rpc-cri-second-after','rpc-cri-call-sema','rpc-cri-third-before','rpc-cri-fourth-before','rpc-cri-fifth-before','rpc-cri-sixth-before','rpc-cri-nth-before','rpc-cache-after','rpc-cache-invalidate-after') else None,
                    'patch': {'address': address, 'file_offset': offset, 'size': len(trap),
                              'authored_words': list(struct.unpack('<' + str(len(trap) // 4) + 'I', trap)),
                              'marker_address': MARKER, 'marker_value': marker},
                    'capture_point': args.capture_point + ': authored marker and self-loop at the observed boundary',
                    'observed_timer3_mode': struct.unpack_from('<I', data, MARKER + 4)[0] if args.capture_point == 'timer3-mode' else None,
                    'observed_timer2_mode': struct.unpack_from('<I', data, MARKER + 4)[0] if args.capture_point == 'timer2-mode' else None,
                    'observed_intc_mask': struct.unpack_from('<I', data, MARKER + 4)[0] if args.capture_point == 'intc-mask' else None,
                    'observed_timer2_readback': struct.unpack_from('<I', data, MARKER + 4)[0] if args.capture_point == 'timer2-readback' else None,
                    'observed_osd_config': struct.unpack_from('<I', data, MARKER + 4)[0] if args.capture_point == 'osd-config' else None,
                    'observed_osd_readback': struct.unpack_from('<I', data, MARKER + 4)[0] if args.capture_point == 'osd-readback' else None,
                    'observed_dma_status': struct.unpack_from('<I', data, MARKER + 4)[0] if args.capture_point == 'dma-status' else None,
                    'observed_dma5_control': struct.unpack_from('<I', data, MARKER + 4)[0] if args.capture_point == 'dma5-control' else None,
                    'observed_sif_command': struct.unpack_from('<I', data, MARKER + 4)[0] if args.capture_point == 'sif-command' else None,
                    'observed_sif_selector': struct.unpack_from('<I', data, MARKER + 8)[0] if args.capture_point == 'sif-command' else None,
                    'observed_sif_rpc_state': struct.unpack_from('<I', data, MARKER + 4)[0] if args.capture_point == 'sif-rpc-state' else None,
                    'observed_sif_rpc_selector': struct.unpack_from('<I', data, MARKER + 8)[0] if args.capture_point == 'sif-rpc-state' else None,
                    'iop_capture_sha256': hashlib.sha256(iop_data).hexdigest() if iop_data is not None else None,
                    'observed_sif_dma_result': struct.unpack_from('<I', data, MARKER + 4)[0] if capture_iop else None,
                    'observed_sif_dma_sp': struct.unpack_from('<I', data, MARKER + 8)[0] if capture_iop else None,
                    'original_osd_config': struct.unpack_from('<I', data, MARKER + 8)[0] if args.capture_point == 'osd-readback' else None,
                    'limitations': ('Disc-search initialization/bind checkpoint and client/state windows stable twice. Uninitialized semaphore fields, physical timing, file lookup, sector reads and native audio playback are not established.' if args.capture_point.startswith('rpc-disc-') else 'Sound/console caller publication and owned client/result or string windows stable twice. Shared global RPC reply and full IOP RAM remain live; helper ready/running scheduling equivalence and playback are unvalidated.' if args.capture_point.startswith(('sound-','rpc-')) or args.capture_point=='deci2-after' else 'Disc-backed GIF/startup checkpoint; actual return/poll publication and selected source window stable twice. No hardware DMA timing, texture draw or presentation established.' if args.capture_point in TEXTURE_POINTS else 'Disc-backed VIF/GS startup checkpoint; actual EE register/return publication stable twice. GS address 0x12001010 is not treated as an IMR readback. Full IOP RAM is live; no VU execution, video timing or presentation established.' if args.capture_point in ('vif-startup-after','graph-startup-after','graph-csr-after','texture-idle','texture-path-idle','texture-poll','texture-after','texture-complete','main-priority-after','ee-arena-after','ee-pools-after','heap-second-after','sound-bind-after','sound-init-after','sound-init-before','deci2-after','sound-service-bind-after','sound-ee-thread-after','sound-ee-thread-before','sound-ee-status-after','sound-service-call-after','rpc-dbc-bind-after','rpc-dbc-second-bind-after','rpc-dbc-command-before','rpc-dbc-command-after','rpc-dbc-binds-after','rpc-dbc-sema-after','rpc-dbc-init-after','rpc-dbc-setup-before','rpc-dbc-setup-after','rpc-dbc-next-before','rpc-dbc-next-after','rpc-dbc-port1-before','rpc-dbc-slots-after','rpc-mc-bind-after','rpc-mc-command-before','rpc-mc-command-after','rpc-mc-query-before','rpc-mc-callback-before','rpc-disc-init-after','rpc-disc-bind-sema','rpc-disc-bind-after','rpc-disc-command-before','rpc-disc-command-after','rpc-ncmd-bind-after','rpc-ncmd-before','rpc-ncmd-after','rpc-ncmd-bind-sema','rpc-ncmd-call-sema','rpc-cri-bind-before','rpc-cri-bind-after','rpc-cri-bind-sema','rpc-cri-command-before','rpc-cri-command-after','rpc-cri-second-before','rpc-cri-second-after','rpc-cri-call-sema','rpc-cri-third-before','rpc-cri-fourth-before','rpc-cri-fifth-before','rpc-cri-sixth-before','rpc-cri-nth-before','rpc-cache-after','rpc-cache-invalidate-after') else 'Disc-backed LOADFILE caller checkpoint; publication/client/request and service/buffer stable twice. Full IOP RAM live, no native module execution established.' if args.capture_point in LOADFILE_POINTS else 'Post-GS disc-backed unique SIF caller; EE publication/table/SREG and selected IOP mailbox/ownership ranges stable twice. Full IOP RAM remains live; no native IOP boot or scheduling established.' if args.capture_point.startswith('post-sif-') else 'Disc-backed independent GIF checkpoint; register/return/SP publication stable twice. First source packet checked twice at gif-idle, second source packet checked twice at gif-path-after. No continuous timing, native transfer or presentation inferred.' if args.capture_point.startswith('gif-') else 'Disc-backed independent INTC point; status, mask, SP and preceding v0 publication stable twice. No continuous timing trace, native video clock or callback execution established.' if args.capture_point in ('gs-intc-before','gs-intc-cleared','gs-intc-event','gs-intc-acknowledged','gif-idle','gif-qwc','gif-after','gif-sync-before','gif-sync-after','gif-sync-poll','gif-vu-idle','gif-path-after','post-sif-before','post-sif-after','post-scmd-bind-success','post-mmode-after','post-cd-after','post-cd-bind-after','post-cd-bind-success','loadfile-bind-after','loadfile-version-after','loadfile-module-before','loadfile-module-after','mcman-module-after','mcserv-module-after','dbcman-module-after','sio2d-module-after','ds2ud-module-after','libsd-module-after','sdrdrv-module-after','modhsyn-module-after','modsesq2-module-after','cdvdstm-module-after','sounds-module-after','cri-adxi-module-after','fileio-bind-after','heap-alloc-after','heap-copy-after','module-buffer-after','heap-free-after','dmac-startup-after','vif-startup-after','graph-startup-after','graph-csr-after','texture-idle','texture-path-idle','texture-poll','texture-after','texture-complete','main-priority-after','ee-arena-after','ee-pools-after','heap-second-after','sound-bind-after','sound-init-after','sound-init-before','deci2-after','sound-service-bind-after','sound-ee-thread-after','sound-ee-thread-before','sound-ee-status-after','sound-service-call-after','rpc-dbc-bind-after','rpc-dbc-second-bind-after','rpc-dbc-command-before','rpc-dbc-command-after','rpc-dbc-binds-after','rpc-dbc-sema-after','rpc-dbc-init-after','rpc-dbc-setup-before','rpc-dbc-setup-after','rpc-dbc-next-before','rpc-dbc-next-after','rpc-dbc-port1-before','rpc-dbc-slots-after','rpc-mc-bind-after','rpc-mc-command-before','rpc-mc-command-after','rpc-mc-query-before','rpc-mc-callback-before','rpc-disc-init-after','rpc-disc-bind-sema','rpc-disc-bind-after','rpc-disc-command-before','rpc-disc-command-after','rpc-ncmd-bind-after','rpc-ncmd-before','rpc-ncmd-after','rpc-ncmd-bind-sema','rpc-ncmd-call-sema','rpc-cri-bind-before','rpc-cri-bind-after','rpc-cri-bind-sema','rpc-cri-command-before','rpc-cri-command-after','rpc-cri-second-before','rpc-cri-second-after','rpc-cri-call-sema','rpc-cri-third-before','rpc-cri-fourth-before','rpc-cri-fifth-before','rpc-cri-sixth-before','rpc-cri-nth-before','rpc-cache-after','rpc-cache-invalidate-after') else 'Disc-backed reached GS register/syscall return, 64-bit value and SP publication stable twice; no renderer or general GS state captured.' if args.capture_point in ('gs-csr-reset','gs-imr-after','gs-crt-after','gs-intc-after','gs-csr-field','gs-intc-before','gs-intc-cleared','gs-intc-event','gs-intc-acknowledged','gif-idle','gif-qwc','gif-after','gif-sync-before','gif-sync-after','gif-sync-poll','gif-vu-idle','gif-path-after','post-sif-before','post-sif-after','post-scmd-bind-success','post-mmode-after','post-cd-after','post-cd-bind-after','post-cd-bind-success','loadfile-bind-after','loadfile-version-after','loadfile-module-before','loadfile-module-after','mcman-module-after','mcserv-module-after','dbcman-module-after','sio2d-module-after','ds2ud-module-after','libsd-module-after','sdrdrv-module-after','modhsyn-module-after','modsesq2-module-after','cdvdstm-module-after','sounds-module-after','cri-adxi-module-after','fileio-bind-after','heap-alloc-after','heap-copy-after','module-buffer-after','heap-free-after','dmac-startup-after','vif-startup-after','graph-startup-after','graph-csr-after','texture-idle','texture-path-idle','texture-poll','texture-after','texture-complete','main-priority-after','ee-arena-after','ee-pools-after','heap-second-after','sound-bind-after','sound-init-after','sound-init-before','deci2-after','sound-service-bind-after','sound-ee-thread-after','sound-ee-thread-before','sound-ee-status-after','sound-service-call-after','rpc-dbc-bind-after','rpc-dbc-second-bind-after','rpc-dbc-command-before','rpc-dbc-command-after','rpc-dbc-binds-after','rpc-dbc-sema-after','rpc-dbc-init-after','rpc-dbc-setup-before','rpc-dbc-setup-after','rpc-dbc-next-before','rpc-dbc-next-after','rpc-dbc-port1-before','rpc-dbc-slots-after','rpc-mc-bind-after','rpc-mc-command-before','rpc-mc-command-after','rpc-mc-query-before','rpc-mc-callback-before','rpc-disc-init-after','rpc-disc-bind-sema','rpc-disc-bind-after','rpc-disc-command-before','rpc-disc-command-after','rpc-ncmd-bind-after','rpc-ncmd-before','rpc-ncmd-after','rpc-ncmd-bind-sema','rpc-ncmd-call-sema','rpc-cri-bind-before','rpc-cri-bind-after','rpc-cri-bind-sema','rpc-cri-command-before','rpc-cri-command-after','rpc-cri-second-before','rpc-cri-second-after','rpc-cri-call-sema','rpc-cri-third-before','rpc-cri-fourth-before','rpc-cri-fifth-before','rpc-cri-sixth-before','rpc-cri-nth-before','rpc-cache-after','rpc-cache-invalidate-after') else 'Disc-mounted ready-branch EE publication stable twice; post-reset IOP RAM is a live diagnostic snapshot, not atomically frozen or proven stable. No native IOP execution validated.' if capture_boot_iop else 'Instrumented ELF with explicit local disc when disc_sha256 is present; reboot submission publication/EE packet stable twice; no IOP image execution or reset-ready result inferred.' if args.capture_point == 'reset-submit-after' else 'Instrumented ELF with explicit local disc when disc_sha256 is present; register return/SP and direct SMFLAG readback publication stable twice; no post-reset IOP snapshot or boot completion established.' if args.capture_point in ('reset-flags-after','reset-rpc-clear','reset-subaddr-clear','reset-cmd-clear') else 'Standalone ELF; authored direct SMFLAG read before/after reached register write, publication stable twice; no reset completion established.' if args.capture_point in ('reset-smflag-before','reset-smflag-after','reset-flags-after','reset-rpc-clear','reset-subaddr-clear','reset-cmd-clear') else 'Standalone ELF; SifStopDma return/publication and kernel implementation windows stable twice; no active DMA cancellation or IOP reset captured.' if args.capture_point in ('sif-stop-after','sif-reset-subaddr') else 'Instrumented standalone ELF with no disc; second bind/media-mode request/reply/client and server windows stable twice, semaphore inferred from completed client and residual descriptor; no timing or hardware DMA result established.' if args.capture_point in ('scmd-bind-after','scmd-mmode-after') else 'Instrumented standalone ELF boot; semaphore descriptor/publication/IDs stable in two reads; no scheduler or attribute semantics established.' if args.capture_point == 'cd-semaphores' else 'Instrumented standalone ELF boot; CD-init request/reply/client/data and IOP service windows stable in two reads; no disc mounted or hardware timing established.' if args.capture_point == 'cd-init-after' else 'Instrumented standalone ELF boot; semaphore descriptor/publication stable in two reads; kernel and IOP remain live.' if args.capture_point == 'rpc-call-sema' else 'Instrumented standalone ELF boot; EE running in loop, IOP active. Bind request, response, client, handler table and IOP service/queue stable in two reads; no interrupt timing captured.' if args.capture_point == 'sif-bind-after' else 'Instrumented standalone ELF boot; EE running in loop, IOP active. SIF descriptor/packet and IOP receive packet/table stable in two reads; kernel and IOP continue running.' if capture_iop else 'Instrumented standalone ELF boot; EE running in loop, IOP active. SIF query publication stable; kernel continues running.' if args.capture_point == 'sif-rpc-state' else 'Instrumented standalone ELF boot; EE running in loop, IOP active. Kernel prefix stable in two reads.') + ' Diagnostic only; no gameplay validated.'}
        (folder / 'capture.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
        print(f'Captured {len(data)} bytes at {args.capture_point}: {dump}')
    finally:
        if reader:
            reader.close()
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)


if __name__ == '__main__':
    main()
