#include "module_service.h"
#include "fileio_boot_native.h"
#include "iop/devices/disc_metadata.h"

// Boot FILEIO file server (0x80000001): original handler, cdrom0: files read from the local disc image.

std::vector<std::uint8_t> NativeIopService::callFileio(unsigned epoch,std::uint32_t command,std::span<const std::uint8_t> input,unsigned result_size,std::span<std::uint8_t> ee,const std::string& disc_path) {
    // FILEIO's server thread is not started natively; the registered handler (base+0x1E68) runs on an
    // owned stack with the request in its receive buffer (base+0x33A0), as the heap handler does.
    if(epoch!=epoch_ || failed_ || !boot_heap_allocations || input.size()>0x800 || result_size>0x800 || ee.size()!=32u*1024*1024 || runtime.current_thread || disc_path.empty())
        throw std::runtime_error("Unsupported FILEIO request");
    const auto buffer=iop_fileio_boot_base+0x33A0;
    const auto saved=runtime.providers;
    try {
        if(!fileio_requests) {
            // FILEIO's module entry is not executed natively. Its kernel objects are created as the entry
            // does (base+0x23C0..0x240C): the SIF-command lock semaphore (attr 1, count 1, max 1) at
            // base+0x3300 and an event flag (attr 2, bit 1 set) at base+0x3304; both IDs are present at
            // the same offsets in captured IOP RAM (rpc-sound-bank-after: retail 0x47730/0x47734).
            if(command!=0xFF || runtime.read(iop_fileio_boot_base+0x3300,4) || runtime.read(iop_fileio_boot_base+0x3304,4))
                throw std::runtime_error("FILEIO module objects outside initialization");
            const auto sema=runtime.next_semaphore++;runtime.semaphores.emplace(sema,IopRuntime::Semaphore{1,0,1,1});
            const auto event=runtime.next_event++;runtime.events.emplace(event,IopRuntime::Event{2,0,1});
            runtime.write(iop_fileio_boot_base+0x3300,sema,4);runtime.write(iop_fileio_boot_base+0x3304,event,4);
        }
        runtime.check(buffer,static_cast<unsigned>(input.size()));
        std::copy(input.begin(),input.end(),runtime.ram.begin()+buffer);
        // IOP -> EE transfers of results/data: copied at once into checked EE memory.
        runtime.providers[{"sifman",7}]=[this,ee](IopRuntime& rt,IopContext& c) {
            const auto count=c.get(5);if(!count || count>16)throw std::runtime_error("Unsupported FILEIO SIF DMA count");
            for(unsigned n=0;n<count;++n) {
                const auto d=c.get(4)+n*16;const auto src=rt.read(d,4),dst=rt.read(d+4,4)&0x1FFFFFFF,size=rt.read(d+8,4);
                rt.check(IopRuntime::physical(src),size);
                if(!size || dst>=ee.size() || size>ee.size()-dst)throw std::runtime_error("FILEIO SIF DMA outside EE memory");
                for(unsigned k=0;k<size;++k)ee[dst+k]=static_cast<std::uint8_t>(rt.read(src+k,1));
            }
            return 0x780001+fileio_dma_submissions++;
        };
        runtime.providers[{"sifman",8}]=[](IopRuntime&,IopContext&) {return 0xFFFFFFFFu;}; // Owned copies completed.
        // sceSifSendCmd / isceSifSendCmd to the EE: extra data is copied now; the header is queued
        // and delivered to the EE command handler after the RPC returns.
        auto send_command=[this,ee](IopRuntime& rt,IopContext& c) {
            const auto cid=c.get(4),packet=c.get(5),size=c.get(6),src=c.get(7);
            const auto dest=rt.read(c.get(29)+16,4)&0x1FFFFFFF,extra=rt.read(c.get(29)+20,4);
            if(size!=16 || (cid!=0x80000011 && cid!=0x80000013))throw std::runtime_error("Unsupported FILEIO SIF command");
            rt.check(IopRuntime::physical(packet),16);
            if(extra) {
                rt.check(IopRuntime::physical(src),extra);
                if(dest>=ee.size() || extra>ee.size()-dest)throw std::runtime_error("FILEIO SIF command data outside EE memory");
                for(unsigned k=0;k<extra;++k)ee[dest+k]=static_cast<std::uint8_t>(rt.read(src+k,1));
            }
            EeCommand command;command.cid=cid;
            for(unsigned k=0;k<16;++k)command.header[k]=static_cast<std::uint8_t>(rt.read(packet+k,1));
            const std::array<std::uint32_t,3> words={16u|(extra<<8),dest,cid};std::memcpy(command.header.data(),words.data(),12);
            pending_ee_commands.push_back(command);++ee_commands_sent;return 0x790000+ee_commands_sent;
        };
        runtime.providers[{"sifcmd",12}]=send_command;runtime.providers[{"sifcmd",13}]=send_command;
        // Message boxes: a send to a waiting worker hands the message over and runs it at once.
        runtime.providers[{"thmsgbx",4}]=[](IopRuntime& rt,IopContext&) {const auto id=rt.next_mailbox++;rt.mailboxes[id];return id;};
        runtime.providers[{"thmsgbx",5}]=[](IopRuntime& rt,IopContext& c) {
            if(!rt.mailboxes.erase(c.get(4)))throw std::runtime_error("Unknown IOP mailbox");return 0u;};
        runtime.providers[{"thmsgbx",6}]=[](IopRuntime& rt,IopContext& c) {
            const auto box=rt.mailboxes.find(c.get(4));if(box==rt.mailboxes.end())throw std::runtime_error("Unknown IOP mailbox");
            for(auto& [id,t]:rt.threads)if(t.waiting && t.wait_mbx==c.get(4)) {
                t.waiting=false;t.ready=true;t.wait_satisfied=true;t.mbx_message=c.get(5);
                const auto parent=rt.current_thread;
                if(parent && t.priority>=rt.threads.at(parent).priority)return 0u; // Runs when the sender blocks.
                rt.current_thread=id;
                try {rt.execute(rt,t.context);rt.current_thread=parent;t.ready=false;}
                catch(const IopYield&) {rt.current_thread=parent;if(!t.waiting && !t.exited)throw std::runtime_error("IOP mailbox worker yielded without a wait");}
                catch(...) {rt.current_thread=parent;throw;}
                return 0u;
            }
            box->second.push_back(c.get(5));return 0u;
        };
        runtime.providers[{"thmsgbx",8}]=[](IopRuntime& rt,IopContext& c) {
            const auto owner=rt.threads.find(rt.current_thread);
            if(owner==rt.threads.end() || rt.in_interrupt)throw std::runtime_error("IOP mailbox receive outside a thread");
            // ReceiveMbx(void** message, int mbx).
            auto& t=owner->second;const auto out=c.get(4),id=c.get(5);const auto box=rt.mailboxes.find(id);
            if(box==rt.mailboxes.end())throw std::runtime_error("Unknown IOP mailbox");
            rt.check(IopRuntime::physical(out),4);
            if(t.wait_satisfied && t.wait_mbx==id) {t.wait_satisfied=false;t.wait_mbx=0;rt.write(out,t.mbx_message,4);return 0u;}
            if(!box->second.empty()) {rt.write(out,box->second.front(),4);box->second.pop_front();return 0u;}
            t.waiting=true;t.ready=false;t.wait_mbx=id;t.wait_order=++rt.next_wait_order;throw IopYield{};
        };
        // ExitThread from a FILEIO worker (after its close): the thread becomes dormant and its
        // borrowed host frame unwinds to the sender that ran it.
        runtime.providers[{"thbase",8}]=[](IopRuntime& rt,IopContext&) -> std::uint32_t {
            const auto owner=rt.threads.find(rt.current_thread);
            if(owner==rt.threads.end() || rt.in_interrupt || owner->second.waiting)throw std::runtime_error("IOP ExitThread outside a FILEIO worker");
            owner->second.ready=false;owner->second.exited=true;throw IopYield{};
        };
        // FILEIO's internal allocations in an owned area (0x197000..0x19F000), never seen by the EE.
        runtime.providers[{"sysmem",4}]=[this](IopRuntime& rt,IopContext& c) {
            const auto size=(c.get(5)+255)&~255u;
            if(c.get(4)!=1 || !c.get(5) || c.get(5)>0x4000 || c.get(6))throw std::runtime_error("Unsupported FILEIO AllocSysMemory");
            for(std::uint32_t at=0x197000;at+size<=0x19F000;at+=0x100) {
                bool free=true;for(auto [lo,hi]:rt.ranges)if(lo<at+size && at<hi){free=false;break;}
                if(!free)continue;
                rt.map(at,std::vector<std::uint8_t>(size));fileio_allocations[at]=size;return at;
            }
            throw std::runtime_error("FILEIO allocation area exhausted");
        };
        runtime.providers[{"sysmem",5}]=[this](IopRuntime& rt,IopContext& c) {
            const auto found=fileio_allocations.find(c.get(4));if(found==fileio_allocations.end())throw std::runtime_error("Unsupported FILEIO FreeSysMemory");
            std::erase(rt.ranges,std::pair<std::uint32_t,std::uint32_t>{found->first,found->first+found->second});fileio_allocations.erase(found);return 0u;
        };
        // IOMAN open/close/read/lseek on cdrom0: (read-only ISO9660 files from the local image).
        runtime.providers[{"ioman",4}]=[this,&disc_path](IopRuntime& rt,IopContext& c) {
            std::string name;for(unsigned n=0;n<256;++n){const auto ch=rt.read(c.get(4)+n,1);if(!ch)break;name+=static_cast<char>(ch);}
            if(name.rfind("cdrom0:",0)!=0 || c.get(5)!=1)throw std::runtime_error("Unsupported IOMAN open: "+name);
            DiscMetadata disc(disc_path);const auto found=disc.lookup(name.substr(7));
            if(!found)return static_cast<std::uint32_t>(-2); // ENOENT
            OwnedFile file;std::memcpy(&file.lba,found->data(),4);std::memcpy(&file.size,found->data()+4,4);
            const auto fd=next_cdrom_fd++;cdrom_files[fd]=file;++cdrom_opens;return fd;
        };
        runtime.providers[{"ioman",8}]=[this](IopRuntime&,IopContext& c) {
            const auto found=cdrom_files.find(c.get(4));if(found==cdrom_files.end())throw std::runtime_error("IOMAN lseek on unowned file");
            auto& f=found->second;const auto offset=static_cast<std::int32_t>(c.get(5));const auto whence=c.get(6);
            const std::int64_t base=whence==0?0:whence==1?f.position:whence==2?f.size:-1;
            if(base<0 || base+offset<0 || base+offset>f.size)throw std::runtime_error("Unsupported IOMAN lseek");
            f.position=static_cast<std::uint32_t>(base+offset);return f.position;
        };
        runtime.providers[{"ioman",6}]=[this,&disc_path](IopRuntime& rt,IopContext& c) {
            const auto found=cdrom_files.find(c.get(4));if(found==cdrom_files.end())throw std::runtime_error("IOMAN read on unowned file");
            auto& f=found->second;const auto count=std::min<std::uint32_t>(c.get(6),f.size-f.position);
            if(!count)return 0u;
            rt.check(IopRuntime::physical(c.get(5)),count);
            std::ifstream iso(disc_path,std::ios::binary);iso.seekg(static_cast<std::streamoff>(std::uint64_t(f.lba)*2048+f.position));
            std::vector<char> bytes(count);if(!iso.read(bytes.data(),count))throw std::runtime_error("Short ISO file read");
            std::copy(bytes.begin(),bytes.end(),rt.ram.begin()+IopRuntime::physical(c.get(5)));
            f.position+=count;++cdrom_reads;cdrom_bytes+=count;return count;
        };
        runtime.providers[{"ioman",5}]=[this](IopRuntime&,IopContext& c) {
            if(!cdrom_files.erase(c.get(4)))throw std::runtime_error("IOMAN close on unowned file");
            ++cdrom_closes;return 0u;
        };
        IopContext c;c.pc=iop_fileio_boot_base+0x1E68;c.set(4,command);c.set(5,buffer);c.set(6,static_cast<unsigned>(input.size()));
        c.set(28,iop_fileio_boot_gp);c.set(29,0x1FFF00);c.set(31,IopRuntime::sentinel);
        runtime.execute(runtime,c);
        runtime.providers=saved;
        if(c.pc!=IopRuntime::sentinel || (result_size && !c.get(2)))throw std::runtime_error("FILEIO handler did not return a response");
        ++fileio_requests;
        if(!result_size)return {};
        runtime.check(IopRuntime::physical(c.get(2)),result_size);
        return {runtime.ram.begin()+IopRuntime::physical(c.get(2)),runtime.ram.begin()+IopRuntime::physical(c.get(2))+result_size};
    } catch(const std::exception& e) {
        runtime.providers=saved;failed_=true;
        throw std::runtime_error(std::string(e.what())+" / FILEIO command="+std::to_string(command)+" / pc="+std::to_string(runtime.executing_pc)+" / address="+std::to_string(runtime.blocked_address));
    }
}
