#include <set>
#include "module_service.h"
#include "disc_native.h"
#include "iop/devices/disc_metadata.h"
#include "iop/devices/disc_sectors.h"
void bt2_iop_disc(IopRuntime&,IopContext&);

// Disc services (CDVD): file search, readiness, S-commands and sector reads from the local disc image.

std::uint32_t NativeIopService::callDiscSearch(unsigned epoch,std::span<const std::uint8_t> input,std::span<std::uint8_t> ee,const std::string& path) {
    static const std::array<std::string,6> names={"\\DATA\\ZS2US.DIR;1","\\DATA\\ZS2US_1.AFS;1","\\DATA\\ZS2US_2.AFS;1","\\DATA\\ZS2USOP.SFD;1","\\DATA\\ZS2USED.SFD;1","\\DATA\\LOGO_O.SFD;1"};

    if(epoch!=epoch_ || failed_ || disc_search_calls>=names.size() || (disc_search_calls && disc_read_calls!=1) || input.size()!=300 || ee.size()!=32*1024*1024
        || std::memcmp(input.data()+36,names[disc_search_calls].c_str(),names[disc_search_calls].size()+1)
        || input[292]!=0 || input[293]!=0x86 || input[294]!=0x3A || input[295] || input[296] || input[297] || input[298] || input[299])
        throw std::runtime_error("Unsupported owned disc search request");
    DiscMetadata disc(path);const auto metadata=disc.lookup(names[disc_search_calls]);
    if(!metadata)throw std::runtime_error("Validated local directory index missing");
    unsigned lba=0,size=0;std::memcpy(&lba,metadata->data(),4);std::memcpy(&size,metadata->data()+4,4);
    const std::array<std::pair<unsigned,unsigned>,6> extents={{{0xE2A,64},{0x1FC26,0x419B9800},{0xA2F9A,0x60014000},{0x14245,0x5CF0800},{0xE2B,0x9A0D000},{0x8B1,0x2BC800}}};
    if(std::pair{lba,size}!=extents[disc_search_calls])throw std::runtime_error("Disc revision/search metadata differs");
    const auto saved=runtime.providers;const auto prior_dma=disc_dma_submissions;
    bool locked=false;unsigned locks=0,lookups=0;
    try {
        if(!disc_search_calls) {runtime.map(iop_disc_base,iop_disc_image);
        definitions_.push_back({"boot:CDVDFSV.search",iop_disc_base,iop_disc_base+0x754,iop_disc_gp,iop_disc_image,
            [](IopRuntime&){},bt2_iop_disc,[](const IopRuntime&){throw std::runtime_error("CDVDFSV boot residency not implemented");}});}
        for(unsigned n=0;n<input.size();++n)runtime.write(iop_disc_base+0x6498+n,input[n],1);
        runtime.providers[{"cdvdman",50}]=[&](IopRuntime& rt,IopContext& c) {
            const auto value=rt.read(c.get(5),4);
            if(c.pc!=iop_disc_base+0x4B68 || c.get(4)!=0xFFFFFFF6 || (value!=0xFF && value!=0))throw std::runtime_error("Unexpected search mutex operation");
            // Bounded serialized request: acquire the owned free search bit, then release it.
            if(value==0xFF){if(locked || locks++)throw std::runtime_error("Search mutex already owned");locked=true;}
            else {if(!locked || lookups!=1 || disc_dma_submissions!=prior_dma+1)throw std::runtime_error("Search release before completion");locked=false;}
            return value;
        };
        runtime.providers[{"cdvdman",84}]=[&](IopRuntime& rt,IopContext& c) {
            if(!locked || lookups++ || c.pc!=iop_disc_base+0x4BB0 || c.get(4)!=iop_disc_base+0x6498 || c.get(5)!=iop_disc_base+0x64BC || c.get(6))throw std::runtime_error("Unsupported layer/search destination");
            for(unsigned n=0;n<32;++n)rt.write(c.get(4)+n,(*metadata)[n],1);
            return 1u;
        };
        runtime.providers[{"sifman",7}]=[&](IopRuntime& rt,IopContext& c) {
            if(c.pc!=iop_disc_base+0x4A3C || c.get(4)!=iop_disc_base+0x53B0 || c.get(5)!=1 || disc_dma_submissions!=prior_dma || !locked || lookups!=1)throw std::runtime_error("Unexpected metadata DMA submit");
            const std::array<unsigned,4> expected={iop_disc_base+0x6498,0x3A8600,36,0};
            for(unsigned n=0;n<4;++n)if(rt.read(c.get(4)+n*4,4)!=expected[n])throw std::runtime_error("Metadata DMA descriptor differs");
            for(unsigned n=0;n<36;++n)ee[0x3A8600+n]=static_cast<std::uint8_t>(rt.read(expected[0]+n,1));
            ++disc_dma_submissions;return 0x750001u;
        };
        IopContext c;c.pc=iop_disc_base+0x754;c.set(4,0);c.set(5,iop_disc_base+0x6498);c.set(6,300);
        c.set(28,iop_disc_gp);c.set(29,0x1FFF00);c.set(31,IopRuntime::sentinel);runtime.execute(runtime,c);
        if(c.pc!=IopRuntime::sentinel || c.get(2)!=iop_disc_base+0x5808 || runtime.read(c.get(2),4)!=1 || locked || locks!=1 || disc_dma_submissions!=prior_dma+1 || disc_dma_polls)throw std::runtime_error("Original search handler completion differs");
        runtime.providers=saved;++disc_search_calls;return 1;
    }catch(...){runtime.providers=saved;failed_=true;throw;}
}

