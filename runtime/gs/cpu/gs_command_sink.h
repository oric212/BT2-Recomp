#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

// Scoped software command consumption, separate from DMA and presentation.
// Unwritten VRAM bits remain unknown; construction does not emulate a GS clear.
class GsCommandSink {
public:
    struct Word { std::uint32_t value=0, known=0; };
    GsCommandSink():vram_(1024*1024) {}
    void resetDrawingState() {
        // CSR reset establishes SCANMSK/FBA/PABE defaults. Do not clear VRAM.
        registers={};tex0={};pabe=0;drawing_reset_=true;
    }
    static std::size_t address(unsigned page,unsigned width,unsigned x,unsigned y,bool depth) {
        // PSMCT32/PSMZ24: 64x32 pages, 8x8 blocks. Public GS bit layout,
        // independently expressed as bit placement rather than reference tables.
        unsigned bx=(x%64)/8, by=(y%32)/8;
        unsigned block=(bx&1)|((by&1)<<1)|((bx&2)<<1)|((by&2)<<2)|((bx&4)<<2);
        unsigned px=x%8, py=y%8;
        unsigned pixel=(px&1)|((py&1)<<1)|((px&6)<<1)|((py&6)<<3);
        std::size_t word=((std::size_t(page)+(y/32)*width+x/64)*2048+block*64+pixel);
        if(depth)word^=1536;
        return word%(1024*1024);
    }
    Word word(std::size_t index) const {return vram_.at(index);}
    void seedForTest(std::size_t index,std::uint32_t value) {vram_.at(index)={value,UINT32_MAX};}
    void consume(std::span<const std::uint8_t> packet);
    void consumeConfiguration(std::span<const std::uint8_t> packet);
    // General GIF transfers (frame path): GIFtags in PACKED, REGLIST or IMAGE form update the GS
    // register file, host->local image transfers write VRAM and vertex kicks draw the supported
    // primitives. A group of transfers is one transaction: register state, CLUT, VRAM and counters
    // are restored if any part is refused, so a rejected group changes nothing.
    void transfer(std::span<const std::uint8_t> data) {transferAll(std::span<const std::span<const std::uint8_t>>(&data,1));}
    void transferAll(std::span<const std::span<const std::uint8_t>> group);
    struct State {
        std::array<std::uint64_t,0x64> regs{};
        std::array<std::uint64_t,3> vertex_xyz{};std::array<std::uint64_t,3> vertex_rgbaq{};std::array<std::uint64_t,3> vertex_uv{};unsigned vertices=0;
        std::uint32_t q=0x3F800000; // ST packing sets Q; RGBAQ packing takes it.
        // Host->local transfer in progress (TRXDIR 0): destination and pixel position.
        unsigned image_qwords_left=0; // IMAGE data of the last tag still to come in later transfers.
        bool trx=false;unsigned trx_bp=0,trx_bw=0,trx_x0=0,trx_y0=0,trx_w=0,trx_h=0,trx_x=0,trx_y=0;
    };
    State state;
    unsigned transfers=0,frame_sprites=0,frame_triangles=0,register_writes=0,clut_loads=0;
    std::uint64_t frame_pixels=0,image_pixels=0;
    // Accepted GIF data in order, after validation (GPU mirror, docs/plans/graphics/GPU_RENDERING_PLAN.md).
    std::function<void(std::span<const std::uint8_t>)> on_accepted;
    // false (GPU-only mode): packets are still validated and tracked, but no pixels are drawn here.
    bool rasterize=true;
    // PSMCT32 word for block base pointer bp (64-word blocks) and buffer width bw (64-pixel units).
    static std::size_t ct32Word(unsigned bp,unsigned bw,unsigned x,unsigned y) {
        unsigned bx=(x%64)/8,by=(y%32)/8;
        unsigned block=(bx&1)|((by&1)<<1)|((bx&2)<<1)|((by&2)<<2)|((bx&4)<<2);
        unsigned px=x%8,py=y%8;
        unsigned pixel=(px&1)|((py&1)<<1)|((px&6)<<1)|((py&6)<<3);
        return (std::size_t(bp)*64+(std::size_t(y/32)*bw+x/64)*2048+block*64+pixel)%(1024*1024);
    }
    // PSMT4 nibble: 128x128 pages, 32x16 blocks, four 32x4 columns per block (128 nibbles each).
    // Within a column, pixel x = 8g+k places k's bits at nibble strides 8, 32 and 64 and g at stride 2;
    // odd rows add 16, rows 2-3 add 1 and flip the 64 stride, as do odd columns (public GS layout,
    // expressed as bit placement and checked against the reference layout). Returns word*8+nibble.
    static std::size_t t4Nibble(unsigned bp,unsigned bw,unsigned x,unsigned y) {
        const unsigned pages_per_row=bw/2;
        const unsigned page=(y/128)*pages_per_row+x/128;
        const unsigned bx=(x%128)/32,by=(y%128)/16;
        const unsigned block=(by&1)|((bx&1)<<1)|((by&2)<<1)|((bx&2)<<2)|((by&4)<<2);
        const unsigned cx=x%32,cy=y%16,column=cy/4,row=cy%4,k=cx%8,g=cx/8;
        const unsigned n=2*g+8*(k&1)+32*((k>>1)&1)+64*(((k>>2)&1)^(row>>1)^(column&1))+16*(row&1)+(row>>1);
        const std::size_t nibble=std::size_t(column)*128+n;
        return ((std::size_t(bp)+std::size_t(page)*32+block)*64*8+nibble)%(std::size_t(1024)*1024*8);
    }
    // PSMT8 byte: 128x64 pages, 16x16 blocks (PSMCT32 block order), four 16x4 columns per block
    // (64 bytes each). Within a column, x's bits 0-3 sit at byte strides 4, 16, 32 and 2; odd rows add
    // 8, rows 2-3 add 1 and flip the 32 stride, as do odd columns (public GS layout, expressed as bit
    // placement and checked against the reference layout). Returns word*4+byte.
    static std::size_t t8Byte(unsigned bp,unsigned bw,unsigned x,unsigned y) {
        const unsigned page=(y/64)*(bw/2)+x/128;
        const unsigned bx=(x%128)/16,by=(y%64)/16;
        const unsigned block=(bx&1)|((by&1)<<1)|((bx&2)<<1)|((by&2)<<2)|((bx&4)<<2);
        const unsigned cx=x%16,cy=y%16,column=cy/4,row=cy%4;
        const unsigned b=column*64+4*(cx&1)+16*((cx>>1)&1)+32*(((cx>>2)&1)^(row>>1)^(column&1))+2*((cx>>3)&1)+8*(row&1)+(row>>1);
        return ((std::size_t(bp)+std::size_t(page)*32+block)*64*4+b)%(std::size_t(1024)*1024*4);
    }
private:
    struct Clut {std::array<std::uint32_t,256> color{};std::array<bool,256> known{};};
    Clut clut_;
    void run(std::span<const std::uint8_t> data,bool apply);
    void image(std::span<const std::uint8_t> data);
    void startTransfer(std::uint64_t dir);
    void loadClut(std::uint64_t texture0);
    void packed(unsigned d,std::uint64_t lo,std::uint64_t hi,bool apply);
    void write(unsigned reg,std::uint64_t value,bool apply);
    struct Rgba;
    static Rgba unpack(std::uint32_t c);
    struct Draw;
    std::uint32_t texel(const Draw& d,int u,int v) const;
    // Draw state validated once per primitive (shared by sprites and triangles).
    Draw prepare(std::uint64_t prim) const;
    Rgba sample(const Draw& d,int u,int v) const;
    // One pixel: texture function, alpha blend, masked framebuffer write and optional Z write.
    void plot(const Draw& d,int px,int py,Rgba vertex,int u,int v,std::uint32_t z);
    void sprite(std::uint64_t prim,bool apply);
    // Triangle (also each strip/fan step): pixels whose integer position lies inside the 12.4 vertex
    // triangle, with a top-left fill rule on shared edges. Gouraud (IIP) interpolates RGBA, otherwise the
    // last vertex's color is used; FST UVs and Z are interpolated linearly (affine).
    void triangle(std::uint64_t prim,bool apply);
public:
    std::array<std::uint64_t,2> tex0{};
    std::uint64_t pabe=0;
    unsigned configuration_packets=0;
    unsigned packets=0,sprites=0;
    std::uint64_t pixels=0;
    std::array<std::uint64_t,14> registers{};
private:
    bool drawing_reset_=false;
    std::vector<Word> vram_;
    // Transaction rollback (transferAll): copy-on-write of 512-word VRAM pages. The first write to a
    // page while logging saves the whole page; a refused group restores the saved pages.
    static constexpr std::size_t undo_page_words=512;
    std::vector<std::uint8_t> undo_saved_=std::vector<std::uint8_t>(1024*1024/undo_page_words);
    std::vector<std::size_t> undo_pages_;std::vector<Word> undo_words_;bool logging_=false;
    Word& writable(std::size_t index) {
        if(logging_) {
            const auto page=index/undo_page_words;
            if(!undo_saved_[page]) {
                undo_saved_[page]=1;undo_pages_.push_back(page);
                undo_words_.insert(undo_words_.end(),vram_.begin()+std::ptrdiff_t(page*undo_page_words),vram_.begin()+std::ptrdiff_t((page+1)*undo_page_words));
            }
        }
        return vram_[index];
    }
    void endTransaction(bool rollback) {
        for(std::size_t k=0;k<undo_pages_.size();++k) {
            if(rollback)std::copy_n(undo_words_.begin()+std::ptrdiff_t(k*undo_page_words),undo_page_words,vram_.begin()+std::ptrdiff_t(undo_pages_[k]*undo_page_words));
            undo_saved_[undo_pages_[k]]=0;
        }
        undo_pages_.clear();undo_words_.clear();logging_=false;
    }
};
