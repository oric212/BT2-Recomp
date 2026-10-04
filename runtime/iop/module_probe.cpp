#include "native_runtime.h"
#include "library_provider.h"
#include "sio2man_native.h"
#include <fstream>
#include <iostream>
void bt2_iop_module(IopRuntime&,IopContext&);
int main(int argc,char** argv) {
    if(argc!=3){std::cerr<<"Usage: bt2-iop-probe private-image report.json\n";return 1;}
    IopRuntime rt;IopContext c;
    try {
        std::ifstream stream(argv[1],std::ios::binary|std::ios::ate);
        if(!stream || stream.tellg()!=iop_module_size)throw std::runtime_error("IOP private image size differs");
        std::vector<std::uint8_t> image(iop_module_size);stream.seekg(0);stream.read(reinterpret_cast<char*>(image.data()),image.size());
        if(!stream || !std::equal(image.begin(),image.end(),iop_expected_image.begin()))throw std::runtime_error("IOP image differs from generated source");
        rt.map(iop_module_base,image);rt.map(0x1FF000,std::vector<std::uint8_t>(0x1000));rt.map(0x1FE000,std::vector<std::uint8_t>(0x1000));
        constexpr char path[]="cdrom0:\\IRX\\SIO2MAN.IRX;1";
        for(unsigned n=0;n<sizeof(path);++n)rt.write(0x1FE100+n,path[n],1);
        rt.write(0x1FE000,0x1FE100,4);rt.write(0x1FE004,0,4);
        c.pc=iop_module_entry;c.set(28,iop_module_gp);c.set(29,0x1FFF00);c.set(31,IopRuntime::sentinel);c.set(4,1);c.set(5,0x1FE000);
        iop_export_bounds(rt);install_iop_library_provider(rt);
        bt2_iop_module(rt,c);throw std::runtime_error("Unexpected module entry completion without validated effects");
    } catch(const std::exception& e) {
        if(!rt.steps){std::cerr<<e.what()<<"\n";return 1;}
        std::ofstream report(argv[2]);if(!report){std::cerr<<"Cannot write IOP report\n";return 1;}report<<"{\"pc\":"<<rt.executing_pc<<",\"steps\":"<<rt.steps<<",\"blocked_address\":"<<rt.blocked_address<<",\"access_kind\":\""<<rt.access_kind<<"\",\"access_width\":"<<rt.access_width<<",\"access_value\":"<<rt.access_value<<",\"mapped_ranges_entered\":"<<rt.entries.size()<<",\"registered_libraries\":"<<rt.libraries.size()<<",\"imports\":[";
        for(unsigned n=0;n<rt.imports.size();++n){if(n)report<<',';report<<'\"'<<rt.imports[n]<<'\"';}report<<"],\"arguments\":["<<c.get(4)<<','<<c.get(5)<<','<<c.get(6)<<','<<c.get(7)<<"]}\n";
        std::cout<<"IOP diagnostic stopped: "<<e.what()<<"; PC=0x"<<std::hex<<rt.executing_pc<<std::dec<<", native steps="<<rt.steps<<"\n";return 0;
    }
}
