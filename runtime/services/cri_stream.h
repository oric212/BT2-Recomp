// CRI ADX stream channel buffers (0x880 bytes): word 0 is the command count, command records follow
// at 0x10-byte steps ({code, IOP object, arguments...}), and the last word is the EE's sequence counter.
// The IOP consumes the commands and returns the buffer with the count cleared and the records unchanged
// (PCSX2 capture rpc-cri-stream-reply, 2026-10-11: commands 8 and 5 on one object).
#pragma once
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>

constexpr std::size_t cri_stream_size=0x880;

// Validates the commands in a returned buffer and marks them consumed. Only the reached command codes
// are accepted; any other stops with a named error.
inline unsigned consumeCriStreamCommands(std::span<std::uint8_t> buffer) {
    if(buffer.size()!=cri_stream_size)throw std::runtime_error("CRI stream buffer size");
    std::uint32_t count;std::memcpy(&count,buffer.data(),4);
    if(count>(cri_stream_size-0x14)/0x10)throw std::runtime_error("CRI stream command count out of range");
    for(std::uint32_t i=0;i<count;++i) {
        std::uint32_t code;std::memcpy(&code,buffer.data()+0x10+i*0x10,4);
        if(code!=5 && code!=8)throw std::runtime_error("Unsupported CRI stream command "+std::to_string(code));
    }
    std::memset(buffer.data(),0,4);
    return count;
}
