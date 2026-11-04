// The game's starting memory made from a disc image (runtime/ee/guest_image.h): segments placed as the loader places
// them, bad executables refused by name; with the developer's local files, equal byte for byte to the image the
// generation tools prepared (tools/ps2recomp/prepare_runtime.py).
#include "ee/guest_image.h"
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {
void check(bool ok,std::source_location at=std::source_location::current()) {
    if(!ok) {std::cerr<<"guest image: check failed at line "<<at.line()<<'\n';std::exit(1);}
}
void put32(std::vector<std::uint8_t>& b,std::size_t at,std::uint32_t v) {for(unsigned n=0;n<4;++n)b[at+n]=std::uint8_t(v>>(8*n));}
// A small ELF: a PT_LOAD of `file_size` bytes (0xA0, 0xA1, ...) at `address`, `memory_size` long, and a PT_NOTE.
std::vector<std::uint8_t> elf(std::uint32_t address,std::uint32_t file_size,std::uint32_t memory_size) {
    std::vector<std::uint8_t> b(0x100+file_size);
    std::memcpy(b.data(),"\x7F" "ELF",4);b[4]=1;b[5]=1;
    put32(b,0x1C,0x34);b[0x2A]=32;b[0x2C]=2;
    put32(b,0x34,1);put32(b,0x34+4,0x100);put32(b,0x34+8,address);put32(b,0x34+16,file_size);put32(b,0x34+20,memory_size);
    put32(b,0x54,4);put32(b,0x54+4,0);put32(b,0x54+8,0);put32(b,0x54+16,0x100);put32(b,0x54+20,0x100);
    for(std::uint32_t n=0;n<file_size;++n)b[0x100+n]=std::uint8_t(0xA0+n);
    return b;
}
bool refused(const std::vector<std::uint8_t>& b) {try {guestImage(b);} catch(const std::runtime_error&) {return true;}return false;}
std::vector<std::uint8_t> read(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in),{});
}
}

int main() {
    const auto memory=guestImage(elf(0x100000,8,0x20));
    check(memory.size()==32u*1024*1024);
    check(memory[0x100000]==0xA0 && memory[0x100007]==0xA7 && memory[0x100008]==0 && memory[0xFFFFF]==0);
    check(refused(elf(32u*1024*1024-4,8,8)));      // Outside main RAM.
    check(refused(elf(0x100000,16,8)));             // More file than memory.
    auto bad=elf(0x100000,8,8);bad[4]=2;check(refused(bad)); // 64-bit.
    bad=elf(0x100000,8,8);put32(bad,0x34+4,0x1000);check(refused(bad)); // Data past the file's end.
    {
        bool named=false;
        try {gameExecutableFromDisc("no such disc.iso");} catch(const std::runtime_error&) {named=true;}
        check(named);
    }
    // The developer's files: the executable from the disc, and the image the tools prepared.
    const std::filesystem::path root=BT2_SOURCE_DIR,prepared=root/"work/bt2-generation/guest-memory.bin";
    std::filesystem::path disc;
    for(const auto& entry:std::filesystem::directory_iterator(root/"rom",std::filesystem::directory_options::skip_permission_denied))
        if(entry.path().extension()==".iso")disc=entry.path();
    if(disc.empty() || !std::filesystem::exists(prepared)) {std::cout<<"Guest image checks passed (no local disc: the comparison skipped)\n";return 0;}
    const auto from_disc=guestImage(gameExecutableFromDisc(disc.string()));
    check(from_disc==read(prepared));
    std::cout<<"Guest image checks passed; the image from the disc equals the prepared one\n";
}
