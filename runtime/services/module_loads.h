#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

// LOADFILE's load replies (0x80000006, functions 0 and 6): {module ID, residency result}. No IOP module runs; the
// reply is what the original's IOP gives. IDs count up from first_module_id: after the IOP reboot with the game's
// IOPRP300 image 24 modules are registered, so the game's first load is module 0x19 and each load the next
// (PCSX2 with the BIOS, docs/plans/validation/PCSX2_COMPARISON_PLAN.md, step 8). The residency result is the one
// each module's entry returned (0 resident, 1 no resident, 2 removable).
struct ModuleLoadReply {std::uint32_t id,result;};

class ModuleLoads {
public:
    static constexpr std::uint32_t first_module_id=0x19;
    static constexpr std::uint32_t embedded_module_result=1; // The EE-embedded module ends without residency.

    // The reply to a load by path, or nothing for a module the game is not known to load.
    std::optional<ModuleLoadReply> byPath(std::string_view path) {
        const auto found=std::find_if(results.begin(),results.end(),[&](const auto& m){return m.path==path;});
        if(found==results.end())return std::nullopt;
        return next(found->result);
    }
    // The reply to a load from an IOP buffer (the module embedded in the EE executable).
    ModuleLoadReply fromBuffer() {return next(embedded_module_result);}
    unsigned loads() const {return loads_;}

private:
    struct Result {std::string_view path;std::uint32_t result;};
    static constexpr std::array<Result,13> results={{
        {"cdrom0:\\IRX\\SIO2MAN.IRX;1",0},{"cdrom0:\\IRX\\MCMAN.IRX;1",2},{"cdrom0:\\IRX\\MCSERV.IRX;1",2},
        {"cdrom0:\\IRX\\DBCMAN.IRX;1",0},{"cdrom0:\\IRX\\SIO2D.IRX;1",0},{"cdrom0:\\IRX\\DS2U_D.IRX;1",0},
        {"cdrom0:\\IRX\\LIBSD.IRX;1",0},{"cdrom0:\\IRX\\SDRDRV.IRX;1",2},{"cdrom0:\\IRX\\MODHSYN.IRX;1",0},
        {"cdrom0:\\IRX\\MODSESQ2.IRX;1",0},{"cdrom0:\\IRX\\CDVDSTM.IRX;1",2},{"cdrom0:\\IRX\\SOUNDS.IRX;1",0},
        {"cdrom0:\\IRX\\CRI_ADXI.IRX;1",2}}};
    ModuleLoadReply next(std::uint32_t result) {return {first_module_id+loads_++,result};}
    unsigned loads_=0;
};
