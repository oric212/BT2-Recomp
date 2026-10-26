// paraLLEl-GS backend on synthetic GIF packets (no game data): drawn pixels checked word by word.
#include "gs/gpu/gpu_gs_backend.h"
#include "gs/cpu/gs_command_sink.h"
#include "gs/gpu/hd_texture_pack.h"
#include "gs/textures/gs_texture_names.h"
#include "host/display_frame.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
// A+D-only PACKED packet: one GIF tag, then one register write per qword.
std::vector<std::uint8_t> packet(std::initializer_list<std::pair<std::uint64_t,std::uint64_t>> writes) {
    std::vector<std::uint8_t> bytes;
    auto q=[&](std::uint64_t lo,std::uint64_t hi){std::uint8_t b[16];std::memcpy(b,&lo,8);std::memcpy(b+8,&hi,8);bytes.insert(bytes.end(),b,b+16);};
    q(writes.size()|(1ULL<<15)|(1ULL<<60),0xE); // NLOOP, EOP, PACKED, NREG 1: A+D.
    for(const auto& [reg,value]:writes)q(value,reg);
    return bytes;
}

// The PSMCT32 word at (x, y) of a frame buffer at page 0, 64 pixels wide.
std::uint32_t pixel(std::span<const std::uint32_t> vram,unsigned x,unsigned y) {return vram[GsCommandSink::ct32Word(0,1,x,y)];}
}

// HD texture packs (hd_texture_pack.h): names as PCSX2 reads them and the DDS files the pack loads.
void hdTexturePackFiles() {
    auto require=[](bool ok,const char* what){if(!ok)throw std::runtime_error(std::string("HD texture pack: ")+what);};
    const auto with=HdTexturePack::key("1a-2B-00005a53"),without=HdTexturePack::key("3c-00001a54");
    require(with && with->texture==0x1a && with->palette==0x2b && with->has_palette && with->format==0x1a53,"three parts, bit 14 cleared");
    require(without && without->texture==0x3c && !without->has_palette && without->palette==0 && without->format==0x1a54,"a name without a palette part");
    require(!HdTexturePack::key("1a") && !HdTexturePack::key("1a-2b-3c-4d") && !HdTexturePack::key("1a--2b") && !HdTexturePack::key("1a-zz")
            && !HdTexturePack::key("1a-100000000") && !HdTexturePack::key("11112222333344445-1a"),"not a name: refused");
    auto dds=[](const char* fourcc,std::uint32_t dxgi,std::uint32_t width,std::uint32_t height,std::uint32_t mips,std::size_t data) {
        std::vector<std::uint8_t> file(std::strcmp(fourcc,"DX10")?128:148);
        std::memcpy(file.data(),"DDS ",4);
        std::memcpy(file.data()+12,&height,4);std::memcpy(file.data()+16,&width,4);std::memcpy(file.data()+28,&mips,4);
        std::memcpy(file.data()+84,fourcc,4);
        if(!std::strcmp(fourcc,"DX10"))std::memcpy(file.data()+128,&dxgi,4);
        for(std::size_t i=0;i<data;++i)file.push_back(std::uint8_t(i));
        return file;
    };
    // BC3 (DXGI 77), 8 x 8 with two levels: 4 blocks, then 1.
    const auto bc3=HdTexturePack::readDds(dds("DX10",77,8,8,2,5*16));
    require(bc3 && bc3->format==137 && bc3->levels.size()==2 && bc3->levels[0].size()==64 && bc3->levels[1].size()==16 && bc3->levels[1][0]==64,"BC3 levels");
    // A mip chain shorter than the header says keeps the levels present.
    require(HdTexturePack::readDds(dds("DX10",77,8,8,4,5*16))->levels.size()==2,"short mip chain");
    // DXT1 (BC1, 8 bytes a block) without the DX10 header.
    const auto bc1=HdTexturePack::readDds(dds("DXT1",0,8,8,1,32));
    require(bc1 && bc1->format==133 && bc1->levels[0].size()==32,"DXT1");
    require(!HdTexturePack::readDds(dds("DX10",28,8,8,1,256)),"an uncompressed format is refused");
    require(!HdTexturePack::readDds(std::vector<std::uint8_t>(16)),"not a DDS file");
}