std::uint32_t NativeIopService::callDiscReady(unsigned epoch) {
    // First query precedes the directory read; later queries follow the completed read and searches.
    const bool first=!disc_ready_calls && disc_search_calls==1;
    const bool repeat=disc_ready_calls && disc_read_calls && disc_search_calls==6;
    // NCMD 14 sends no data, so a repeat leaves the previous read request in the receive buffer.
    if(epoch!=epoch_ || failed_ || !(first || repeat) || runtime.disc_type_known
        || (first && runtime.read(iop_disc_base+0x6088,4)))
        throw std::runtime_error("Unsupported owned disc readiness request: ready="+std::to_string(disc_ready_calls)
            +" read="+std::to_string(disc_read_calls)+" search="+std::to_string(disc_search_calls)
            +" pending="+std::to_string(runtime.read(iop_disc_base+0x6088,4)));
    // One serialized readiness request owns acquire -> state queries -> release.
    // SC(-3)/SC(-12) ignore param; residual stack bytes are not input state.
    const auto saved=runtime.providers;unsigned stage=0;
    try {
        // A repeat sees the same idle drive: identical disc-type/status register sequence.
        runtime.disc_type_known=true;runtime.disc_type_reads=0;runtime.disc_ready_status_reads=0;
        runtime.providers[{"cdvdman",50}]=[&](IopRuntime& rt,IopContext& c) {
            if(c.pc!=iop_disc_base+0x4B68)throw std::runtime_error("Disc readiness import differs");
            const auto value=c.get(4)==0xFFFFFFF6?rt.read(c.get(5),4):0u;
            if(stage==0 && c.get(4)==0xFFFFFFF6 && value==14){++stage;return 14u;}
            if(stage==1 && c.get(4)==0xFFFFFFFD){++stage;return 0u;}
            if(stage==2 && c.get(4)==0xFFFFFFF4){++stage;return 1u;}
            if(stage==3 && c.get(4)==0xFFFFFFF6 && value==0){++stage;return 0u;}
            throw std::runtime_error("Unsupported disc readiness state transition");
        };
        IopContext c;c.pc=iop_disc_base+0x3ABC;c.set(4,14);c.set(5,iop_disc_base+0x6088);c.set(6,0);
        c.set(28,iop_disc_gp);c.set(29,0x1FFF00);c.set(31,IopRuntime::sentinel);runtime.execute(runtime,c);
        if(c.pc!=IopRuntime::sentinel || c.get(2)!=iop_disc_base+0x5830 || runtime.read(c.get(2),4)!=2
            || stage!=4 || runtime.disc_type_reads!=2 || runtime.disc_ready_status_reads!=1)throw std::runtime_error("Original readiness handler completion differs");
        runtime.providers=saved;runtime.disc_type_known=false;++disc_ready_calls;return 2;
    }catch(const std::exception& e){const auto detail=std::string(e.what())+" / pc="+std::to_string(runtime.executing_pc)+" / a0="+std::to_string(runtime.last_import_arguments[0])+" / a1="+std::to_string(runtime.last_import_arguments[1]);runtime.providers=saved;runtime.disc_type_known=false;failed_=true;throw std::runtime_error(detail);}
}

