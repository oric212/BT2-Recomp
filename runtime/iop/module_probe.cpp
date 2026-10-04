#include "module_service.h"
#include "library_provider.h"
#include "kernel_provider.h"
#include "sio2man_native.h"
#include <filesystem>
#include <fstream>
#include <iostream>
void bt2_iop_module(IopRuntime&,IopContext&);
int main(int argc,char** argv) {
    const std::string mode=argc==4?argv[3]:"";
    const std::vector<std::string> modes={"","--mcman","--mcserv","--dbcman","--sio2d","--ds2ud"};
    const auto selected=std::find(modes.begin(),modes.end(),mode);
    if((argc!=3 && argc!=4) || selected==modes.end()){std::cerr<<"Usage: bt2-iop-probe private-image report.json [--mcman|--mcserv|--dbcman|--sio2d|--ds2ud]\n";return 1;}
    const auto stage=selected-modes.begin();
    if(std::filesystem::weakly_canonical(std::filesystem::path(argv[2]).parent_path())!=std::filesystem::weakly_canonical("work/iop")) {
        std::cerr<<"IOP output must stay under ignored work/iop; run from project root\n";return 1;
    }
    NativeIopService service(1,bt2_compiled_iop_modules());auto& rt=service.runtime;auto& c=service.entry;bool entry_returned=false;std::string diagnostic;

    try {
        std::ifstream stream(argv[1],std::ios::binary|std::ios::ate);
        if(!stream || stream.tellg()!=iop_module_size)throw std::runtime_error("IOP private image size differs");
        std::vector<std::uint8_t> image(iop_module_size);stream.seekg(0);stream.read(reinterpret_cast<char*>(image.data()),image.size());
        if(!stream || !std::equal(image.begin(),image.end(),iop_expected_image.begin()))throw std::runtime_error("IOP image differs from generated source");
        service.load(1,"cdrom0:\\IRX\\SIO2MAN.IRX;1");
        if(stage>=1)service.load(1,"cdrom0:\\IRX\\MCMAN.IRX;1");
        if(stage>=2) {service.attachEeTransport(1);service.load(1,"cdrom0:\\IRX\\MCSERV.IRX;1");}
        if(stage>=3)service.load(1,"cdrom0:\\IRX\\DBCMAN.IRX;1");
        if(stage>=4)service.load(1,"cdrom0:\\IRX\\SIO2D.IRX;1");
        if(stage>=5)service.load(1,"cdrom0:\\IRX\\DS2U_D.IRX;1");
        entry_returned=true;diagnostic="Scoped resident effects validated; transfers remain unvalidated";
    } catch(const std::exception& e) {
        if(!rt.steps){std::cerr<<e.what()<<"\n";return 1;}
        diagnostic=e.what();entry_returned=c.pc==IopRuntime::sentinel;
    }
    {
        std::ofstream memory(std::string(argv[2])+".ram.bin",std::ios::binary);memory.write(reinterpret_cast<const char*>(rt.ram.data()),rt.ram.size());
        if(!memory){std::cerr<<"Cannot write private IOP memory\n";return 1;}
        std::ofstream report(argv[2]);if(!report){std::cerr<<"Cannot write IOP report\n";return 1;}report<<"{\"entry_returned\":"<<(entry_returned?"true":"false")<<",\"entry_result\":"<<c.get(2)<<",\"sio2_control_known\":"<<(rt.sio2_control_known?"true":"false")<<",\"sio2_control\":"<<rt.sio2_control<<",\"events\":"<<rt.events.size()<<",\"semaphores\":"<<rt.semaphores.size()<<",\"threads\":[";
        bool first_thread=true;
        for(const auto& [id,t]:rt.threads) {if(!first_thread)report<<',';first_thread=false;report<<"{\"id\":"<<id<<",\"entry\":"<<t.entry<<",\"pc\":"<<t.context.pc<<",\"waiting\":"<<(t.waiting?"true":"false")<<",\"event\":"<<t.wait_event<<",\"bits\":"<<t.wait_bits<<",\"mode\":"<<t.wait_mode<<",\"result\":"<<t.wait_result<<",\"rpc_queue\":"<<t.wait_rpc<<",\"priority\":"<<t.priority<<",\"stacksize\":"<<t.stacksize<<",\"attr\":"<<t.attr<<",\"option\":"<<t.option<<",\"gp\":"<<t.gp<<"}";}
        report<<"],\"event_objects\":[";bool first=true;
        for(const auto& [id,e]:rt.events) {if(!first)report<<',';first=false;report<<"{\"id\":"<<id<<",\"attr\":"<<e.attr<<",\"option\":"<<e.option<<",\"bits\":"<<e.bits<<"}";}
        report<<"],\"semaphore_objects\":[";first=true;
        for(const auto& [id,e]:rt.semaphores) {if(!first)report<<',';first=false;report<<"{\"id\":"<<id<<",\"attr\":"<<e.attr<<",\"option\":"<<e.option<<",\"count\":"<<e.count<<",\"maximum\":"<<e.maximum<<"}";}
        report<<"],\"interrupts\":[";first=true;
        for(const auto& [id,e]:rt.interrupts) {if(!first)report<<',';first=false;report<<"{\"irq\":"<<id<<",\"mode\":"<<e.mode<<",\"handler\":"<<e.handler<<",\"argument\":"<<e.argument<<",\"enabled\":"<<(e.enabled?"true":"false")<<"}";}
        report<<"],\"dma_channels\":[";first=true;
        for(const auto& [id,e]:rt.dma_channels) {if(!first)report<<',';first=false;report<<"{\"channel\":"<<id<<",\"priority\":"<<e.priority<<",\"enabled\":"<<(e.enabled?"true":"false")<<"}";}
        report<<"],\"pc\":"<<rt.executing_pc<<",\"steps\":"<<rt.steps<<",\"blocked_address\":"<<rt.blocked_address<<",\"access_kind\":\""<<rt.access_kind<<"\",\"access_width\":"<<rt.access_width<<",\"access_value\":"<<rt.access_value<<",\"mapped_ranges_entered\":"<<rt.entries.size()<<",\"registered_libraries\":"<<rt.libraries.size()<<",\"imports\":[";
        for(unsigned n=0;n<rt.imports.size();++n){if(n)report<<',';report<<'\"'<<rt.imports[n]<<'\"';}report<<"],\"arguments\":["<<c.get(4)<<','<<c.get(5)<<','<<c.get(6)<<','<<c.get(7)<<"]}\n";
        std::cout<<"IOP diagnostic stopped: "<<diagnostic<<"; PC=0x"<<std::hex<<rt.executing_pc<<std::dec<<", native steps="<<rt.steps<<"\n";return 0;
    }
}
