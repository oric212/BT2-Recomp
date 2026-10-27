#include "probe_state.h"
#include "game_starter.h"
#include "ee/ee_runtime.h"
#include "ee/ps2_runtime_macros.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

// Entry-probe reports: the private JSON stop report for each checkpoint, the RAM/GS evidence
// sidecars, and the console summary.

namespace {
std::string jsonString(std::string_view value) {
    std::string result="\"";constexpr char hex[]="0123456789abcdef";
    for(unsigned char c:value) {
        if(c=='"' || c=='\\') {result+='\\';result+=static_cast<char>(c);}
        else if(c<32) {result+="\\u00";result+=hex[c>>4];result+=hex[c&15];}
        else result+=static_cast<char>(c);
    }
    return result+'"';
}
std::string jsonEEAddresses(const std::set<std::uint32_t>& addresses) {
    std::string out="[";bool first=true;
    for(auto address:addresses)if(address<0x80000000) {if(!first)out+=",";out+=std::to_string(address);first=false;}
    return out+"]";
}

// The interpreter bridge: instructions run interpreted, and each unmapped entry that ran (ee_interpreter.cpp).
std::string interpreterReport(const EeRuntime& runtime) {
    std::string out=",\"ee_interpreted_instructions\":"+std::to_string(runtime.interpreted_instructions)
        +",\"ee_interpret_all_instructions\":"+std::to_string(runtime.interpret_all_instructions)+",\"ee_interpreted_entries\":[";
    bool first=true;
    for(const auto& [entry,record]:runtime.interpreted_entries) {
        out+=std::string(first?"":",")+"{\"entry\":"+std::to_string(entry)+",\"dispatches\":"+std::to_string(record.dispatches)
            +",\"instructions\":"+std::to_string(record.instructions)+",\"first_word\":"+std::to_string(record.first_word)+"}";
        first=false;
    }
    // The system call numbers the game installed its own handlers for (SetSyscall), which the runtime runs.
    out+="],\"installed_syscalls\":[";
    first=true;
    for(std::size_t number=0;number<runtime.syscall_handlers.size();++number)
        if(runtime.syscall_handlers[number]){out+=std::string(first?"":",")+std::to_string(number);first=false;}
    return out+"]";
}

std::string jsonStartupThreads(const EeRuntime& runtime) {
    std::ostringstream out;out<<"[";bool first=true;
    for(const auto& t:runtime.threads)if(t.id>=4 && t.id<=7) {
        if(!first)out<<",";first=false;
        out<<"["<<t.id<<","<<t.entry<<","<<t.ready<<","<<t.suspended<<","<<t.current_priority
           <<","<<t.context.r[28].low<<","<<t.context.r[29].low<<","<<t.context.r[31].low<<","<<t.waiting<<"]";
    }
    out<<"]";return out.str();
}
}