std::uint32_t NativeIopService::callDiscScmd(unsigned epoch,unsigned command) {
    // Original CDVDFSV S-command handler (retail 0x41BC8, registered for 0x80000593). Only the
    // reached no-input GetError (4) after the completed reads is admitted; it reads driver state.
    if(epoch!=epoch_ || failed_ || command!=4 || disc_read_calls<2 || runtime.disc_type_known)
        throw std::runtime_error("Unsupported owned S-command request");
    const auto saved=runtime.providers;
    try {
        // Driver state after the completed owned reads: no error (as the read handler observed).
        runtime.providers[{"cdvdman",8}]=[&](IopRuntime&,IopContext& c) {
            if(c.pc!=iop_disc_base+0x4AD8)throw std::runtime_error("S-command error query differs");
            return 0u;
        };
        IopContext c;c.pc=iop_disc_base+0x3E98;c.set(4,command);c.set(5,iop_disc_base+0x5C78);c.set(6,0);
        c.set(28,iop_disc_gp);c.set(29,0x1FFF00);c.set(31,IopRuntime::sentinel);runtime.execute(runtime,c);
        if(c.pc!=IopRuntime::sentinel || !c.get(2))throw std::runtime_error("Original S-command handler completion differs");
        runtime.providers=saved;++disc_scmd_calls;return runtime.read(c.get(2),4);
    }catch(const std::exception& e){runtime.providers=saved;failed_=true;throw std::runtime_error(std::string(e.what())+" / pc="+std::to_string(runtime.executing_pc)+" / a0="+std::to_string(runtime.last_import_arguments[0]));}
}

