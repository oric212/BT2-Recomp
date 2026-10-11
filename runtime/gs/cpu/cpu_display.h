#pragma once
#include "host/display_frame.h"
#include "gs/cpu/gs_command_sink.h"

// Decodes the displayed framebuffer of the owned GS memory model into a BGRA image (host order for
// Win32 DIBs). The privileged DISPFB/DISPLAY registers select the buffer: FBP (2048-word pages),
// FBW (64-pixel units), PSM, DBX/DBY, and the display width/height after horizontal magnification.
// Never-written pixels become a dark checker so missing coverage stays visible.
inline DisplayFrame decodeDisplay(const GsCommandSink& gs,std::uint64_t dispfb,std::uint64_t display) {
    const unsigned fbp=unsigned(dispfb&511),fbw=unsigned((dispfb>>9)&63),psm=unsigned((dispfb>>15)&31);
    const unsigned dbx=unsigned((dispfb>>32)&2047),dby=unsigned((dispfb>>43)&2047);
    const unsigned magh=unsigned((display>>23)&15);
    DisplayFrame f;
    f.width=unsigned(((display>>32)&4095)+1)/(magh+1);f.height=unsigned(((display>>44)&2047)+1);
    if((psm!=0 && psm!=1) || !fbw || !f.width || !f.height || f.width>2048 || f.height>2048)
        throw std::runtime_error("Unsupported display framebuffer");
    f.bgra.resize(std::size_t(f.width)*f.height);
    for(unsigned y=0;y<f.height;++y)for(unsigned x=0;x<f.width;++x) {
        const auto w=gs.word(GsCommandSink::ct32Word(fbp*32,fbw,dbx+x,dby+y));
        auto& out=f.bgra[std::size_t(y)*f.width+x];
        if((w.known&0xFFFFFF)!=0xFFFFFF) {++f.unwritten;out=((x/8+y/8)&1)?0x00FF00FFu:0x00400040u;continue;}
        const std::uint32_t r=w.value&255,g=(w.value>>8)&255,b=(w.value>>16)&255;
        out=b|(g<<8)|(r<<16);
    }
    return f;
}