void writeRuntimeBoundaryReport(EeRuntime& runtime,R5900Context& context,const ProbeStop& stop,char** argv) {
    const auto execution_budget_exhausted=stop.execution_budget_exhausted;
    const auto stop_address=stop.address;
    const auto stop_size=stop.size;
    const auto stop_selector=stop.selector;
    const auto stop_target=stop.target;
    const auto stop_source=stop.source;
    const auto stop_resume=stop.resume;
    std::ofstream report(argv[2]);
    if (!report) throw std::runtime_error("Could not write SCMD report");
    report << "{\"status\":\"blocked at checked runtime boundary\",\"entry_executed\":true,\"game_booted\":false,\"pc\":" << context.pc
        << ",\"selector\":" << stop_selector << ",\"target\":" << stop_target << ",\"source_pc\":" << stop_source
        << ",\"resume_pc\":" << stop_resume << ",\"overlay_dispatch_validations\":" << runtime.overlay_dispatch_validations
        << ",\"interior_entry_dispatches\":" << runtime.interior_entry_dispatches
        << interpreterReport(runtime)
        << ",\"sound_tick_boundaries\":" << runtime.sound_tick_boundaries << ",\"timer1_starts\":" << runtime.timer1_starts << ",\"frame_budget_reached\":" << (runtime.frame_budget_reached?1:0) << ",\"timer1_counting\":" << (runtime.timer1.counting()?1:0) << ",\"timer1_mode\":" << runtime.timer1.mode() << ",\"timer1_polls\":" << runtime.timer1_polls
        << ",\"frame_gif_starts\":" << runtime.gif_channel.starts << ",\"frame_gif_chain_starts\":" << runtime.gif_channel.chain_starts << ",\"frame_gif_bytes\":" << runtime.gif_channel.bytes << ",\"frame_syncs\":" << runtime.frame_syncs
        << ",\"vif1_unpacks\":" << runtime.vif1.unpacks << ",\"vif1_programs\":" << runtime.vif1.programs << ",\"vu1_runs\":" << runtime.vif1.vu.runs
        << ",\"vu1_instructions\":" << runtime.vif1.vu.instructions << ",\"vu1_native_runs\":" << runtime.vif1.vu.native_runs << ",\"vu1_hand_written_runs\":" << runtime.vif1.vu.hand_written_runs << ",\"vu1_verified_runs\":" << runtime.vif1.vu.verified_runs << ",\"vu1_kicks\":" << runtime.vif1.vu.kicks << ",\"vu1_kick_hash\":\"" << std::hex << runtime.vif1.vu.kick_hash << std::dec << "\""
        << ",\"vu1_early_q_reads\":" << runtime.vif1.vu.early_q_reads << ",\"vu1_early_clip_reads\":" << runtime.vif1.vu.early_clip_reads
        << ",\"vif1_starts\":" << runtime.frame_vif1.starts << ",\"vif1_tags\":" << runtime.frame_vif1.tags << ",\"vif1_codes\":" << runtime.frame_vif1.codes << ",\"vif1_directs\":" << runtime.frame_vif1.directs << ",\"vif1_direct_qwords\":" << runtime.frame_vif1.direct_qwords << ",\"vif1_blocked_code\":" << runtime.frame_vif1.blocked_code << ",\"vif1_blocked_tag\":" << runtime.frame_vif1.blocked_tag << ",\"vif1_blocked_tadr\":" << runtime.frame_vif1.blocked_tadr
        << ",\"gs_transfers\":" << runtime.gs_sink.transfers << ",\"gs_register_writes\":" << runtime.gs_sink.register_writes
        << ",\"frame_gs_resets\":" << runtime.frame_gs_resets << ",\"frame_gs_reset_step\":" << runtime.frame_gs_reset_step << ",\"frame_gs_crt_request\":[" << runtime.frame_gs_crt_request[0] << "," << runtime.frame_gs_crt_request[1] << "," << runtime.frame_gs_crt_request[2] << "],\"frame_vsync_handler_removed\":" << (runtime.frame_vsync_handler_removed?1:0) << ",\"frame_vsync_handler\":" << runtime.frame_vsync_handler
                << ",\"gs_display_flips\":" << runtime.gs_display_flips << ",\"gs_display_writes\":" << runtime.gs_display_writes
        << ",\"gs_pmode\":" << runtime.gs_privileged[0] << ",\"gs_dispfb2\":" << runtime.gs_privileged[9] << ",\"gs_display2\":" << runtime.gs_privileged[10] << ",\"gs_dispfb1\":" << runtime.gs_privileged[7] << ",\"gs_display1\":" << runtime.gs_privileged[8]
        << ",\"execution_budget_exhausted\":" << (execution_budget_exhausted?1:0)
        << ",\"frame_vsync_deliveries\":" << runtime.frame_vsync_deliveries
        << ",\"timer1_stops\":" << runtime.timer1_stops << ",\"timer1_count\":" << runtime.timer1.count()
        << ",\"calls\":" << runtime.guest_calls << ",\"distinct_ee_entered\":" << std::count_if(runtime.entered_functions.begin(),runtime.entered_functions.end(),[](auto a){return a<0x80000000;})
        << ",\"distinct_ee_completed\":" << std::count_if(runtime.completed_functions.begin(),runtime.completed_functions.end(),[](auto a){return a<0x80000000;})
        << ",\"ee_entered_addresses\":" << jsonEEAddresses(runtime.entered_functions)
        << ",\"ee_completed_addresses\":" << jsonEEAddresses(runtime.completed_functions)
        << ",\"distinct_kernel_entered\":" << std::count_if(runtime.entered_functions.begin(),runtime.entered_functions.end(),[](auto a){return a>=0x80000000;})
        << ",\"distinct_kernel_completed\":" << std::count_if(runtime.completed_functions.begin(),runtime.completed_functions.end(),[](auto a){return a>=0x80000000;})
        << ",\"returns\":" << runtime.returns
        << ",\"words\":" << runtime.word_writes << ",\"doublewords\":" << runtime.doubleword_writes
        << ",\"bytes\":" << runtime.byte_writes << ",\"quads\":" << runtime.quadword_writes
        << ",\"waits\":" << runtime.semaphore_waits << ",\"signals\":" << runtime.semaphore_signals
        << ",\"deletions\":" << runtime.semaphore_deletions << ",\"dma_disables\":" << runtime.dmac_disable_calls
        << ",\"dma_removals\":" << runtime.dmac_removals << ",\"media_mode_calls\":" << runtime.scmd_mmode_calls << ",\"first_cd_init_calls\":" << runtime.cd_init_calls
        << ",\"sif_stops\":" << runtime.sif_stop_calls << ",\"sif_register_reads\":" << runtime.sif_register_reads
        << ",\"sif_register_writes\":" << runtime.sif_register_writes << ",\"sif_system_commands\":" << runtime.sif_system_commands
        << ",\"sif_smflag\":" << runtime.sif_smflag << ",\"iop_epoch\":" << runtime.iop_epoch
        << ",\"halfwords\":" << runtime.halfword_writes << ",\"gs_reset_writes\":" << runtime.gs_csr_reset_writes
        << ",\"gs_imr_writes\":" << runtime.gs_imr_writes << ",\"gs_crt_calls\":" << runtime.gs_crt_calls
        << ",\"native_module_loads\":" << runtime.native_module_loads
        << ",\"native_module_request\":" << std::quoted(runtime.native_module_request)
        << ",\"heap_transfers\":" << runtime.heap_transfers << ",\"heap_dma_polls\":" << runtime.heap_dma_polls
        << ",\"console_messages\":" << runtime.ee_console_messages.size()
        << ",\"console_first_message\":" << jsonString(runtime.ee_console_messages.empty()?std::string{}:runtime.ee_console_messages.front());
    report << ",\"console_messages_text\":[";
    for(unsigned n=0;n<runtime.ee_console_messages.size();++n){if(n)report<<",";report<<jsonString(runtime.ee_console_messages[n]);}
    report << "]"
        << ",\"indexed_cache_operations\":" << runtime.indexed_cache_operations
        << ",\"indexed_cache_tag_reads\":" << runtime.indexed_cache_tag_reads
        << ",\"ee_status\":" << context.cop0_status
        << ",\"ee_thread_count\":" << runtime.threads.size()
        << ",\"sound_thread_starts\":" << runtime.sound_thread_starts
        << ",\"dbc_rpc_binds\":" << runtime.dbc_rpc_binds
        << ",\"mc_rpc_binds\":" << runtime.mc_rpc_binds
        << ",\"mc_rpc_calls\":" << runtime.mc_rpc_calls
        << ",\"mc_frame_queries\":" << runtime.mc_frame_queries
        << ",\"dbc_rpc_calls\":" << runtime.dbc_rpc_calls
        << ",\"dbc_lock_waits\":" << runtime.dbc_lock_waits
        << ",\"dbc_lock_signals\":" << runtime.dbc_lock_signals
        << ",\"disc_bind_semaphores\":" << runtime.disc_bind_semaphores << ",\"disc_rpc_binds\":" << runtime.disc_rpc_binds << ",\"disc_rpc_calls\":" << runtime.disc_rpc_calls << ",\"ncmd_ready_calls\":" << runtime.ncmd_ready_calls << ",\"ncmd_read_calls\":" << runtime.ncmd_read_calls << ",\"cri_rpc_binds\":" << runtime.cri_rpc_binds << ",\"cri_rpc_calls\":" << runtime.cri_rpc_calls 
        << ",\"probe_initial_sp\":" << runtime.initial_sp << ",\"probe_semaphore_count\":" << runtime.semaphores.size() << ",\"ncmd_rpc_binds\":" << runtime.ncmd_rpc_binds << ",\"ncmd_bind_semaphores\":" << runtime.ncmd_bind_semaphores << ",\"disc_result\":" << runtime.read32(runtime.memory().data(),0x3A8740)
        << ",\"stop_returns\":[" << [&]{std::string s;for(auto a:runtime.stop_returns){if(!s.empty())s+=',';s+=std::to_string(a);}return s;}() << "]"
        << ",\"stop_ra\":" << GPR_U32(&context,31) << ",\"stop_s1\":" << GPR_U32(&context,17) << ",\"stop_s3\":" << GPR_U32(&context,19)
        << ",\"sounds_rpc_binds\":" << runtime.sounds_rpc_binds
        << ",\"sounds_rpc_calls\":" << runtime.sounds_rpc_calls
        << ",\"sound_rpc_binds\":" << runtime.sound_rpc_binds
        << ",\"sound_rpc_calls\":" << runtime.sound_rpc_calls
        << ",\"sound_client_words\":[";
    for(unsigned i=0;i<10;++i)report<<(i?",":"")<<runtime.read32(runtime.memory().data(),0x3A5C80+i*4);
    report<<"],\"sound_payload_words\":[";
    for(unsigned i=0;i<16;++i)report<<(i?",":"")<<runtime.read32(runtime.memory().data(),0x3A5C40+i*4);
    report<<"],\"sound_thread_words\":[";
    report<<"],\"sound_iop_context\":[";
    report<<"],\"cri_thread_words\":[";
    report<<"],\"spu2_auto_dma_words\":[";
    report<<"],\"ee_sound_context\":[";
    if(runtime.sound_thread_starts) {
        const auto& t=runtime.threads.at(1);const auto& c=t.context;
        report<<t.id<<","<<t.ready<<","<<c.pc<<","<<c.r[28].low<<","<<c.r[29].low<<","<<c.r[31].low<<","<<c.r[4].low<<","<<c.cop0_status;
    }
    report<<"],\"iop_rpc_servers\":[";
    report<<"],\"dbc_client_words\":[";
    for(const auto client:std::array<std::uint32_t,4>{0x3A03C0,0x3A0410,0x3A0438,0x3A0460}) {
        if(client!=0x3A03C0)report<<",";report<<"[";
        for(unsigned n=0;n<10;++n)report<<(n?",":"")<<runtime.read32(runtime.memory().data(),client+n*4);report<<"]";
    }
    report<<"],\"dbc_dma_ee_words\":[";
    for(unsigned n=0;n<32;++n)report<<(n?",":"")<<runtime.read32(runtime.memory().data(),0x3A27C0+n*4);
    report<<"],\"dbc_dma_ee_at_transfer_words\":[";
    report<<"],\"dbc_dma_payload_words\":[";
    report<<"],\"mc_dma_ee_words\":[";
    for(unsigned n=0;n<48;++n)report<<(n?",":"")<<runtime.read32(runtime.memory().data(),0x3A7480+n*4);
    report<<"],\"pending_ee_bind\":[";
    report<<"],\"mc_dma_payload_words\":[";
    report<<"],\"sio2_dma_channels\":[";
    report<<"],\"sio2_card_transfer_counts\":[";
    report<<"],\"sio2_card_controls\":[";
    report<<"],\"native_iop_import_arguments\":[";
    report<<"],\"mc_client_words\":[";
    for(unsigned n=0;n<10;++n)report<<(n?",":"")<<runtime.read32(runtime.memory().data(),0x3A6F80+n*4);
    report<<"],\"mc_input_words\":[";
    for(unsigned n=0;n<12;++n)report<<(n?",":"")<<runtime.read32(runtime.memory().data(),0x3A7000+n*4);
    report<<"],\"mc_result_words\":[";
    for(unsigned n=0;n<3;++n)report<<(n?",":"")<<runtime.read32(runtime.memory().data(),0x3A8540+n*4);
    report<<"],\"dbc_result_words\":[";
    for(unsigned n=0;n<36;++n)report<<(n?",":"")<<runtime.read32(runtime.memory().data(),0x3A04C0+n*4);
    report<<"],\"sounds_client_words\":[";
    if(runtime.sounds_rpc_binds)for(unsigned n=0;n<10;++n)report<<(n?",":"")<<runtime.read32(runtime.memory().data(),0x6351C0+n*4);
    report<<"],\"sounds_result_words\":[";
    if(runtime.sounds_rpc_calls)for(unsigned n=0;n<16;++n)report<<(n?",":"")<<runtime.read32(runtime.memory().data(),0x635180+n*4);
    report<<"],\"heap_calls\":" << runtime.heap_calls
        << ",\"heap_frees\":" << runtime.heap_frees
        << ",\"dmac_startup_writes\":" << runtime.dmac_startup_writes
        << ",\"dmac_control\":" << runtime.dmac_control << ",\"dmac_priority\":" << runtime.dmac_priority
        << ",\"ee_pool_pointers\":[" << runtime.read32(runtime.memory().data(),0x38A628) << "," << runtime.read32(runtime.memory().data(),0x38A62C) << "," << runtime.read32(runtime.memory().data(),0x38A630) << "," << runtime.read32(runtime.memory().data(),0x38A634) << "]"
        << ",\"startup_worker_dispatches\":" << runtime.startup_worker_dispatches
        << ",\"startup_worker_status_queries\":" << runtime.startup_worker_status_queries
        << ",\"startup_worker_waits\":" << runtime.startup_worker_waits
        << ",\"main_priority_admissions\":" << runtime.main_priority_admissions << ",\"main_priority_previous\":" << runtime.main_priority_previous
        << ",\"startup_scheduler_active\":" << runtime.startup_scheduler_active << ",\"current_thread_id\":" << runtime.current_thread_id
        << ",\"cri_stream_commands\":[" << [&]{std::string s;for(const auto& [key,count]:runtime.native_cri_audio.commands){if(!s.empty())s+=',';s+="["+std::to_string(key[0])+","+std::to_string(key[1])+","+std::to_string(key[2])+","+std::to_string(count)+"]";}return s;}() << "]"
        << "," << gameStarterReport() << ",\"sounds_bank_replacements\":" << runtime.sounds_bank_replacements << ",\"sound_effects_played\":" << runtime.sound_sampler.played << ",\"sound_effects_looping\":" << runtime.sound_sampler.looping << ",\"sound_effects_kept\":" << runtime.sound_sampler.kept << ",\"sound_effects_kept_refused\":" << runtime.sound_sampler.kept_refused
        << ",\"sound_effects_stopped\":" << runtime.sound_sampler.stopped << ",\"sound_effects_stops_unmatched\":" << runtime.sound_sampler.stops_unmatched << ",\"sound_effects_silenced\":" << runtime.sound_sampler.silenced
        << ",\"sound_voices_peak\":" << runtime.sound_sampler.peak_voices << ",\"sound_voices_stolen\":" << runtime.sound_sampler.stolen
        << ",\"audio_fields\":" << runtime.audio_fields << ",\"audio_stream_bytes\":" << runtime.audio_stream_bytes << ",\"audio_single_stream_bytes\":" << runtime.audio_single_stream_bytes << ",\"audio_voice_samples\":" << runtime.audio_mixer.voice_samples << ",\"audio_underruns\":" << runtime.audio_mixer.underruns
        << ",\"gs_downloads\":" << runtime.gs_readback.downloads << ",\"gs_download_qwords\":" << runtime.gs_readback.dma_qwords+runtime.gs_readback.fifo_qwords << ",\"vif1_sends\":" << runtime.frame_vif1.sends
        << ",\"busy_main_vblanks\":" << runtime.busy_main_vblanks << ",\"ipu_commands\":" << runtime.ipu.commands << ",\"ipu_vlc_decodes\":" << runtime.ipu.vlc_decodes << ",\"ipu_macroblocks\":" << runtime.ipu.macroblocks << ",\"ipu_converted_macroblocks\":" << runtime.ipu.converted_macroblocks << ",\"ipu_output_qwords\":" << runtime.ipu.output_qwords << ",\"ipu_input_qwords\":" << runtime.ipu.input.dma_qwords << ",\"scratchpad_transfers\":" << runtime.scratchpad_transfers
        << ",\"scheduled_priority_changes\":" << runtime.scheduled_priority_changes << ",\"scheduled_resumes\":" << runtime.scheduled_resumes << ",\"scheduled_wakes\":" << runtime.scheduled_wakes << ",\"main_yields\":" << runtime.main_yields << ",\"native_disc_requests\":" << runtime.native_disc_requests << ",\"native_fileio_requests\":" << runtime.native_fileio_requests << ",\"native_card_requests\":" << runtime.native_card_requests << ",\"native_pad_requests\":" << runtime.native_pad_requests << ",\"native_pad_transfers\":" << runtime.native_pad_transfers << ",\"native_cri_requests\":" << runtime.native_cri_requests << ",\"native_cri_returns\":" << runtime.native_cri_returns << ",\"native_cri_commands\":" << runtime.native_cri_commands << ",\"main_sleeps\":" << runtime.main_sleeps << ",\"main_wakes\":" << runtime.main_wakes << ",\"main_sleep_fields\":" << runtime.main_sleep_fields << ",\"frame_csr_reads\":" << runtime.frame_csr_reads
        << ",\"scheduled_suspends\":" << runtime.scheduled_suspends << ",\"cri_stream_transfers\":" << runtime.cri_stream_transfers
        << ",\"file_load_idles\":" << runtime.file_load_idles << ",\"fileio_binds\":" << runtime.fileio_binds << ",\"fileio_calls\":" << runtime.fileio_calls << ",\"fileio_completions\":" << runtime.fileio_completions << ",\"iop_uploads\":" << runtime.iop_uploads << ",\"iop_upload_polls\":" << runtime.iop_upload_polls << ",\"timer2_fields\":" << runtime.timer2.fields << ",\"timer2_wraps\":" << runtime.timer2.wraps << ",\"timer2_deliveries\":" << runtime.timer2_deliveries << ",\"display_reset_phase\":" << runtime.display_reset_phase << ",\"polled_vblanks\":" << runtime.polled_vblanks << ",\"vblank_handler_registrations\":" << runtime.vblank_handler_registrations << ",\"vblank_enable_calls\":" << runtime.vblank_enable_calls
        << ",\"vblank_disable_calls\":" << runtime.vblank_disable_calls << ",\"load_poll_idles\":" << runtime.load_poll_idles << ",\"asset_poll_idles\":" << runtime.asset_poll_idles << ",\"timer1_vblanks\":" << runtime.timer1_vblanks << ",\"dbc_port_calls\":" << runtime.dbc_port_calls << ",\"dbc_main_calls\":" << runtime.dbc_main_calls << ",\"scmd_calls\":" << runtime.scmd_calls
        << ",\"vblank_deliveries\":" << runtime.vblank_deliveries << ",\"worker_wakes\":" << runtime.worker_wakes
        << ",\"worker_resumes\":" << runtime.worker_resumes << ",\"worker_sleep_returns\":" << runtime.worker_sleep_returns
        << ",\"startup_dispatch_parent_pc\":" << runtime.startup_dispatch_parent.pc
        << ",\"queued_sound_submissions\":" << runtime.queued_sound_submissions
        << ",\"queued_sound_completions\":" << runtime.queued_sound_completions
        << ",\"sound_thread_waiting\":" << runtime.sound_thread_waiting
        << ",\"sound_dispatch_bind_id\":" << runtime.sound_dispatch_bind_id
        << ",\"sound_dispatch_wait_ra\":" << runtime.sound_dispatch_wait_ra
        << ",\"sound_dispatch_wait_sp\":" << runtime.sound_dispatch_wait_sp
        << ",\"ee_sound_probe_attempted\":" << runtime.ee_sound_probe_attempted
        << ",\"ee_sound_probe_waits\":" << runtime.ee_sound_probe_waits
        << ",\"ee_sound_probe_parent_pc\":" << runtime.ee_sound_probe_parent.pc
        << ",\"ee_sound_probe_frontier\":[" << runtime.ee_sound_probe_frontier.pc << "," << runtime.ee_sound_probe_frontier.r[28].low
        << "," << runtime.ee_sound_probe_frontier.r[29].low << "," << runtime.ee_sound_probe_frontier.r[31].low
        << "," << runtime.ee_sound_probe_frontier.r[16].low << "," << runtime.ee_sound_probe_frontier.cop0_status << "]"
        << ",\"main_thread_priority\":" << runtime.main_thread_priority << ",\"main_thread_queries\":" << runtime.main_thread_queries << ",\"startup_worker_state\":" << jsonStartupThreads(runtime)
        << ",\"startup_worker_queries\":" << runtime.startup_worker_queries << ",\"startup_worker_starts\":" << runtime.startup_worker_starts
        << ",\"startup_ready_queries\":" << runtime.startup_ready_queries
        << ",\"startup_worker_suspends\":" << runtime.startup_worker_suspends << ",\"startup_worker_priority_changes\":" << runtime.startup_worker_priority_changes
        << ",\"heap_end_queries\":" << runtime.heap_end_queries
        << ",\"ee_heap_lock_waits\":" << runtime.ee_heap_lock_waits
        << ",\"ee_heap_lock_signals\":" << runtime.ee_heap_lock_signals
        << ",\"first_flip_gif_starts\":" << runtime.first_flip.gif_starts
        << ",\"tex0\":[" << runtime.first_flip.gs_registers[8] << "," << runtime.first_flip.gs_registers[9] << "]"
        << ",\"pabe\":" << (runtime.first_flip.gs_registers[0x49]&1)
        << ",\"graph_startup_phase\":" << runtime.graph_startup_phase
        << ",\"vif_resets\":" << runtime.vif_setup.vif_resets << ",\"vif_commands\":" << runtime.vif_setup.codes
        << ",\"vif_words\":" << runtime.vif_setup.fifo_writes*4 << ",\"vif_fifo_writes\":" << runtime.vif_setup.fifo_writes
        << ",\"vu_resets\":" << runtime.vif_setup.vu_resets << ",\"gif_resets\":" << runtime.gif_resets
        << ",\"vif_registers\":[" << runtime.vif_setup.error << ',' << runtime.vif_setup.registers.cycle << ',' << runtime.vif_setup.registers.mode
        << ',' << runtime.vif_setup.registers.mask << ',' << runtime.vif_setup.registers.code << ',' << runtime.vif_setup.registers.itops
        << ',' << runtime.vif_setup.registers.base << ',' << runtime.vif_setup.registers.ofst << ',' << runtime.vif_setup.registers.tops << ']'
        << ",\"heap_binds\":" << runtime.heap_binds
        << ",\"loadfile_binds\":" << runtime.post_loadfile.binds << ",\"loadfile_versions\":" << runtime.post_loadfile.versions
        << ",\"post_cd_binds\":" << runtime.post_cd.binds << ",\"post_cd_calls\":" << runtime.post_cd.calls << ",\"post_scmd_binds\":" << runtime.post_cd.scmd_binds << ",\"post_scmd_modes\":" << runtime.post_cd.modes
        << ",\"post_sif_rpc_initialized\":" << (runtime.post_sif.rpc_initialized ? 1 : 0)
        << ",\"post_sif_submissions\":" << runtime.post_sif.submissions
        << ",\"post_sif_responses\":" << runtime.post_sif.sreg_responses
        << ",\"gif_starts\":" << runtime.gif_channel.starts
        << ",\"gs_csr_reads\":" << runtime.gs_csr_reads
        << ",\"memory_address\":" << stop_address << ",\"memory_size\":" << stop_size
        << ",\"iop_reset_pending\":" << (runtime.iop_reset_pending ? "true" : "false")
        << ",\"prior_iop_services_valid\":" << (runtime.prior_iop_services_valid ? "true" : "false")
        << ",\"sp\":" << context.r[29].low << ",\"scmd_binds\":" << runtime.scmd_bind_calls << ",\"arguments\":[" << GPR_U32(&context,4) << "," << GPR_U32(&context,5) << "]"
        << ",\"descriptor_words\":[";
    for(unsigned i=0;i<((stop_selector==0x40 || stop_selector==0x77)?8u:0u);++i) report << (i?",":"") << runtime.read32(runtime.memory().data(),GPR_U32(&context,4)+i*4);
    report << "],\"packet_words\":[";
    for(unsigned i=0;i<16;++i) report << (i?",":"") << runtime.read32(runtime.memory().data(),0x3A97C0+i*4);
    report << "],\"heap_client_words\":[";
    for(unsigned i=0;i<10;++i)report << (i?",":"") << runtime.read32(runtime.memory().data(),0x3AC840+i*4);
    report << "],\"reset_packet_words\":[";
    for(unsigned i=0;i<26;++i) report << (i?",":"") << runtime.read32(runtime.memory().data(),0x3ACC40+i*4);
    report << "]}\n";
}

void writeEvidenceDumps(EeRuntime& runtime,char** argv) {
    if(runtime.heap_calls) {
        if(runtime.ncmd_read_calls) { // After the first disc read.
            std::ofstream ee_dump(std::string(argv[2])+".ee.bin",std::ios::binary);
            ee_dump.write(reinterpret_cast<const char*>(runtime.memory().data()),runtime.memory().size());
            if(!ee_dump)throw std::runtime_error("EE evidence dump failed");
        }
        const auto& iop=runtime.nativeIopMemory(); // Native IOP memory store (uploaded data).
        std::ofstream dump(std::string(argv[2])+".iop.bin",std::ios::binary);
        if(!dump.write(reinterpret_cast<const char*>(iop.data()),iop.size()))throw std::runtime_error("IOP checkpoint write failed");
    }
}

