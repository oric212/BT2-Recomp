#pragma once
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

// Read-only bytes owned by the caller. This performs no guest DMA, IRQ or completion.
inline std::vector<std::uint8_t> read_iso_sectors(const std::string& path,
        std::uint32_t lba,std::uint32_t count) {
    constexpr std::uint32_t sector_size=2048,maximum_sectors=512; // Streamed CDVDFSV requests read whole requests from the ISO.
    if(!count || count>maximum_sectors)throw std::runtime_error("Unsupported bounded sector count");
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file || file.tellg()<0)throw std::runtime_error("Local disc unavailable");
    const auto length=static_cast<std::uint64_t>(file.tellg());
    const auto offset=static_cast<std::uint64_t>(lba)*sector_size;
    const auto size=static_cast<std::uint64_t>(count)*sector_size;
    if(offset>length || size>length-offset)throw std::runtime_error("Sector range outside local disc");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));file.seekg(static_cast<std::streamoff>(offset));
    if(!file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(size)))throw std::runtime_error("Short sector read");
    return bytes;
}