// The pack's loader (hd_texture_pack.h, streaming): a file asked for is loaded on the loader's thread;
// until then a lookup says it is loading; a file that is not a DDS ends refused; a name not in the pack is none.
void hdTexturePackLoading(GpuGsBackend& gpu) {
    auto require=[](bool ok,const char* what){if(!ok)throw std::runtime_error(std::string("HD texture loading: ")+what);};
    const auto folder=std::filesystem::temp_directory_path()/"bt2-hd-pack-test";
    std::filesystem::remove_all(folder);std::filesystem::create_directories(folder/"replacements");
    std::vector<std::uint8_t> file(148);
    std::memcpy(file.data(),"DDS ",4);
    const std::uint32_t size=8,levels=1,dxgi=77;
    std::memcpy(file.data()+12,&size,4);std::memcpy(file.data()+16,&size,4);std::memcpy(file.data()+28,&levels,4);
    std::memcpy(file.data()+84,"DX10",4);std::memcpy(file.data()+128,&dxgi,4);
    file.resize(file.size()+4*16);
    std::ofstream(folder/"replacements"/"1a-2b-00005a53.dds",std::ios::binary).write(reinterpret_cast<const char*>(file.data()),std::streamsize(file.size()));
    std::ofstream(folder/"replacements"/"3c-00001a54.dds")<<"not a DDS file";
    // Two lit variants of texture 77, opaque red and opaque blue, 16 x 16 BC3.
    for(const auto& [palette,colour]:{std::pair<const char*,std::uint16_t>{"1",0xF800},{"2",0x001F}}) {
        std::vector<std::uint8_t> variant(file.begin(),file.begin()+148);
        const std::uint32_t side=16;std::memcpy(variant.data()+12,&side,4);std::memcpy(variant.data()+16,&side,4);
        for(int block=0;block<16;++block) {
            const std::uint8_t b[16]{255,255,0,0,0,0,0,0,std::uint8_t(colour),std::uint8_t(colour>>8),std::uint8_t(colour),std::uint8_t(colour>>8),0,0,0,0};
            variant.insert(variant.end(),b,b+16);
        }
        std::ofstream(folder/"replacements"/(std::string("77-")+palette+"-00001a53.dds"),std::ios::binary).write(reinterpret_cast<const char*>(variant.data()),std::streamsize(variant.size()));
    }
    GsTextureNames names;
    gpu.enableHdTextures(folder,64<<20,names);
    auto& pack=*gpu.hdTexturePack();
    const HdTexturePack::Key good{0x1a,0x2b,0x1a53,true},bad{0x3c,0,0x1a54,false},absent{0x99,0,0x1a54,false};
    const auto first=pack.find(good);
    require(first.image || first.loading,"a file asked for is loading or loaded");
    pack.find(bad);
    pack.waitLoaded();
    const auto later=pack.find(good);
    require(later.image && !later.loading && pack.loaded==1,"loaded by the loader");
    const auto refused=pack.find(bad);
    require(!refused.image && !refused.loading && pack.refused==1,"not a DDS file: refused");
    const auto none=pack.find(absent);
    require(!none.image && !none.loading,"a name not in the pack");
    // Lit variants: the colours are read by the loader first; then the nearer variant within the limit.
    const auto colours=[](float r,float g,float b){TextureColours c;for(unsigned cell=0;cell<TextureColours::cells;++cell)c.add(cell,r,g,b,1,1);c.finish();return c;};
    require(pack.closest(0x77,0x1a53,colours(240,10,0)).loading,"variants' colours read first");
    pack.waitLoaded();
    const auto reddish=pack.closest(0x77,0x1a53,colours(240,10,0));
    require(reddish.key && reddish.key->palette==1 && pack.variants_chosen==1,"the nearer variant");
    require(!pack.closest(0x77,0x1a53,colours(0,200,0)).key && pack.variants_too_far==1,"none near enough");
    require(!pack.closest(0x78,0x1a53,colours(240,10,0)).key,"a texture without variants");
    std::error_code error;std::filesystem::remove_all(folder,error);
}

