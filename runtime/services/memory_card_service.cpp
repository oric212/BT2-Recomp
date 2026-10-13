#include "ee/ee_runtime.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>

// Native memory card service (MCSERV, 0x80000400) with no card inserted. Requests use the public
// mcDescParam layout (48 bytes: port, slot, flags, type/free/format request flags, buffer, EE end
// parameter address). Results as MCSERV returned them for an empty port 0 / slot 0:
// - init (0xFE): {0, server version 0x210, driver version 0x226};
// - GetInfo (1, no-wait): reply -11 (no card); the 192-byte end parameter receives type 0 and free
//   space 0, after which the game's end function runs. The original's remaining end-parameter bytes
//   are stale IOP buffer content and are zero here.

namespace {
constexpr std::uint32_t mc_get_info=1,mc_init=0xFE;
constexpr std::uint32_t mc_no_card=static_cast<std::uint32_t>(-11);
constexpr unsigned mc_info_size=192;
}

bool EeRuntime::nativeCardCall(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server) {
    if(r.send_size!=48 && memory_card_folder.empty())throw UnsupportedSyscall(0x77,context->pc);
    std::array<std::uint32_t,12> in{};for(unsigned n=0;n<12 && n*4+4<=r.send_size;++n)in[n]=read32(rdram,r.send_source+n*4);
    if(r.rpc_number==mc_init) {
        if(r.recv_size!=12)throw UnsupportedSyscall(0x77,context->pc);
        write32(rdram,r.receive,0);write32(rdram,r.receive+4,0x210);write32(rdram,r.receive+8,0x226);
        completeNativeCall(rdram,context,r,server);
        ++mc_rpc_calls;++sif_dma_submissions;++native_card_requests;context->r[2].low=0x720002;return true;
    }
    if(r.rpc_number==mc_get_info) {
        // Port 0 / slot 0 only; the end parameter address must match the client's end parameter.
        const auto info=in[7];
        if(r.recv_size!=4 || in[0] || in[1] || info!=read32(rdram,r.client+32))throw UnsupportedSyscall(0x77,context->pc);
        checkMemory(rdram,info,mc_info_size);
        for(unsigned n=0;n<mc_info_size;n+=4)write32(rdram,info+n,0); // Type 0, free 0.
        if(memory_card_folder.empty())write32(rdram,r.receive,mc_no_card);
        else {
            // A card folder is in the slot: a formatted PS2 card. The game's end function reads the
            // type at +0, the free space (in 1 KB clusters) at +4 and the format flag at +0x90.
            // The first query after the card appeared answers -1 (a new, formatted card), later ones 0.
            write32(rdram,info,2);write32(rdram,info+4,memoryCardFreeClusters());write32(rdram,info+0x90,1);
            write32(rdram,r.receive,memory_card_reported_?0u:static_cast<std::uint32_t>(-1));
            memory_card_reported_=true;
        }
        completeNativeCall(rdram,context,r,server);
        ++sif_dma_submissions;++native_card_requests;
        if(mc_rpc_calls==1){++mc_rpc_calls;context->r[2].low=0x720003;}
        else {++mc_frame_queries;context->r[2].low=0x750000+mc_frame_queries;}
        return true;
    }
    if(!memory_card_folder.empty()) {
        // File requests on the card folder (services/card_folder.h). Two parameter forms, as in the
        // public library: by descriptor (48 bytes: fd, port, slot, size, offset, origin, buffer, end
        // parameter) and by name (1,044 bytes: port, slot, flags, maximum entries, a pointer, the name).
        if(!memory_card_)memory_card_.emplace(memory_card_folder);
        auto& card=*memory_card_;
        const auto name=[&] {
            if(r.send_size!=1044)throw std::runtime_error("Memory card request by name with an unexpected parameter size");
            std::string text;
            for(unsigned n=0;n<1024;++n) {const auto c=static_cast<char>(read8(rdram,r.send_source+20+n));if(!c)break;text.push_back(c);}
            return text;
        };
        std::int32_t result=0;bool handled=true;
        switch(r.rpc_number) {
        case 2:result=card.open(name(),in[2]);break;                         // Open: flags in word 2.
        case 3:result=card.close(std::int32_t(in[0]));break;                 // Close.
        case 4:result=card.seek(std::int32_t(in[0]),std::int32_t(in[4]),in[5]);break; // Seek: offset, origin.
        case 5: {                                                            // Read: size in word 3, buffer in word 6.
            // The original sends the unaligned head and tail of the data through the end parameter
            // (two sizes at +0 and +4, copied by the game's end function); here the whole data goes
            // to the buffer and both sizes are zero.
            checkMemory(rdram,in[6],in[3]);
            std::vector<std::uint8_t> data(in[3]);
            result=card.read(std::int32_t(in[0]),data);
            for(std::int32_t n=0;n<result;++n)write8(rdram,in[6]+std::uint32_t(n),data[std::size_t(n)]);
            if(in[7]) {checkMemory(rdram,in[7],8);write32(rdram,in[7],0);write32(rdram,in[7]+4,0);}
            break;
        }
        case 0xC:result=card.changeDirectory(name());break;                  // Change directory.
        case 0xD: {                                                          // List: flags 0 starts, otherwise continues.
            if(!in[2]) {memory_card_listing_=card.list(name());memory_card_listed_=0;}
            const std::size_t count=std::min<std::size_t>(in[3],memory_card_listing_.size()-memory_card_listed_);
            checkMemory(rdram,in[4],count*64);
            for(std::size_t n=0;n<count;++n) {
                // One 64-byte entry: created, modified (8 bytes each), size, mode, 10 reserved bytes, the name (32).
                const auto& e=memory_card_listing_[memory_card_listed_++];const std::uint32_t at=in[4]+std::uint32_t(n)*64;
                for(unsigned b=0;b<64;++b)write8(rdram,at+b,0);
                for(unsigned b=0;b<8;++b) {write8(rdram,at+b,e.created[b]);write8(rdram,at+8+b,e.modified[b]);}
                write32(rdram,at+16,e.size);write16(rdram,at+20,e.mode);
                for(unsigned b=0;b<31 && b<e.name.size();++b)write8(rdram,at+32+b,std::uint8_t(e.name[b]));
            }
            result=std::int32_t(count);break;
        }
        default:handled=false;
        }
        if(handled) {
            if(r.recv_size!=4)throw std::runtime_error("Memory card request with an unexpected reply size");
            write32(rdram,r.receive,std::uint32_t(result));
            completeNativeCall(rdram,context,r,server);
            ++sif_dma_submissions;++native_card_requests;++mc_frame_queries;++memory_card_file_requests;
            context->r[2].low=0x750000+mc_frame_queries;
            return true;
        }
        char text[200];std::snprintf(text,sizeof text,"Memory card request %u is not implemented (send %u bytes, receive %u; words %X %X %X %X %X %X %X %X)",
            r.rpc_number,r.send_size,r.recv_size,in[0],in[1],in[2],in[3],in[4],in[5],in[6],in[7]);
        throw std::runtime_error(text);
    }
    throw UnsupportedSyscall(0x77,context->pc);
}

// Free space of the card folder in 1 KB clusters: an 8 MB card's 8,000 less what the saves take, each
// file rounded up to whole clusters plus two for its directory entry (an estimate of the card's own
// bookkeeping; the game only compares it with what a save needs).
std::uint32_t EeRuntime::memoryCardFreeClusters() const {
    std::uint64_t used=0;std::error_code error;
    for(std::filesystem::recursive_directory_iterator it(memory_card_folder,error),end;!error && it!=end;it.increment(error))
        used+=it->is_regular_file(error)?(it->file_size(error)+1023)/1024+2:2;
    return used>=8000?0u:static_cast<std::uint32_t>(8000-used);
}