std::uint32_t NativeIopService::callDiscRead(unsigned epoch,std::span<const std::uint8_t> input,std::span<std::uint8_t> ee,const std::string& path) {
    // NCMD 1 input: LBN, sector count, EE destination, mode word, completion record, progress word.
    // The first request is the observed directory read; later requests follow a readiness query
    // and use the same original handler path with their own LBN/count/destination.
    const std::array<unsigned,6> expected={0xE2A,2,0x39DE80,0x100,0x3694C0,0x369580};
    std::array<unsigned,6> in{};
    if(input.size()==sizeof(in))std::memcpy(in.data(),input.data(),sizeof(in));
    const bool first=!disc_read_calls;
    const bool repeat=disc_read_calls && disc_ready_calls>disc_read_calls; // A readiness query precedes each read.
    if(epoch!=epoch_ || failed_ || input.size()!=sizeof(expected) || ee.size()!=32*1024*1024
        || (first ? (disc_ready_calls!=1 || in!=expected)
                  : (!repeat || !in[1] || in[1]>512 || in[2]%64 || in[2]<0x100000 || in[2]+in[1]*2048>ee.size()
                     || (in[3]!=0 && in[3]!=0x100) || in[4]!=expected[4] || in[5]!=expected[5])))
        throw std::runtime_error("Unsupported owned disc read");
    const auto lbn=in[0],count=in[1],dest=in[2],mode=in[3],bytes=count*2048;
    // Requests above 16 sectors stream through the 16-sector FSV ring: the handler reprograms
    // CDVD DMA channel 3 for each further chunk (retail block layout: 43 blocks of 12 words).
    constexpr unsigned ring_sectors=16,block_words=12,blocks_per_sector=43;
    const unsigned chunks=(count+ring_sectors-1)/ring_sectors;
    unsigned ring_offset=0,ring_count=0,signalled=0,sector_dmas=0,completions=0,progress_dmas=0,last_progress=0;
    std::uint32_t dma3_madr=0,dma3_bcr=0,dma3_chcr=0;bool dma3_stopped=false;
    const unsigned dma_base=disc_read_dma_submissions,alarm_base=disc_read_alarm_cancels,set_base=disc_read_alarm_sets;
    const auto payload=read_iso_sectors(path,lbn,count);
    const auto saved=runtime.providers;const auto event=runtime.next_event++;
    const auto wait=saved.at({"thevent",10});unsigned stage=0;bool accepted=false,callback=false;
    constexpr unsigned workspace=0x1D4B00,buffer=0x1D5B00,raw=buffer+0x1248,token=0x770001;
    auto fill_ring=[&](IopRuntime& rt,unsigned offset,unsigned sectors) {
        for(unsigned sector=0;sector<sectors;++sector){const auto at=raw+sector*2064,id=0x20030000+lbn+offset+sector;
            for(unsigned n=0;n<4;++n)rt.write(at+n,(id>>(8*(3-n)))&255,1);
            for(unsigned n=0;n<2048;++n)rt.write(at+12+n,payload[(offset+sector)*2048+n],1);
        }
        ring_offset=offset;ring_count=sectors;
    };
    try {
        if(first){runtime.map(workspace,std::vector<std::uint8_t>(256));runtime.map(buffer,std::vector<std::uint8_t>(42128));}
        runtime.events.emplace(event,IopRuntime::Event{0,0,0});
        // Authored inactive shared workspace and owned FSV buffer. No live retail state is copied.
        runtime.write(iop_disc_base+0x5568,workspace,4);runtime.write(iop_disc_base+0x53A0,buffer,4);
        runtime.write(iop_disc_base+0x5398,buffer,4);runtime.write(iop_disc_base+0x539C,iop_disc_base+0x5390,4);
        runtime.write(iop_disc_base+0x5550,event,4);
        for(unsigned n=0;n<input.size();++n)runtime.write(iop_disc_base+0x6088+n,input[n],1);
        runtime.disc_type_reads=0;runtime.disc_type_known=true;
        runtime.providers[{"cdvdman",50}]=[&](IopRuntime& rt,IopContext& c) {
            const auto command=c.get(4);
            if(c.pc!=iop_disc_base+0x4B68)throw std::runtime_error("Read SC import differs");
            if(stage==0 && command==0xFFFFFFF6 && rt.read(c.get(5),4)==1){++stage;return 1u;}
            if(stage==1 && command==0xFFFFFFFC){++stage;return 0u;}
            if(stage==2 && command==0xFFFFFFEA){++stage;return 1u;}
            if(stage==3 && command==0xFFFFFFE9 && rt.read(c.get(5),4)==lbn){++stage;return lbn;}
            if(stage==4 && command==0xFFFFFFF1){++stage;return 15000u;}
            if(command==0xFFFFFFF6 && rt.read(c.get(5),4)==0 && callback && disc_read_dma_submissions>dma_base){++stage;return 0u;}
            throw std::runtime_error("Unsupported read SC operation");
        };
        runtime.providers[{"thbase",35}]=[&](IopRuntime& rt,IopContext& c) {
            if(stage!=5 || disc_read_alarm_sets!=set_base || c.pc!=iop_disc_base+0x4980 || c.get(4)!=iop_disc_base+0x5548 || c.get(5)!=iop_disc_base+0x8C8 || c.get(6)!=c.get(4)
                || rt.read(c.get(4),4)!=552960000 || rt.read(c.get(4)+4,4))throw std::runtime_error("Unsupported disc timeout registration");
            disc_read_alarm={rt.logical_time_us+15000000,c.get(5),c.get(6),c.get(28)};++disc_read_alarm_sets;return 0u;
        };
        runtime.providers[{"thbase",37}]=[&](IopRuntime&,IopContext& c) {
            if(!callback || !disc_read_alarm.deadline || disc_read_alarm_cancels!=alarm_base || c.pc!=iop_disc_base+0x4988 || c.get(4)!=disc_read_alarm.callback || c.get(5)!=disc_read_alarm.argument)
                throw std::runtime_error("Unsupported disc timeout cancellation");
            disc_read_alarm={};++disc_read_alarm_cancels;return 0u;
        };
        runtime.providers[{"cdvdman",63}]=[&](IopRuntime& rt,IopContext& c) {
            if(stage!=5 || accepted || c.pc!=iop_disc_base+0x4B80 || c.get(4)!=lbn || c.get(5)!=count || c.get(6)!=raw || c.get(7)!=iop_disc_base+0x54F0
                || rt.read(c.get(29)+16,4)!=std::min(count,ring_sectors) || rt.read(c.get(29)+20,4)!=iop_disc_base+0xB00 || rt.read(c.get(7),4)!=mode)
                throw std::runtime_error("Unsupported RV destination/mode/callback: buf="+std::to_string(c.get(6))+" modeptr="+std::to_string(c.get(7))
                    +" arg5="+std::to_string(rt.read(c.get(29)+16,4))+" cb="+std::to_string(rt.read(c.get(29)+20,4))+" mode="+std::to_string(rt.read(c.get(7),4)));
            // sceCdRV arg5 is the cache (ring) sector count. Captured DVD records contain a 12-byte
            // header, 2048 ISO bytes and four trailing bytes. CDVDMAN's DMA block fields come from
            // the retail workspace (12-word blocks, 43 per sector).
            rt.write(workspace+164,block_words,2);rt.write(workspace+166,blocks_per_sector,2);
            fill_ring(rt,0,std::min(count,ring_sectors));
            accepted=true;return 1u;
        };
        auto deliver_read_completion=[&](IopRuntime& rt) {
                if(!accepted || !rt.cpu_interrupts_enabled || rt.in_interrupt || rt.events.at(event).bits)
                    throw std::runtime_error("Read completion before owned bytes/interrupt eligibility");
                IopContext completion;completion.pc=iop_disc_base+0xB00;completion.set(4,workspace);
                completion.set(28,iop_disc_gp);completion.set(29,0x1FF700);completion.set(31,IopRuntime::sentinel);
                rt.in_interrupt=true;try{rt.execute(rt,completion);rt.in_interrupt=false;}catch(...){rt.in_interrupt=false;throw;}
                if(completion.pc!=IopRuntime::sentinel || completion.get(2)!=0 || rt.events.at(event).bits!=0x20)
                    throw std::runtime_error("Original read callback effects differ");
                // One callback per ring chunk; the read completes with the last chunk.
                callback=true;++signalled;if(ring_offset+ring_count==count)++disc_read_callbacks;
        };
        runtime.providers[{"thevent",10}]=[&](IopRuntime& rt,IopContext& c) {
            if(c.get(4)==event && c.get(5)==0x20 && !callback)throw std::runtime_error("Disc wait before synchronization completed");
            return wait(rt,c);
        };
        runtime.providers[{"cdvdman",11}]=[&](IopRuntime& rt,IopContext& c) {
            const bool all=ring_offset+ring_count==count && signalled==chunks;
            if(!accepted || c.pc!=iop_disc_base+0x4AF0 || (c.get(4)!=32 && !(c.get(4)==5 && all)))throw std::runtime_error(std::string("Unsupported owned read synchronization / accepted=")+(accepted?"1":"0")+" callback="+(callback?"1":"0"));
            if(c.get(4)==32) {
                // The current ring chunk's data and DMA completion; the next chunk needs a DMA restart.
                if(signalled!=ring_offset/ring_sectors || sector_dmas!=signalled)throw std::runtime_error("Read chunk synchronization out of order");
                deliver_read_completion(rt);
            }
            return 0u;
        };
        runtime.providers[{"cdvdman",8}]=[&](IopRuntime&,IopContext& c) {
            if(!callback || c.pc!=iop_disc_base+0x4AD8)throw std::runtime_error("Read error query before data completion");
            return 0u;
        };
        runtime.providers[{"intrman",7}]=[&](IopRuntime& rt,IopContext& c) {
            if(!callback || c.pc!=iop_disc_base+0x48A4 || c.get(4)!=35 || rt.enabled_interrupts.contains(35))
                throw std::runtime_error("Unexpected disc DMA3 interrupt ownership");
            if(c.get(5))rt.write(c.get(5),0xFFFFFF99,4);
            return 0xFFFFFF99u; // Actual driver completion already disabled DMA3.
        };
        runtime.providers[{"dmacman",8}]=[&](IopRuntime& rt,IopContext& c) {
            if(c.get(4)!=3 || c.pc!=iop_disc_base+0x490C || sector_dmas!=signalled || !signalled || ring_offset+ring_count>=count)
                throw std::runtime_error("Unsupported CDVD DMA control");
            if(!c.get(5)) {dma3_stopped=true;dma3_chcr=0;return 0u;}
            const auto next=std::min(ring_sectors,count-ring_offset-ring_count);
            if(!dma3_stopped || c.get(5)!=0x41000200 || dma3_madr!=raw || dma3_bcr!=((blocks_per_sector*next)<<16|block_words)
                || rt.read(workspace+0xA8,4)!=raw)throw std::runtime_error("Unsupported CDVD DMA restart");
            // The drive continues the original request: the next chunk streams into the ring start.
            fill_ring(rt,ring_offset+ring_count,next);dma3_stopped=false;dma3_chcr=c.get(5);dma3_madr=dma3_bcr=0;return 0u;
        };
        runtime.providers[{"dmacman",9}]=[&](IopRuntime&,IopContext& c) {
            if(c.get(4)!=3 || c.pc!=iop_disc_base+0x4914)throw std::runtime_error("Unsupported CDVD DMA status");
            return dma3_chcr;
        };
        runtime.providers[{"dmacman",4}]=[&](IopRuntime&,IopContext& c) {
            if(c.get(4)!=3 || c.pc!=iop_disc_base+0x48FC || !dma3_stopped)throw std::runtime_error("Unsupported CDVD DMA address");
            dma3_madr=c.get(5);return 0u;
        };
        runtime.providers[{"dmacman",6}]=[&](IopRuntime&,IopContext& c) {
            if(c.get(4)!=3 || c.pc!=iop_disc_base+0x4904 || !dma3_stopped)throw std::runtime_error("Unsupported CDVD DMA block count");
            dma3_bcr=c.get(5);return 0u;
        };
        std::set<unsigned> polled;
        runtime.providers[{"sifman",7}]=[&](IopRuntime& rt,IopContext& c) {
            if(!callback || c.pc!=iop_disc_base+0x4A3C)throw std::runtime_error("Sector DMA before owned completion");
            if(c.get(4)==iop_disc_base+0x53E0) {
                // One sector DMA per signalled ring chunk, to the chunk's EE offset.
                if(c.get(5)!=ring_count || sector_dmas+1!=signalled)throw std::runtime_error("Unsupported sector DMA");
                for(unsigned sector=0;sector<ring_count;++sector) {
                    const std::array<unsigned,4> d={raw+sector*2064+12,dest+(ring_offset+sector)*2048,2048,0};
                    for(unsigned n=0;n<4;++n)if(rt.read(c.get(4)+sector*16+n*4,4)!=d[n])throw std::runtime_error("Sector DMA descriptor differs");
                    for(unsigned n=0;n<2048;++n)ee[d[1]+n]=static_cast<std::uint8_t>(rt.read(d[0]+n,1));
                }
                ++sector_dmas;
            }else {
                const bool progress=c.get(4)==iop_disc_base+0x5508;
                if(c.get(5)!=1 || (!progress && (c.get(4)!=iop_disc_base+0x54F8 || completions || sector_dmas!=chunks)))throw std::runtime_error("Unsupported read completion DMA / batch="+std::to_string(disc_read_dma_submissions)+" src="+std::to_string(rt.read(c.get(4),4))+" dst="+std::to_string(rt.read(c.get(4)+4,4))+" bytes="+std::to_string(rt.read(c.get(4)+8,4))+" value="+std::to_string(rt.read(rt.read(c.get(4),4),4)));
                const std::array<unsigned,4> d={iop_disc_base+(progress?0x5818:0x5580),progress?0x369580u:0x3694C0u,progress?4u:144u,0};
                for(unsigned n=0;n<4;++n)if(rt.read(c.get(4)+n*4,4)!=d[n])throw std::runtime_error("Read completion descriptor differs");
                if(progress) {
                    // Progress only grows (chunked reads add the next chunk ahead); the final value is every byte.
                    const auto value=rt.read(d[0],4);
                    if(value>bytes || value<last_progress || (completions && value!=bytes))throw std::runtime_error("Read progress differs: "+std::to_string(value));
                    last_progress=value;++progress_dmas;
                } else ++completions;
                if(!progress && (rt.read(d[0],4) || rt.read(d[0]+4,4) || rt.read(d[0]+8,4)!=dest || rt.read(d[0]+12,4)!=dest+bytes))throw std::runtime_error("Owned completion range differs");
                for(unsigned n=0;n<d[2];++n)ee[d[1]+n]=static_cast<std::uint8_t>(rt.read(d[0]+n,1));
            }
            return token+disc_read_dma_submissions++;
        };
        runtime.providers[{"sifman",8}]=[&](IopRuntime&,IopContext& c) {
            if(c.pc==iop_disc_base+0x4A44 && c.get(4)==0)return 0xFFFFFFFFu; // No prior DMA exists in the fresh owned buffer.
            if(c.pc!=iop_disc_base+0x4A44 || c.get(4)<token+dma_base || c.get(4)>=token+disc_read_dma_submissions)throw std::runtime_error("Unsupported sector DMA poll");
            if(polled.insert(c.get(4)).second)++disc_read_dma_polls;
            return 0xFFFFFFFFu; // These owned copies have completed; hardware poll counts are not equated.
        };
        IopContext c;c.pc=iop_disc_base+0x3ABC;c.set(4,1);c.set(5,iop_disc_base+0x6088);c.set(6,24);
        c.set(28,iop_disc_gp);c.set(29,0x1FFF00);c.set(31,IopRuntime::sentinel);runtime.execute(runtime,c);
        if(c.pc!=IopRuntime::sentinel || stage!=6 || !accepted || !callback || signalled!=chunks || sector_dmas!=chunks || completions!=1
            || progress_dmas<2 || last_progress!=bytes || disc_read_dma_submissions!=dma_base+chunks+progress_dmas+1 || !disc_read_dma_polls || disc_read_alarm.deadline || disc_read_alarm_cancels!=alarm_base+1)
            throw std::runtime_error("Original sector handler incomplete");
        // The original handler resubmits its updated progress word after the completion buffer.
        if(runtime.read(iop_disc_base+0x5818,4)!=bytes)throw std::runtime_error("Final owned progress differs");
        runtime.providers=saved;runtime.disc_type_known=false;++disc_read_calls;return runtime.read(c.get(2),4);
    }catch(const std::exception& e){const auto detail=std::string(e.what())+" / pc="+std::to_string(runtime.executing_pc)+" / a0="+std::to_string(runtime.last_import_arguments[0])+" / a1="+std::to_string(runtime.last_import_arguments[1]);runtime.providers=saved;runtime.disc_type_known=false;failed_=true;throw std::runtime_error(detail);}
}
