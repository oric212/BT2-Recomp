#include "ee/guest_image.h"
#include "services/devices/disc_metadata.h"
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace {
constexpr std::uint32_t ram_size=32*1024*1024;
// SLUS_214.41 of Dragon Ball Z: Budokai Tenkaichi 2 (USA): its size and 64-bit FNV-1a hash.
constexpr std::uint32_t executable_size=2675936;
constexpr std::uint64_t executable_hash=0xC352CFE3D195477Eull;
constexpr const char* wrong_disc="This disc image is not Dragon Ball Z: Budokai Tenkaichi 2 (USA, SLUS-21441)";

std::uint32_t le32(std::span<const std::uint8_t> b,std::size_t at) {
    if(at+4>b.size())throw std::runtime_error("The game's executable is cut short");
    return std::uint32_t(b[at])|std::uint32_t(b[at+1])<<8|std::uint32_t(b[at+2])<<16|std::uint32_t(b[at+3])<<24;
}
std::uint16_t le16(std::span<const std::uint8_t> b,std::size_t at) {
    if(at+2>b.size())throw std::runtime_error("The game's executable is cut short");
    return std::uint16_t(b[at]|b[at+1]<<8);
}
}

std::vector<std::uint8_t> gameExecutableFromDisc(const std::string& disc_path) {
    DiscMetadata disc(disc_path);
    const auto record=disc.lookup("\\SLUS_214.41;1");
    if(!record)throw std::runtime_error(std::string(wrong_disc)+": it has no SLUS_214.41");
    std::uint32_t lba=0,size=0;std::memcpy(&lba,record->data(),4);std::memcpy(&size,record->data()+4,4);
    if(size!=executable_size)throw std::runtime_error(std::string(wrong_disc)+": its SLUS_214.41 has another size");
    std::ifstream file(disc_path,std::ios::binary);
    std::vector<std::uint8_t> executable(size);
    file.seekg(std::streamoff(std::uint64_t(lba)*2048));
    if(!file.read(reinterpret_cast<char*>(executable.data()),std::streamsize(size)))throw std::runtime_error("The disc image could not be read");
    std::uint64_t hash=0xCBF29CE484222325ull;
    for(const auto byte:executable)hash=(hash^byte)*0x100000001B3ull;
    if(hash!=executable_hash)throw std::runtime_error(std::string(wrong_disc)+" (another revision, or a damaged image)");
    return executable;
}

std::vector<std::uint8_t> guestImage(std::span<const std::uint8_t> elf) {
    if(elf.size()<0x34 || std::memcmp(elf.data(),"\x7F" "ELF",4) || elf[4]!=1 || elf[5]!=1)throw std::runtime_error("The game's executable is not a 32-bit little-endian ELF");
    const std::uint32_t table=le32(elf,0x1C);const unsigned entry_size=le16(elf,0x2A),count=le16(elf,0x2C);
    if(entry_size<32)throw std::runtime_error("The game's executable has an unexpected program header");
    std::vector<std::uint8_t> memory(ram_size);
    std::vector<std::pair<std::uint32_t,std::uint32_t>> placed;
    for(unsigned n=0;n<count;++n) {
        const std::size_t at=table+std::size_t(n)*entry_size;
        if(le32(elf,at)!=1)continue; // PT_LOAD only.
        const std::uint32_t offset=le32(elf,at+4),address=le32(elf,at+8),file_size=le32(elf,at+16),memory_size=le32(elf,at+20);
        if(!memory_size)continue;
        if(std::uint64_t(address)+memory_size>ram_size || file_size>memory_size)throw std::runtime_error("The game's executable loads outside main RAM");
        for(const auto& [start,end]:placed)if(address<end && start<address+memory_size)throw std::runtime_error("The game's executable has overlapping segments");
        placed.emplace_back(address,address+memory_size);
        if(std::uint64_t(offset)+file_size>elf.size())throw std::runtime_error("The game's executable is cut short");
        std::memcpy(memory.data()+address,elf.data()+offset,file_size);
    }
    return memory;
}
