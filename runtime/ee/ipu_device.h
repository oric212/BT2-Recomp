#pragma once
#include <array>
#include <cstdint>
#include <deque>
#include <optional>

// IPU (MPEG decoder) register model, scoped to setup: the public register layout (IPU_CMD 0x10002000,
// IPU_CTRL 0x10002010, IPU_BP 0x10002020, IPU_TOP 0x10002030, IN_FIFO 0x10007010) and the commands
// sceIpuInit issues: BCLR, SETIQ, SETVQ and SETTH, which complete immediately (never BUSY). Decode
// commands (IDEC, BDEC, VDEC, FDEC, CSC, PACK) and IPU DMA starts are not modelled: they are refused.
// IPU DMA channels 3 (IPU_FROM, 0x1000B000) and 4 (IPU_TO, 0x1000B400) accept register writes only
// while stopped.
class IpuDevice {
public:
    std::array<std::uint8_t,64> intra_iq{},nonintra_iq{}; // SETIQ tables (zigzag order as written).
    std::array<std::uint16_t,16> vq_clut{};               // SETVQ colour table.
    std::uint32_t thresholds=0;                            // SETTH TH0 (bits 0-8), TH1 (bits 16-24).
    bool iq_loaded[2]{},vq_loaded=false;
    std::uint32_t decode_parameters=0; // IPU_CTRL IDP (16-17), AS (20), IVF (21), QST (22), MP1 (23), PCT (24-26).
    static constexpr std::uint32_t decode_parameter_mask=0x07F30000u;
    unsigned resets=0,commands=0;
    struct Channel {std::uint32_t chcr=0,madr=0,qwc=0,tadr=0;};
    Channel from_ipu,to_ipu; // DMA channel 3 (IPU_FROM) and 4 (IPU_TO).

    // Register access; nullopt/false means the address is not an IPU or IPU DMA register.
    std::optional<std::uint32_t> read32(std::uint32_t address) const;
    bool write32(std::uint32_t address,std::uint32_t value);
    bool writeFifo(std::uint32_t address,const std::array<std::uint32_t,4>& qword);

private:
    std::deque<std::array<std::uint32_t,4>> in_fifo_; // At most 8 quadwords.
    std::uint32_t command_result_=0,bit_pointer_=0;
    void reset();
    void command(std::uint32_t value);
    void take(std::uint8_t* out,unsigned qwords);
};