// A second renderer on the device (the in-between pictures), its work on the async compute queue: it takes the
// main renderer's kept state (keepState, takeState), draws on it and scans out on the graphics queue the picture
// the main renderer scans out after the same draw; the main renderer does not see its drawing.
void secondRenderer(GpuGsBackend& gpu) {
    auto require=[](bool ok,const char* what){if(!ok)throw std::runtime_error(std::string("Second renderer: ")+what);};
    const auto frame=[](GpuGsBackend& r) {
        // The 64 x 32 frame at page 0, NTSC without interlace: DX 636, DY 25, MAGH 3 (four clocks a pixel), DW 255, DH 31.
        r.privileged(0,1u|(1u<<5)|(0xFFu<<8));r.privileged(1,(32u<<3)|(2u<<13));r.privileged(2,0);
        r.privileged(7,1u<<9);r.privileged(8,636u|(25u<<12)|(3u<<23)|(255ull<<32)|(31ull<<44));
        r.submit(); // Its drawing so far, as at a flip.
        return r.scanout(0);
    };
    gpu.flush();gpu.keepState(0);
    GpuGsBackend second;second.initShared(gpu);
    second.takeState(gpu,0);
    // A sprite in another colour at x 4..15, y 20..27, on both.
    const auto more=packet({{0x4C,0x0000000000010000ULL},{0x4E,0x0000000101000020ULL},{0x18,0},
        {0x40,0x001F0000003F0000ULL},{0x47,0x30000},{0x00,6},{0x01,0x0000000020C04080ULL},
        {0x05,0x0000000001400040ULL},{0x05,0x0000000001C00100ULL}});
    second.transfer(more);
    const auto drawn=second.readVram();
    require(pixel(drawn,8,8)==0x80402010u && pixel(drawn,40,16)==0x80208040u,"the kept state taken");
    require(pixel(drawn,8,24)==0x20C04080u,"its own drawing");
    require(pixel(gpu.readVram(),8,24)!=0x20C04080u,"the main renderer unchanged");
    gpu.transfer(more);
    const auto main_picture=frame(gpu),second_picture=frame(second);
    require(main_picture.width && main_picture.width==second_picture.width && main_picture.height==second_picture.height,"scanout sizes");
    require(main_picture.bgra==second_picture.bgra,"the same picture");
    require(std::count(main_picture.bgra.begin(),main_picture.bgra.end(),0xFF000000u)<std::ptrdiff_t(main_picture.bgra.size()),"a picture with the drawing");
}

// Native passes (gs/gpu/native/native_passes.h): the alpha fill leaves GS memory as the renderer's drawing does. Two
// renderers take the same kept state, one drawing everything and one with the pass, and draw the same sprites: two
// blended flat strips that write only the alpha byte of x 0..31, y 0..31.
void nativeAlphaFill(GpuGsBackend& gpu) {
    auto require=[](bool ok,const char* what){if(!ok)throw std::runtime_error(std::string("Native alpha fill: ")+what);};
    const auto passes=[](const char* value) {
#ifdef _WIN32
        _putenv_s("BT2_NATIVE_PASSES",value);
#else
        setenv("BT2_NATIVE_PASSES",value,1);
#endif
    };
    gpu.flush();gpu.keepState(1);
    passes("none");GpuGsBackend emulated;emulated.initShared(gpu);
    passes("alpha-fill");GpuGsBackend native;native.initShared(gpu);
    passes("");
    const auto fill=packet({{0x4C,0x00FFFFFF00010000ULL},{0x4E,0x0000000101000020ULL},{0x18,0},
        {0x40,0x001F0000003F0000ULL},{0x47,0x30000},{0x42,0x44},{0x00,6|(1u<<6)},{0x01,0x0000000040FFFFFFULL},
        {0x05,0},{0x05,0x0000000002000100ULL},{0x05,0x0000000000000100ULL},{0x05,0x0000000002000200ULL}});
    std::vector<std::uint32_t> before;
    for(auto* r:{&emulated,&native}) {
        r->takeState(gpu,1);
        if(before.empty()) {const auto v=r->readVram();before.assign(v.begin(),v.end());}
        r->transfer(fill);
    }
    const auto e=emulated.readVram();
    const std::vector<std::uint32_t> expected(e.begin(),e.end());
    const auto n=native.readVram();
    require(native.nativePassSummary()=="Native passes: alpha-fill 1" && emulated.nativePassSummary().empty(),"the pass ran on its renderer only");
    require(pixel(expected,8,8)==((pixel(before,8,8)&0xFFFFFFu)|0x40000000u) && pixel(expected,40,8)==pixel(before,40,8),"the emulated fill");
    require(std::equal(expected.begin(),expected.end(),n.begin()),"the same GS memory");
}

