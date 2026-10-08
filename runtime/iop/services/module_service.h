#pragma once
#include "iop/providers/providers.h"
#include <string_view>

struct NativeIopModule {
    std::string path;
    std::uint32_t base,entry,gp;
    std::span<const std::uint8_t> image;
    std::function<void(IopRuntime&)> exports;
    std::function<void(IopRuntime&,IopContext&)> execute;
    std::function<void(const IopRuntime&)> validate;
};

class NativeIopService {
public:
    struct Resident {unsigned id;std::uint32_t result;};
    // Maps the owned IOP argument/stack pages, installs the kernel and library providers and the
    // native module dispatcher.
    explicit NativeIopService(unsigned epoch,std::vector<NativeIopModule> definitions);
    NativeIopService(const NativeIopService&)=delete;
    NativeIopService& operator=(const NativeIopService&)=delete;
    // Maps a compiled module, runs its entry and validates its resident state.
    Resident load(unsigned epoch,std::string_view path,std::span<const std::uint8_t> arguments={});
    void attachEeTransport(unsigned epoch) {
        if(epoch!=epoch_ || failed_ || runtime.sif_transport_attached)throw std::runtime_error("Invalid native SIF transport attachment");
        runtime.sif_transport_attached=true;
    }
    std::vector<std::uint8_t> callCriCreate(unsigned epoch,std::span<const std::uint8_t> input);
    std::vector<std::uint8_t> callCriControl(unsigned epoch,std::uint32_t command=0x403,std::span<const std::uint8_t> input={});
    unsigned cri_create_calls=0,cri_control_calls=0;
    void completeEeSoundBind(unsigned epoch,std::span<const std::uint8_t> ee_memory);
    unsigned ee_sound_bind_returns=0;
    bool ee_sound_server_owned=false;
    void ownEeSoundServer(unsigned epoch,std::span<const std::uint8_t> ee);
    std::vector<std::uint8_t> callDbcRpc(unsigned epoch,std::uint32_t command,std::span<const std::uint8_t> input,unsigned result_size,std::span<std::uint8_t> ee_memory={});
    // Frame-phase no-wait requests on the DBC services 0x8000131E/0x8000131F: original handlers.
    std::vector<std::uint8_t> callDbcPort(unsigned epoch,std::uint32_t service,std::uint32_t client,std::uint32_t result,std::uint32_t command,std::span<const std::uint8_t> input,unsigned result_size);
    unsigned dbc_port_requests=0;
    // Frame-phase requests on DBCMAN's main service (0x80001300): original handler.
    std::vector<std::uint8_t> callDbcMain(unsigned epoch,std::uint32_t command,std::span<const std::uint8_t> input);
    unsigned dbc_main_requests=0;
    std::vector<std::uint8_t> callMcQuery(unsigned epoch,std::span<const std::uint8_t> input,std::span<std::uint8_t> ee_memory={});
    std::vector<std::uint8_t> callMcRpc(unsigned epoch,std::span<const std::uint8_t> input);
    // Later sound-service commands from main (after display setup): original registered handler.
    std::vector<std::uint8_t> callSoundCommand(unsigned epoch,std::uint32_t command,std::span<const std::uint8_t> input);
    std::vector<std::uint8_t> callSoundsCommand(unsigned epoch,std::uint32_t command,std::span<const std::uint8_t> input);
    unsigned sound_command_requests=0;
    void advanceSoundTimer(unsigned epoch); // IOP hardware timer ticks for one logical NTSC field.
    void advanceVblank(unsigned epoch,std::span<std::uint8_t> ee_memory={}); // IOP VBLANK start and end edges for one logical NTSC field.
    unsigned dbc_pad_transfers=0,cri_stream_returns=0;
    // Boot FILEIO file server (0x80000001): original handler, request in its native receive buffer.
    std::vector<std::uint8_t> callFileio(unsigned epoch,std::uint32_t command,std::span<const std::uint8_t> input,unsigned result_size,std::span<std::uint8_t> ee_memory,const std::string& disc_path);
    unsigned fileio_requests=0,fileio_dma_submissions=0;
    // IOP->EE SIF commands sent by FILEIO workers (16-byte header; extra data already copied).
    struct EeCommand {std::uint32_t cid=0;std::array<std::uint8_t,16> header{};};
    std::vector<EeCommand> pending_ee_commands;unsigned ee_commands_sent=0;
    // Owned IOMAN cdrom0: device: read-only ISO9660 files from the local disc image.
    struct OwnedFile {std::uint32_t lba=0,size=0,position=0;};
    std::map<std::uint32_t,OwnedFile> cdrom_files;std::uint32_t next_cdrom_fd=1;
    unsigned cdrom_opens=0,cdrom_reads=0,cdrom_closes=0;std::uint64_t cdrom_bytes=0;
    // FILEIO's internal allocations (request records, read buffers): model-local placement in an
    // owned IOP area; these addresses never reach the EE.
    std::map<std::uint32_t,std::uint32_t> fileio_allocations;
    std::vector<std::uint8_t> callSoundRpc(unsigned epoch,std::uint32_t command,std::span<const std::uint8_t> input,unsigned result_size,std::uint32_t service=0x80000701);
    std::uint32_t allocateBootHeap(unsigned epoch,unsigned size);
    Resident loadBootBuffer(unsigned epoch,unsigned pointer);
    std::uint32_t freeBootHeap(unsigned epoch,unsigned pointer);
    std::uint32_t callDiscSearch(unsigned epoch,std::span<const std::uint8_t> input,std::span<std::uint8_t> ee_memory,const std::string& disc_path);
    std::uint32_t callDiscReady(unsigned epoch);
    std::uint32_t callDiscScmd(unsigned epoch,unsigned command);
    unsigned disc_scmd_calls=0;
    std::uint32_t callDiscRead(unsigned epoch,std::span<const std::uint8_t> input,std::span<std::uint8_t> ee,const std::string& path);
    IopRuntime::Alarm disc_read_alarm;
    unsigned disc_read_alarm_sets=0,disc_read_alarm_cancels=0;
    unsigned disc_read_calls=0,disc_read_callbacks=0,disc_read_dma_submissions=0,disc_read_dma_polls=0;
    unsigned disc_ready_calls=0;
    unsigned disc_search_calls=0,disc_dma_submissions=0,disc_dma_polls=0;
    unsigned mc_rpc_requests=0,mc_query_requests=0;
    unsigned sound_rpc_requests=0,sounds_rpc_requests=0,dbc_rpc_requests=0;
    unsigned dbc_dma_submissions=0,dbc_dma_polls=0;
    unsigned mc_dma_submissions=0,mc_dma_polls=0;
    std::array<std::uint8_t,192> mc_dma_payload{};
    std::array<std::uint8_t,128> dbc_dma_payload{};
    std::array<std::uint8_t,128> dbc_dma_ee_snapshot{}; // EE destination right after the initial DBCMAN DMA.
    unsigned buffer_loads=0;
    unsigned boot_heap_frees=0;
    unsigned boot_heap_allocations=0;
    bool supports(std::string_view path) const {for(const auto& m:definitions_)if(m.path==path)return true;return false;}
    unsigned epoch() const {return epoch_;}
    IopRuntime runtime;
    IopContext entry;
    std::map<std::string,Resident> residents;
private:
    std::vector<std::uint8_t> executeRegisteredRpc(std::uint32_t service,std::uint32_t command,std::span<const std::uint8_t> input,unsigned result_size,std::uint32_t client,std::uint32_t result);
    unsigned epoch_,next_id_=1;
    bool failed_=false;
    std::vector<NativeIopModule> definitions_;
};
std::vector<NativeIopModule> bt2_compiled_iop_modules();