int main() try {
    hdTexturePackFiles();
    GpuGsBackend gpu;
    try {gpu.init();}
    catch(const std::exception& e) {std::cout<<"skipped: "<<e.what()<<'\n';return 77;}
    // Flat sprite into a 64-pixel-wide PSMCT32 frame at page 0; depth writes masked.
    const auto sprite=packet({{0x4C,0x0000000000010000ULL},{0x4E,0x0000000101000020ULL},{0x18,0},
        {0x40,0x001F0000003F0000ULL},{0x47,0x30000},{0x00,6},{0x01,0x0000000080402010ULL},
        {0x05,0x0000000000400040ULL},{0x05,0x0000000001000100ULL}});
    gpu.transfer(sprite);
    {
        // Pixels (4,4) to (15,15) take the colour; the right and bottom edges and beyond do not.
        const auto vram=gpu.readVram();
        for(unsigned y=4;y<16;++y)for(unsigned x=4;x<16;++x)if(pixel(vram,x,y)!=0x80402010u)throw std::runtime_error("Sprite pixel missing");
        if(pixel(vram,16,8)==0x80402010u || pixel(vram,8,16)==0x80402010u || pixel(vram,3,3)==0x80402010u)throw std::runtime_error("Sprite drew past its edges");
    }
    {
        // A draw packet that reaches the renderer in pieces ending inside the packet (as the texture names'
        // lockstep sends it) draws as the whole packet does: a PACKED UV/RGBAQ/XYZ2 triangle strip (the layout
        // the renderer runs a whole loop at a time; no texture), 4 vertices.
        std::vector<std::uint8_t> strip;
        auto q=[&](std::uint64_t lo,std::uint64_t hi){std::uint8_t b[16];std::memcpy(b,&lo,8);std::memcpy(b+8,&hi,8);strip.insert(strip.end(),b,b+16);};
        q(4|(1ULL<<15)|(1ULL<<46)|(4ULL<<47)|(3ULL<<60),0x513); // NLOOP 4, EOP, PRE triangle strip, NREG 3: UV, RGBAQ, XYZ2.
        const std::uint64_t xy[4][2]={{0x200,0x80},{0x300,0x80},{0x200,0x180},{0x300,0x180}}; // 12.4: x 32..48, y 8..24.
        for(const auto& v:xy) {q(0,0);q(0x40ULL|(0x80ULL<<32),0x20ULL|(0x80ULL<<32));q(v[0]|(v[1]<<32),0);}
        // Each piece in its own buffer followed by poison (a vertex over the whole scissor area), so reading
        // past a piece's end draws pixels outside the square.
        for(std::size_t at=0;at<strip.size();at+=48) {
            const std::size_t size=std::min<std::size_t>(48,strip.size()-at);
            std::vector<std::uint8_t> piece(strip.begin()+std::ptrdiff_t(at),strip.begin()+std::ptrdiff_t(at+size));
            for(int k=0;k<8;++k) {const std::uint64_t poison[2]={0x0FF000000FF0ULL,0};piece.insert(piece.end(),reinterpret_cast<const std::uint8_t*>(poison),reinterpret_cast<const std::uint8_t*>(poison)+16);}
            gpu.transfer(std::span<const std::uint8_t>(piece).first(size));
        }
        // The square x 32..47, y 8..23 in R 0x40, G 0x80, B 0x20, A 0x80; nothing outside it.
        const auto vram=gpu.readVram();
        for(unsigned y=8;y<24;++y)for(unsigned x=32;x<48;++x)if(pixel(vram,x,y)!=0x80208040u)throw std::runtime_error("Strip pixel missing");
        if(pixel(vram,48,16) || pixel(vram,40,24) || pixel(vram,60,28))throw std::runtime_error("The strip drew outside its square (read past a piece)");
    }
    // The sink checks the packet's framing and passes it on; it draws nothing.
    GsCommandSink validator;validator.transfer(sprite);
    if(validator.transfers!=1)throw std::runtime_error("The sink lost the transfer");
    secondRenderer(gpu);
    nativeAlphaFill(gpu);
    hdTexturePackLoading(gpu); // Last: the pack stays on the renderer.
    std::cout<<"GPU GS draws the sprite and the strip as expected; a second renderer on the compute queue takes the kept state and scans out the same picture; the native alpha fill leaves the same GS memory; HD textures load on their thread\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
