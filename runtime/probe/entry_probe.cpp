#include "gs/cpu/cpu_display.h"
#ifdef BT2_GPU_GS
#include "gs/gpu/gpu_gs_mirror.h"
#endif
#ifdef _WIN32
#include "host/viewer_window.h"
#endif
#include "gs/textures/gs_texture_names.h"
#include "probe_state.h"
#include "game_starter.h"
#include "host/audio_device.h"
#include "host/frame_pacer.h"
#include "host/host_input.h"
#include "host/input_mapping.h"
#include "host/player_session.h"
#include "host/sample_profiler.h"
#include "host/wav_writer.h"
#include "vu/vu1_program.h"
#include "ee/ee_runtime.h"
#include "ee/game_patches.h"
#include "ee/ps2_runtime_macros.h"
#include "ee/state_hash.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <tuple>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>

// Entry probe: runs the generated BT2 entry on a prepared guest image with the supplied evidence,
// classifies the checked boundary where it stops, validates the reached checkpoint and writes the
// private report. Stages live in probe_inputs.cpp, startup_bindings.cpp, checkpoint_checks.cpp and
// probe_reports.cpp.

int main(int argc, char** argv) {
    try {
        if (argc < 3 || argc > 25) throw std::runtime_error("Usage: bt2-entry-probe MEMORY_IMAGE STOP_REPORT [KERNEL_SNAPSHOT [TIMER3_CAPTURE_WORD [TIMER2_CAPTURE_WORD [INTC_MASK_WORD [TIMER2_READBACK_WORD [OSD_CONFIG_WORD [OSD_READBACK_WORD [DMA_STATUS_WORD [DMA5_CONTROL_WORD [SIF_COMMAND_WORD [SIF_DMA_BUNDLE [SIF_RPC_STATE_WORD [SIF_BIND_BUNDLE]]]]]]]]]]]]]");
        const ProbeModes modes=ProbeModes::fromArgc(argc);
        const bool cd_mode=modes.cd,bind_mode=modes.bind,snapshot_mode=modes.snapshot,timer_mode=modes.timer,timer2_mode=modes.timer2,
            intc_mode=modes.intc,readback_mode=modes.readback,osd_mode=modes.osd,osd_readback_mode=modes.osd_readback,
            dma_status_mode=modes.dma_status,sif_rpc_mode=modes.sif_rpc,sif_dma_mode=modes.sif_dma,sif_mode=modes.sif,dma5_mode=modes.dma5;
        std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
        if (!input || input.tellg() != static_cast<std::streamoff>(EeRuntime::ram_size))
            throw std::runtime_error("Invalid prepared guest image size");
        std::vector<std::uint8_t> image(EeRuntime::ram_size);
        input.seekg(0);
        if (!input.read(reinterpret_cast<char*>(image.data()), static_cast<std::streamsize>(image.size())))
            throw std::runtime_error("Could not read guest image");
        EeRuntime runtime;
        runtime.ee_sound_dispatch_probe = std::getenv("BT2_EE_SOUND_DISPATCH_PROBE") != nullptr;
        runtime.ee_queued_sound_rpc = !runtime.ee_sound_dispatch_probe;
        // Frame budget: stop after this many display flips (default 1000, none in viewer mode;
        // BT2_FRAME_LIMIT overrides, 0 disables).
        const bool viewer_mode = std::getenv("BT2_VIEWER") != nullptr;
        runtime.frame_limit = viewer_mode ? 0 : 1000;
        if (const char* limit = std::getenv("BT2_FRAME_LIMIT")) runtime.frame_limit = static_cast<unsigned>(std::stoul(limit));
        if (const char* watch = std::getenv("BT2_EE_WATCH_FLOAT")) {
            runtime.float_watch_address = static_cast<std::uint32_t>(std::stoul(watch, nullptr, 16));
            runtime.float_watch_limit = std::strchr(watch, ':') ? std::stof(std::strchr(watch, ':') + 1) : 1e9f;
        }
        // BT2_STALL_FIELDS: fields without a display flip before the run stops (default 3600).
        if (const char* limit = std::getenv("BT2_STALL_FIELDS")) runtime.flipless_field_limit = static_cast<unsigned>(std::stoul(limit));
        // BT2_STATE_HASH=<path>: a hash of the guest memories at the end of every field; with
        // BT2_STATE_HASH_PAGES=<field> also each 4 KB page's hash at that field (compare_state_hash.py).
        std::unique_ptr<StateHashLog> state_hash;
        if (const char* path = std::getenv("BT2_STATE_HASH")) {
            const char* pages = std::getenv("BT2_STATE_HASH_PAGES");
            state_hash = std::make_unique<StateHashLog>(path, pages ? static_cast<unsigned>(std::stoul(pages)) : 0u);
            runtime.on_field_end = [&runtime, &state_hash] {
                const StateHashLog::Region regions[] = {{"ee", runtime.memory()}, {"sp", runtime.scratchpad()},
                    {"vu0", runtime.vu0_memory_}, {"vu1data", runtime.vif1.vu.data}, {"vu1code", runtime.vif1.vu.program}};
                state_hash->field(static_cast<unsigned>(runtime.vblank_deliveries + 1), static_cast<unsigned>(runtime.gs_display_flips), regions);
            };
        }
        // BT2_NO_INTERIOR_ENTRIES: dispatch knows function starts only (compare with the default).
        if (std::getenv("BT2_NO_INTERIOR_ENTRIES")) runtime.interior_entries_disabled = true;
        // BT2_AUDIO_WAV=<path>: the mixed audio of the run as a 48 kHz stereo WAV file.
        std::unique_ptr<WavWriter> wav;
        if (const char* path = std::getenv("BT2_AUDIO_WAV")) {
            wav = std::make_unique<WavWriter>(path, AudioMixer::rate, 2);
        }
        // BT2_AUDIO_DEVICE: play the mixed audio on the default audio device.
        // Pacing: with the viewer or the audio device the run is held at the game's field rate by the
        // clock (host/frame_pacer.h). Each field waits once for its deadline, when its picture is shown
        // or at its end. With the device, the period follows the device's queue (kept near 50 ms).
        struct Speaker {
            std::unique_ptr<AudioDevice> device;
            ~Speaker() {
                if (device) std::cerr << std::dec << "Audio device: " << device->fields << " fields played, ran dry before " << device->dry
                    << ", waited " << device->waits << " times for the device." << std::endl;
            }
        };
        static Speaker speaker; // Static, so the summary is also printed when the viewer's closing ends the run.
        static std::unique_ptr<FramePacer> pacer;
        if (std::getenv("BT2_AUDIO_DEVICE")) {
            speaker.device = std::make_unique<AudioDevice>(AudioMixer::rate, 2, AudioMixer::rate / 4);
            speaker.device->fillTo(AudioMixer::rate / 20); // The queue starts at its target level.
        }
        if (viewer_mode || speaker.device) pacer = std::make_unique<FramePacer>();
        // BT2_FAST_FORWARD: the whole run in fast forward, as when its key is held (to measure it).
        if (pacer && std::getenv("BT2_FAST_FORWARD")) pacer->free_run = true;
        if (wav || pacer)
            runtime.on_audio_field = [&wav](std::span<const std::int16_t> pcm) {
                if (wav) wav->write(pcm);
                if (pacer) {
                    pacer->wait(); // The field that ends here, if it showed no picture.
                    // A queue above its target means the fields come faster than the device plays them: lengthen the period a little.
                    const double excess = speaker.device ? (double(speaker.device->queuedFrames()) - AudioMixer::rate / 20) / AudioMixer::rate : 0;
                    pacer->fieldStarted(1.0 + excess * 0.1);
                }
                // Fast forward is silent; when it ends the device's queue, run empty meanwhile, is filled
                // to its target with silence at once instead of running dry for seconds.
                static bool was_fast = false;
                const bool fast = pacer && pacer->free_run;
                if (speaker.device && !fast) {
                    if (was_fast) speaker.device->fillTo(AudioMixer::rate / 20);
                    speaker.device->write(pcm);
                }
                was_fast = fast;
            };
        // BT2_SOUND_LOG=<folder>: the sound effects requests as text (requests.txt: field, kind, function,
        // command words) and each loaded bank's two blocks as files. Local evidence, not committed.
        std::ofstream sound_log;
        if (const char* folder = std::getenv("BT2_SOUND_LOG")) {
            const std::string base = folder;
            sound_log.open(base + "/requests.txt");
            if (!sound_log) throw std::runtime_error("Cannot write the sound log in " + base);
            runtime.on_sound_evidence = [&runtime, &sound_log, base](std::string_view kind, std::uint32_t number, std::span<const std::uint8_t> data) {
                static unsigned banks = 0;
                if (kind == "bank-head" || kind == "bank-body" || kind == "samples") {
                    if (kind == "samples") ++banks; // The sample transfer precedes its bank load.
                    std::ofstream(base + "/bank-" + std::to_string(banks) + (kind == "bank-head" ? "-head.bin" : kind == "samples" ? "-samples.bin" : "-body.bin"), std::ios::binary)
                        .write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));
                    sound_log << runtime.vblank_deliveries << ' ' << kind << ' ' << std::hex << number << ' ' << data.size() << std::dec << '\n';
                    return;
                }
                sound_log << runtime.vblank_deliveries << ' ' << kind << ' ' << std::hex << number;
                for (std::size_t n = 0; n + 4 <= data.size(); n += 4) { std::uint32_t word; std::memcpy(&word, data.data() + n, 4); sound_log << ' ' << word; }
                sound_log << std::dec << '\n';
            };
        }
        // BT2_VU1_CENSUS=<existing folder>: every distinct VU1 microprogram the run starts (the pairs
        // reachable from each start address), one file per program named by its key: the start address,
        // the pair count and the pairs as address, lower word, upper word. Local evidence, not committed.
        struct Vu1Census {
            struct Cost {std::uint64_t runs=0,pairs=0,nanoseconds=0;};
            std::string folder;std::map<std::uint64_t,std::size_t> programs;unsigned entries=0,indirect=0;
            std::array<std::uint64_t,Vu1::program_mask+1> key_at{};std::map<std::uint64_t,Cost> cost;
            ~Vu1Census() {
                if(folder.empty())return;
                std::size_t pairs=0;for(const auto& [key,count]:programs)pairs+=count;
                std::cerr<<std::dec<<"VU1 census: "<<entries<<" new entries, "<<programs.size()<<" distinct programs ("<<indirect<<" with register jumps), "<<pairs<<" pairs in all."<<std::endl;
                // What each program cost over the run: `key size runs pairs milliseconds`.
                std::ofstream out(folder+"/cost.txt");
                for(const auto& [key,c]:cost)out<<std::hex<<std::setw(16)<<std::setfill('0')<<key<<std::dec<<' '<<programs[key]<<' '<<c.runs<<' '<<c.pairs<<' '<<c.nanoseconds/1'000'000<<'\n';
            }
        };
        static Vu1Census census;
        if (const char* folder = std::getenv("BT2_VU1_CENSUS")) {
            census.folder = folder;
            runtime.vif1.vu.on_entry = [&runtime](std::uint32_t start) {
                const auto program = Vu1Program::reachable(runtime.vif1.vu.program, start);
                ++census.entries;census.key_at[start & Vu1::program_mask] = program.key;
                if (!census.programs.emplace(program.key, program.pairs.size()).second) return;
                if (program.indirect) ++census.indirect;
                char name[32];std::snprintf(name, sizeof name, "/%016llx.vu1", static_cast<unsigned long long>(program.key));
                std::ofstream out(census.folder + name, std::ios::binary);
                const std::uint32_t head[2] = {program.start, static_cast<std::uint32_t>(program.pairs.size())};
                out.write(reinterpret_cast<const char*>(head), 8);
                out.write(reinterpret_cast<const char*>(program.pairs.data()), std::streamsize(program.pairs.size() * sizeof(Vu1Program::Pair)));
                if (!out) throw std::runtime_error("Cannot write the VU1 census in " + census.folder);
            };
            runtime.vif1.vu.on_ran = [](std::uint32_t start, std::uint64_t pairs, std::uint64_t nanoseconds) {
                auto& c = census.cost[census.key_at[start]];++c.runs;c.pairs += pairs;c.nanoseconds += nanoseconds;
            };
        }
        // BT2_EE_CALL_TRACE=<file>:<from flip>:<hex address>[,<hex address>...]: one line per call of those
        // functions from that display flip on: the flip, the calling address, the target, a0 to a3,
        // f12, f13 and f0 (the previous call's result), and the four floats at each of a0 to a2 that points into RAM. Local evidence, not committed.
        static std::ofstream call_trace;
        if (const char* trace = std::getenv("BT2_EE_CALL_TRACE")) {
            const std::string text = trace;
            const auto list = text.rfind(':'), from = text.rfind(':', list - 1);
            if (list == std::string::npos || from == std::string::npos) throw std::runtime_error("BT2_EE_CALL_TRACE needs <file>:<from flip>:<addresses>");
            call_trace.open(text.substr(0, from));
            if (!call_trace) throw std::runtime_error("Cannot write the call trace " + text.substr(0, from));
            const unsigned first = static_cast<unsigned>(std::stoul(text.substr(from + 1, list - from - 1)));
            std::vector<std::uint32_t> targets;
            for (std::size_t at = list + 1; at < text.size();) {
                const auto end = std::min(text.find(',', at), text.size());
                targets.push_back(static_cast<std::uint32_t>(std::stoul(text.substr(at, end - at), nullptr, 16)));at = end + 1;
            }
            runtime.on_guest_call = [&runtime, targets, first](const R5900Context& context, std::uint32_t source, std::uint32_t target) {
                if (runtime.gs_display_flips < first || std::find(targets.begin(), targets.end(), target) == targets.end()) return;
                call_trace << std::dec << runtime.gs_display_flips << std::hex << ' ' << source << ' ' << target;
                for (unsigned reg = 4; reg < 8; ++reg) call_trace << ' ' << context.r[reg].low;
                call_trace << std::dec << ' ' << context.f[12] << ' ' << context.f[13] << ' ' << context.f[0];
                for (unsigned reg = 4; reg < 7; ++reg) {
                    const std::uint32_t address = context.r[reg].low;
                    if (address < 0x80000 || address > runtime.memory().size() - 16) continue;
                    float values[4];std::memcpy(values, runtime.memory().data() + address, 16);
                    call_trace << " | a" << reg - 4 << ':';
                    for (const float value : values) call_trace << ' ' << value;
                }
                call_trace << '\n';
            };
        }
        // BT2_EE_WATCH_QUADWORD=<file>:<from flip>:<hex address>: one line per 128-bit store to that EE
        // address from that display flip on: the flip, main's program counter and return address, the
        // last call (source and target) and the four floats stored. Local evidence, not committed.
        static std::ofstream store_trace;
        if (const char* watch = std::getenv("BT2_EE_WATCH_QUADWORD")) {
            const std::string text = watch;
            const auto address = text.rfind(':'), from = text.rfind(':', address - 1);
            if (address == std::string::npos || from == std::string::npos) throw std::runtime_error("BT2_EE_WATCH_QUADWORD needs <file>:<from flip>:<address>");
            store_trace.open(text.substr(0, from));
            if (!store_trace) throw std::runtime_error("Cannot write the store trace " + text.substr(0, from));
            const unsigned first = static_cast<unsigned>(std::stoul(text.substr(from + 1, address - from - 1)));
            runtime.quadword_watch_address = static_cast<std::uint32_t>(std::stoul(text.substr(address + 1), nullptr, 16));
            runtime.on_watched_quadword = [&runtime, first](const ProbeGpr& value) {
                if (runtime.gs_display_flips < first) return;
                float lanes[4];std::memcpy(lanes, &value, 16);
                const auto* context = runtime.mainContext();
                store_trace << std::dec << runtime.gs_display_flips << std::hex << " pc " << (context ? context->pc : 0) << " ra " << (context ? context->r[31].low : 0)
                            << " call " << runtime.lastCallSource() << "->" << runtime.lastCallTarget() << std::dec;
                for (const float lane : lanes) store_trace << ' ' << lane;
                store_trace << '\n';
            };
        }
        // BT2_CARD_FOLDER=<folder>: a memory card folder in slot 1 (the player's run takes it from the settings later).
        {
            // The folder may have a name outside the system's code page: read it as wide characters.
            wchar_t card[1024];
            const DWORD length = GetEnvironmentVariableW(L"BT2_CARD_FOLDER", card, DWORD(std::size(card)));
            if (length && length < std::size(card)) runtime.memory_card_folder = std::filesystem::path(card);
        }
        // BT2_VU1_INTERPRET: run every VU1 microprogram in the interpreter, also where the build has a native translation.
        if (std::getenv("BT2_VU1_INTERPRET")) runtime.vif1.vu.use_native = false;
        // BT2_VU1_NO_HAND_WRITTEN: leave the hand-written VU1 programs out (translations and the interpreter only).
        if (std::getenv("BT2_VU1_NO_HAND_WRITTEN")) runtime.vif1.vu.use_hand_written = false;
        // BT2_VU1_VERIFY: run every start that has a native program in the interpreter as well and stop at the first difference.
        if (std::getenv("BT2_VU1_VERIFY")) runtime.vif1.vu.verify_native = true;
        // BT2_PROFILE_TOTALS: the frame profile's sums over the whole run, printed at exit (headless runs).
        if (std::getenv("BT2_PROFILE_TOTALS")) {
            frame_profile.enabled = true;
            static const struct Totals {
                ~Totals() {
                    std::cerr << "Profile totals: VIF1 without VU1 " << frame_profile.vif1 - frame_profile.vu1 << " s, VU1 without its GS packets "
                              << frame_profile.vu1 - frame_profile.vu1_gs << " s, GS packet handling " << frame_profile.gs - frame_profile.renderer
                              << " s (of VU1's packets " << frame_profile.vu1_gs << " s with the renderer's share), GPU renderer intake "
                              << frame_profile.renderer << " s, GPU flush " << frame_profile.flush << " s" << std::endl;
                }
            } totals;
        }
        // BT2_EE_LOOP_LIMIT: loop iterations one thread may run within a field before the runaway guard stops it.
        if (const char* limit = std::getenv("BT2_EE_LOOP_LIMIT")) runtime.checkpoint_limit = static_cast<unsigned>(std::stoul(limit));
#ifdef _WIN32
        // Viewer mode: present the displayed framebuffer at each flip; keep the last frame visible after
        // the run stops until the window is closed. Closing the window during a run ends the process.
        const char* scale = std::getenv("BT2_VIEWER_SCALE");
        static ViewerWindow viewer(scale && std::string_view(scale) == "stretch" ? ViewerWindow::Scaling::Stretch : ViewerWindow::Scaling::Integer);
        {   // The widescreen patch draws 16:9; the window shows it so.
            const char* patches = std::getenv("BT2_GAME_PATCHES");
            for (const GamePatch* patch : parseGamePatches(patches ? patches : "")) if (patch->name == "widescreen") viewer.setWidescreen(true);
        }
        struct ViewerHold {bool active;~ViewerHold(){if(active && viewer.opened())viewer.waitUntilClosed();}} viewer_hold{viewer_mode && !std::getenv("BT2_VIEWER_CLOSE")}; // BT2_VIEWER_CLOSE: the window closes with the run (automated runs with a window).
        if (viewer_mode) runtime.on_display_flip = [&runtime] {
            const auto dispfb = runtime.gs_privileged[9] ? runtime.gs_privileged[9] : runtime.gs_privileged[7];
            const auto display = runtime.gs_privileged[9] ? runtime.gs_privileged[10] : runtime.gs_privileged[8];
            const auto frame = decodeDisplay(runtime.gif_dma.gs, dispfb, display);
            if (pacer) pacer->wait();
            viewer.present(frame, runtime.gs_display_flips);
            if (viewer.closed()) {std::cerr << "Viewer closed; ending the run." << std::endl; std::exit(0);}
        };
        // BT2_HOST_INPUT: the keyboard and a gamepad play the game while the viewer window has the focus
        // (default mapping, host/input_mapping.h); a pad script is then ignored.
        // BT2_PAD_RECORD=<path>: the buttons played, written as a pad script that replays the session
        // (stick positions are not part of a pad script).
        static std::unique_ptr<HostInput> host_input;
        static std::unique_ptr<PlayerSession> player;
        static std::ofstream pad_record;
        if (std::getenv("BT2_HOST_INPUT")) {
            if (!viewer_mode) throw std::runtime_error("BT2_HOST_INPUT needs the viewer");
            host_input = std::make_unique<HostInput>();
            runtime.on_pad_input = [] {
                if (!viewer.focused()) return PadInput{};
                return mapInput(InputMapping{}, host_input->gamepad(), &HostInput::keyDown);
            };
            // BT2_PLAYER_SETTINGS=<settings file>: a player's run (host/player_session.h). Set by the game program only.
            if (const char* settings_path = std::getenv("BT2_PLAYER_SETTINGS")) {
                player = std::make_unique<PlayerSession>(settings_path, viewer, *host_input, runtime.audio_mixer, pacer.get());
                runtime.on_pad_input = [] { return player->padInput(); };
            }
            if (const char* path = std::getenv("BT2_PAD_RECORD")) {
                pad_record.open(path);
                if (!pad_record) throw std::runtime_error(std::string("Cannot write ") + path);
                runtime.on_pad_change = [](unsigned field, std::uint16_t pressed) {
                    static bool first = true;
                    pad_record << (first ? "" : ",") << field << ':';first = false;
                    if (!pressed) pad_record << '-';
                    for (unsigned bit = 0, any = 0; bit < 16; ++bit)
                        if (pressed >> bit & 1) pad_record << (any++ ? "+" : "") << PadScript::names[bit];
                    pad_record.flush();
                };
            }
        }
#endif
        // BT2_TEXTURE_NAMES=<path>: name each texture drawn as PCSX2's texture replacement does and write
        // the names with how often each was named, and what the names' copy of GS memory did not follow,
        // when the run ends (docs/plans/graphics/HD_TEXTURES_PLAN.md, step 1).
        static struct TextureNameLog {
            GsTextureNames names;std::string path;
            ~TextureNameLog() {
                if (path.empty()) return;
                std::ofstream out(path);
                out << "# uploaded_pixels " << names.uploaded_pixels << " unsupported_uploads " << names.unsupported_uploads
                    << " local_copies " << names.local_copies << " unsupported_cluts " << names.unsupported_cluts
                    << " unsupported_textures " << names.unsupported_textures;
                for (const auto& [psm, count] : names.unsupported_formats) out << " psm_" << std::hex << psm << std::dec << ' ' << count;
                out << '\n';
                for (const auto& [name, count] : names.drawn) out << name << ' ' << count << '\n';
            }
        } texture_name_log;
        if (const char* path = std::getenv("BT2_TEXTURE_NAMES")) {
            texture_name_log.path = path;
            runtime.gif_dma.gs.texture_names = &texture_name_log.names;
        }
#ifdef BT2_GPU_GS
        // BT2_GPU_GS=mirror: render on paraLLEl-GS as well and compare VRAM at each flip.
        // BT2_GPU_GS=on: paraLLEl-GS draws alone; the CPU model only validates packets.
        static GpuGsMirror gpu_mirror;
        const char* gpu_mode = std::getenv("BT2_GPU_GS");
        if (gpu_mode && std::string_view(gpu_mode) != "mirror" && std::string_view(gpu_mode) != "on")
            throw std::runtime_error("BT2_GPU_GS must be mirror or on");
        if (gpu_mode) {
            const bool gpu_only = std::string_view(gpu_mode) == "on";
            // BT2_SUPER_SAMPLING=<1|2|4|8|16>: samples per pixel; 4 or more scan out at twice the resolution.
            gpu_mirror.attach(runtime.gif_dma.gs, std::getenv("BT2_SUPER_SAMPLING") ? static_cast<unsigned>(std::stoul(std::getenv("BT2_SUPER_SAMPLING"))) : 1u);
            runtime.gif_dma.gs.rasterize = !gpu_only;
            // BT2_HD_TEXTURES=<pack folder>[:<budget in MB, default 2048>]: draw with a PCSX2 texture pack's
            // replacements (docs/plans/graphics/HD_TEXTURES_PLAN.md).
            if (const char* hd = std::getenv("BT2_HD_TEXTURES")) {
                std::string folder = hd;std::uint64_t budget_mb = 2048;
                if (const auto colon = folder.rfind(':'); colon != std::string::npos && colon > 1) {budget_mb = std::stoull(folder.substr(colon + 1));folder.resize(colon);}
                runtime.gif_dma.gs.texture_names = &texture_name_log.names;
                gpu_mirror.enableHdTextures(folder, budget_mb << 20, texture_name_log.names);
            }
            // The window shows the renderer's picture on the GPU, without reading it back (host/window_presenter.h);
            // BT2_VIEWER_GDI=1 reads each picture back and draws it with GDI as before.
            if (viewer_mode && !std::getenv("BT2_VIEWER_GDI")) {
                viewer.setPresenter(&gpu_mirror.windowPresenter());
                if (player) player->picture_source = [] { return gpu_mirror.readback(); };
            }
            // GPU-only: VRAM lives in the renderer, which also answers local->host transfers.
            if (gpu_only) runtime.gs_download_source = [](std::span<std::uint8_t> out) { gpu_mirror.readFifo(out); };
            // With the viewer, the window shows the GPU scanout instead of the CPU framebuffer.
            // BT2_GPU_GS_SNAPSHOT=<flip>:<path> writes that flip's GPU scanout as a binary PPM.
            // Several flips: <flip>:<path> entries separated by ';'.
            static std::map<unsigned, std::string> snapshots;
            if (const char* snapshot = std::getenv("BT2_GPU_GS_SNAPSHOT")) {
                std::string list = snapshot;
                for (std::size_t at = 0; at < list.size();) {
                    const auto end = std::min(list.find(';', at), list.size());
                    const auto entry = list.substr(at, end - at);
                    const auto colon = entry.find(':');
                    if (colon != std::string::npos) snapshots[static_cast<unsigned>(std::stoul(entry))] = entry.substr(colon + 1);
                    at = end + 1;
                }
            }
            runtime.on_display_flip = [&runtime, viewer_mode, gpu_only] {
                const auto flip = runtime.gs_display_flips;
                // BT2_SAMPLE_PROFILE=<path>: sample where this thread executes, from flip
                // BT2_SAMPLE_FROM (default 1) to the end of the run (host/sample_profiler.h).
                static std::unique_ptr<SampleProfiler> profiler;
                static const char* profile_path = std::getenv("BT2_SAMPLE_PROFILE");
                static const unsigned profile_from = std::getenv("BT2_SAMPLE_FROM") ? static_cast<unsigned>(std::stoul(std::getenv("BT2_SAMPLE_FROM"))) : 1;
                if (profile_path && !profiler && flip >= profile_from) profiler = std::make_unique<SampleProfiler>(profile_path);
                if (gpu_only) gpu_mirror.flip(); else gpu_mirror.compare(runtime.gif_dma.gs, flip);
                const auto snapshot = snapshots.find(flip);
                // In fast forward a picture is read back and shown only once per field period of the clock.
                const bool show = viewer_mode && (!pacer || pacer->pictureDue());
                if (!show && snapshot == snapshots.end()) return;
                // BT2_VIEWER_STATS: every 120 flips, the wall-clock rate of flips and fields and the time
                // per flip spent on the renderer's flush, on reading the picture back and on showing it.
                using Clock = std::chrono::steady_clock;
                static const bool stats = std::getenv("BT2_VIEWER_STATS") != nullptr;
                frame_profile.enabled = stats;
                static Clock::time_point window_start = Clock::now();
                static double scanout_s = 0, present_s = 0, slowest_s = 0, waited_at_window = 0;static unsigned window_fields = 0, late = 0;
                static Clock::time_point last_flip = Clock::now();
                const auto t0 = Clock::now();
                // On the GPU path the picture stays in the renderer; it is read back only for a snapshot.
                const bool on_gpu = viewer.gpuPresented();
                DisplayFrame frame;unsigned width = 0, height = 0;
                if (on_gpu) std::tie(width, height) = gpu_mirror.vsync(runtime.gs_privileged, runtime.gs_crt_request[1], flip);
                else {frame = gpu_mirror.scanout(runtime.gs_privileged, runtime.gs_crt_request[1], flip);width = frame.width;height = frame.height;}
                const auto t1 = Clock::now();
                if (snapshot != snapshots.end()) {
                    // The field is what a pad script counts in; flips are what snapshots count in.
                    writePpm(snapshot->second, on_gpu ? gpu_mirror.readback() : frame);
                    std::cerr << "Snapshot: flip " << flip << " at field " << runtime.vblank_deliveries << std::endl;
                }
                if (!show || !width) return;
                const double waited_before = pacer ? pacer->waited_seconds : 0;
                if (pacer) pacer->wait(); // The picture appears at the field's deadline.
                const auto t_shown = Clock::now();
                if (player) player->shown(on_gpu ? nullptr : &frame);
                if (on_gpu) viewer.presentRenderer(width, height, flip); else viewer.present(frame, flip);
                if (stats) {
                    const auto t2 = Clock::now();
                    scanout_s += std::chrono::duration<double>(t1 - t0).count();present_s += std::chrono::duration<double>(t2 - t_shown).count();
                    // The slowest single flip of the window, and how many took more than 1.25 fields (20.9 ms):
                    // at 60 flips per second each of those is a visible dip.
                    const double this_flip = std::chrono::duration<double>(t2 - last_flip).count();last_flip = t2;
                    slowest_s = std::max(slowest_s, this_flip);if (this_flip > 0.0209) ++late;
                    if (flip % 120 == 0) {
                        const double wall = std::chrono::duration<double>(t2 - window_start).count();
                        std::cerr << std::dec << "Viewer stats at flip " << flip << ": " << 120 / wall << " flips/s, " << (runtime.vblank_deliveries - window_fields) / wall
                            << " fields/s; per flip " << wall / 120 * 1000 << " ms (slowest " << slowest_s * 1000 << " ms, " << late << " over 20.9 ms), work " << (wall - ((pacer ? pacer->waited_seconds : 0) - waited_at_window)) / 120 * 1000 << " ms, of it scanout " << scanout_s / 120 * 1000 << " ms, show " << present_s / 120 * 1000
                            << " ms, VIF1 " << (frame_profile.vif1 - frame_profile.vu1) / 120 * 1000 << " ms, VU1 with its GS packets " << frame_profile.vu1 / 120 * 1000 << " ms, GS packets (all) " << frame_profile.gs / 120 * 1000 << " ms, of it in the GPU renderer " << frame_profile.renderer / 120 * 1000
                            << " ms, GPU flush " << frame_profile.flush / 120 * 1000 << " ms" << std::endl;
                        window_start = t2;scanout_s = present_s = slowest_s = 0;late = 0;waited_at_window = pacer ? pacer->waited_seconds : 0;(void)waited_before;window_fields = runtime.vblank_deliveries;frame_profile = {};frame_profile.enabled = true;
                    }
                }
                if (viewer.closed()) {std::cerr << "Viewer closed; ending the run." << std::endl; std::exit(0);}
            };
        }
#else
        if (std::getenv("BT2_GPU_GS")) throw std::runtime_error("BT2_GPU_GS needs a probe built with -DBT2_GPU_GS=ON");
#endif
        if (const char* phase = std::getenv("BT2_PHASE_STOP")) runtime.phase_stop = static_cast<std::uint32_t>(std::stoul(phase, nullptr, 16));
        runtime.loadMemory(image);
        // BT2_GAME_PATCHES=<name>[,<name>]: the player's optional patches of the executable (ee/game_patches.h).
        if (const char* list = std::getenv("BT2_GAME_PATCHES"))
            for (const GamePatch* patch : parseGamePatches(list)) {applyGamePatch(runtime.memory(), *patch);std::cerr << "Game patch: " << patch->name << std::endl;}
        const std::uint32_t osd_word=loadStartupEvidence(runtime,modes,argv);
        const std::vector<std::uint8_t> kernel_before(runtime.kernelBaseline().begin(), runtime.kernelBaseline().end());
        constexpr std::uint32_t zero_start=startup_zero_start,zero_end=startup_zero_end;
        runtime.expectStartupZeroRegion(zero_start, zero_end);
        // Poison only the observed startup zero region so zero stores are observable.
        std::fill(runtime.memory().begin() + zero_start, runtime.memory().begin() + zero_end, std::uint8_t{0xA5});
        R5900Context context{};
        for (auto& reg : context.r) reg = {UINT64_MAX, UINT64_MAX};
        context.f.fill(1.0f);
        context.hi = context.lo = context.hi1 = context.lo1 = UINT64_MAX;
        context.fcr31 = context.sa = UINT32_MAX;
        context.f_acc = 1.0f;
        loadContinuationEvidence(runtime,modes,argv);
        registerStartupFunctions(runtime);
        registerKernelPayloads(runtime,modes);
        // BT2_START_BATTLE / BT2_START_SCREEN / BT2_START_EPISODE: a CPU-against-CPU battle, a screen or a
        // story episode without input, for tool runs (game_starter.h).
        {
            const char* battle = std::getenv("BT2_START_BATTLE");
            const char* screen = std::getenv("BT2_START_SCREEN");
            if ((battle || screen) && std::getenv("BT2_PLAYER_SETTINGS")) throw std::runtime_error("BT2_START_BATTLE and BT2_START_SCREEN are for tool runs, not a player's session");
            const char* episode = std::getenv("BT2_START_EPISODE");
            if ((battle != nullptr) + (screen != nullptr) + (episode != nullptr) > 1) throw std::runtime_error("BT2_START_BATTLE, BT2_START_SCREEN and BT2_START_EPISODE exclude each other");
            if (episode && std::getenv("BT2_PLAYER_SETTINGS")) throw std::runtime_error("BT2_START_EPISODE is for tool runs, not a player's session");
            if (battle) installBattleStarter(runtime, parseBattleStart(battle));
            if (screen) installScreenStarter(runtime, parseScreenStart(screen));
            if (episode) installEpisodeStarter(runtime, parseEpisodeStart(episode));
        }
        // The interpreter bridge for code no generated function holds (ee_interpreter.cpp). BT2_EE_INTERPRET:
        // "off" stops at such code as before; "all" runs every generated function interpreted as well
        // (a diagnostic comparison with the compiled run through BT2_STATE_HASH).
        {
            const char* interpret = std::getenv("BT2_EE_INTERPRET");
            const std::string mode = interpret ? interpret : "";
            if (!mode.empty() && mode != "off" && mode != "all") throw std::runtime_error("BT2_EE_INTERPRET must be off or all");
            if (mode != "off") runtime.setInterpreterHandlers({ee_interpreter_handler_parts, ee_interpreter_handler_part_count});
            runtime.interpret_all = mode == "all";
        }
        if (argc >= 18) loadScmdEvidence(runtime,argv);
        bool stopped = false,execution_budget_exhausted=false;
        std::uint32_t stop_address = 0;
        std::size_t stop_size = 0;
        std::uint32_t stop_selector = 0, stop_handler = 0;
        std::uint32_t stop_target = 0, stop_source = 0, stop_resume = 0;
        try { runtime.callFunction(0x00100008, context); }
        catch (const UnsupportedGuestBranch& stop) {
            if(runtime.ee_sound_probe_attempted) context=runtime.ee_sound_probe_frontier;
            std::cerr << "Branch target/source/resume: " << std::hex << stop.target << "/" << stop.source_pc << "/" << stop.resume_pc << "\n";
            if (!bind_mode) throw;
            stop_target = stop.target; stop_source = stop.source_pc; stop_resume = stop.resume_pc; stopped = true;
        }
        catch (const GifPacketMismatch& mismatch) {
            // Diagnostics follow the caller's report path; pipeline uses ignored work/.
            for(const auto& item : {std::pair{".gif-expected.bin",&mismatch.expected},
                                    std::pair{".gif-actual.bin",&mismatch.actual}}) {
                std::ofstream dump(std::string(argv[2])+item.first,std::ios::binary);
                if(!dump.write(reinterpret_cast<const char*>(item.second->data()),item.second->size()))
                    throw std::runtime_error("Could not write GIF mismatch diagnostic");
            }
            throw;
        }
        catch (const UnsupportedGuestMemory& stop) {
            if(runtime.ee_sound_probe_attempted) context=runtime.ee_sound_probe_frontier;
            std::cerr << "Memory address/pc/acks/polls: " << std::hex << stop.address << "/" << context.pc << "/" << runtime.gs_interrupt.acknowledgements << "/" << runtime.gs_interrupt.polls << "\n";
            std::cerr << "Memory sp-offset/ra/main/cri/graph/display: " << std::hex << (runtime.initial_sp-context.r[29].low) << "/" << context.r[31].low << "/" << runtime.main_priority_admissions << "/" << std::dec << runtime.cri_rpc_calls << "/" << runtime.graph_startup_phase << "/" << runtime.display_reset_phase << " stat/polled/mask " << std::hex << runtime.display_intc_stat << "/" << runtime.polled_vblanks << "/" << runtime.intc_mask << "\n";
            if (dma5_mode && !(argc>=18 && runtime.reset_ready_checkpoint_reads)) throw;
            const bool vif_handoff=runtime.native_module_loads==14 && runtime.heap_frees==1 && runtime.dmac_startup_writes==3
                && stop.address==0x10003C10 && stop.size==4 && context.pc==0x30FF70 && context.r[29].low==runtime.initial_sp-32;
            const bool texture_handoff=runtime.graph_startup_phase==4 && runtime.vif_startup.phase==6
                && stop.address==0x1000A000 && stop.size==4 && context.pc==0x28B928
                && context.r[29].low==runtime.initial_sp-128 && runtime.gif_dma.completions==2;
            // Frame phase (after the first display flip, main thread): a refused access to a hardware
            // register (EE registers or GS privileged registers) is the checked boundary of the next
            // subsystem, named by its address. Refused accesses elsewhere (RAM) remain failures.
            const bool hardware_register=(stop.address>=0x10000000 && stop.address<0x10010000) || (stop.address>=0x12000000 && stop.address<0x12002000);
            const bool frame_register_boundary=runtime.gs_display_flips>=1 && runtime.current_thread_id==1 && (stop.size==4 || stop.size==8 || runtime.frame_budget_reached)
                && hardware_register;
            // Frame-phase GS reset (the GS library's CSR reset during the frame loop; next subsystem).
            const bool frame_gs_reset=runtime.gs_display_flips>=1 && runtime.current_thread_id==1 && runtime.display_reset_phase==13
                && stop.address==0x12001000 && stop.size==8 && context.pc==0x30FE44;
            if(runtime.post_sif.supplied && !vif_handoff && !texture_handoff && !frame_register_boundary && !frame_gs_reset) throw; // Explicit subsystem boundaries only.
            if(vif_handoff || texture_handoff || frame_register_boundary || frame_gs_reset) { /* Preserve the checked memory stop; no unsupported request succeeds. */ }
            else if (argc>=18 && runtime.reset_ready_checkpoint_reads) require(
                (stop.address==0x12001000 && stop.size==8 && context.pc==0x30FE44)
                || (stop.address==0x1000F000 && stop.size==4 && context.pc==0x32F10C)
                || (runtime.gs_interrupt.acknowledgements==2 && stop.address==0x12001000 && stop.size==8 && context.pc==0x3104F4)
                || (runtime.gs_field_reads==1 && stop.address==0x1000A000 && stop.size==4 && context.pc==0x3100E4)
                || (runtime.gif_dma.starts==1 && stop.address==0x10009000 && stop.size==4 && context.pc==0x310560)
                || (runtime.gif_dma.starts==2 && stop.address==0x1000E010 && stop.size==4 && context.pc==0x3319CC)
                || (runtime.gif_dma.ready() && stop.address==0x1000A000 && stop.size==4 && context.pc==0x3101A8),
                "Unexpected GS continuation memory boundary");
            else if (dma_status_mode) require(stop.address == 0x1000C000 && stop.size == 4
                    && context.pc == 0x003319EC && runtime.dma_status_reads == 1,
                    "Unexpected DMA channel control boundary");
            else if (osd_readback_mode) require(stop.address == 0x1000E010 && stop.size == 4
                    && context.pc == 0x003319CC && GPR_U32(&context, 4) == 0
                    && runtime.semaphore_waits == 1 && runtime.semaphore_signals == 1,
                    "Unexpected DMA status boundary");
            else if (readback_mode) throw;
            else if (intc_mode) require(stop.address == 0x10001010 && stop.size == 4 && context.pc == 0x00338DD8
                    && runtime.intc_mask == 0x1802 && runtime.intc_enable_calls == 1
                    && runtime.timer2_mode_reads == 1 && runtime.timer2_mode_writes == 1,
                    "Unexpected post-EnableIntc timer boundary");
            else if (timer2_mode) throw;
            else if (timer_mode) require(stop.address == 0x10001010 && stop.size == 4 && context.pc == 0x00338C58,
                                    "Unexpected Timer 2 MMIO boundary");
            else if (snapshot_mode) require(stop.address == 0x10001810 && stop.size == 4 && context.pc == 0x00338A1C,
                                       "Unexpected snapshot MMIO boundary");
            else require(stop.address == 0x80000000 && stop.size == 4 && context.pc == 0x00338388,
                         "Unexpected entry memory boundary");
            stop_address = stop.address;
            stop_size = stop.size;
            stopped = true;
        }
        catch (const UnsupportedSyscall& stop) {
            if(runtime.ee_sound_probe_attempted) context=runtime.ee_sound_probe_frontier;
            if (bind_mode) std::cerr << "Syscall selector/handler/pc: " << std::hex << stop.selector << "/" << stop.handler << "/" << context.pc << "\n";
            else if (sif_rpc_mode) require(stop.selector == 0x77 && !stop.handler && stop.resume_pc == 0x0032F028
                    && context.pc == stop.resume_pc && GPR_U32(&context, 4) == runtime.initial_sp - 464
                    && GPR_U32(&context, 5) == 1 && runtime.sif_dma_submissions == 1 && runtime.sif_rpc_reads == 1,
                    "Unexpected first RPC bind submission boundary");
            else if (sif_dma_mode) require(stop.selector == 0x7A && !stop.handler && stop.resume_pc == 0x0032F078
                    && context.pc == stop.resume_pc && GPR_U32(&context, 4) == 0x80000002
                    && runtime.sif_dma_submissions == 1 && runtime.sif_rpc_reads == 0,
                    "Unexpected RPCINIT query boundary");
            else if (sif_mode) require(stop.selector == 0x77 && !stop.handler && stop.resume_pc == 0x0032F028
                    && context.pc == stop.resume_pc && GPR_U32(&context, 4) == runtime.initial_sp - 240
                    && GPR_U32(&context, 5) == 1 && runtime.sif_command_reads == 1
                    && runtime.data_cache_lines == 1, "Unexpected first SIF DMA submission boundary");
            else if (dma5_mode) require(stop.selector == 0x7A && !stop.handler && stop.resume_pc == 0x0032F078
                    && context.pc == stop.resume_pc && GPR_U32(&context, 4) == 0x80000000,
                    "Unexpected first SIF register boundary");
            else if (osd_readback_mode) throw;
            else if (osd_mode) require(stop.selector == 0x4A && !stop.handler && stop.resume_pc == 0x0032ED08
                    && context.pc == stop.resume_pc && GPR_U32(&context, 4) == runtime.initial_sp - 124
                    && runtime.osd_config_reads == 1 && runtime.priority_changes == 1
                    && runtime.read32(runtime.memory().data(), runtime.initial_sp - 128) == osd_word
                    && runtime.read32(runtime.memory().data(), runtime.initial_sp - 124) == ((osd_word & 0xFFFF1FFF) | 0x2000),
                    "Unexpected SetOsdConfigParam boundary");
            else if (readback_mode) require(stop.selector == 0x4B && !stop.handler && stop.resume_pc == 0x0032ED18
                    && context.pc == stop.resume_pc && GPR_U32(&context, 4) == runtime.initial_sp - 128
                    && runtime.main_thread_priority == 1 && runtime.priority_changes == 1
                    && runtime.timer2_mode_reads == 2 && runtime.timer2_mode_writes == 1
                    && runtime.intc_mask == 0x1802 && runtime.intc_enable_calls == 1,
                    "Unexpected GetOsdConfigParam boundary after priority change");
            else require(!intc_mode && timer2_mode && stop.selector == 0x14 && !stop.handler && stop.resume_pc == 0x0032E9A8
                    && context.pc == stop.resume_pc && GPR_U32(&context, 4) == 11
                    && runtime.timer2_mode_writes == 1 && runtime.timer2_mode_after_write == 0x382
                    && context.cop0_errorepc == 0x00338C90 && !context.ll_bit,
                    "Unexpected syscall after generated Timer 2 write/ERET");
            stop_selector = stop.selector;
            stop_handler = stop.handler;
            stopped = true;
        }
        catch (const std::runtime_error& error) {
            if(!bind_mode || !std::string_view(error.what()).starts_with("Bootstrap execution budget exceeded;"))throw;
            std::cerr << error.what() << "\n";execution_budget_exhausted=true;stopped=true;
        }
        const ProbeStop stop{stopped,execution_budget_exhausted,stop_address,stop_size,stop_selector,stop_handler,stop_target,stop_source,stop_resume};
        if (cd_mode && argc < 18) {
            checkCdContinuation(runtime,context,stop);
            writeCdReport(runtime,context,stop,argv);
            std::cout << "CD-init continuation report written.\n";
            return 0;
        }
        if (argc >= 18) {
            checkRuntimeBoundary(runtime,context,stop);
            writeRuntimeBoundaryReport(runtime,context,stop,argv);
            writeEvidenceDumps(runtime,argv);
            return 0;
        }
        if (bind_mode) {
            checkBindContinuation(runtime,context,stop,kernel_before);
            writeBindReport(runtime,context,stop,argv);
            std::cout << "Generated RPC bind and cleanup returned; first CD-init RPC call reached a checked runtime boundary.\n";
            return 0;
        }
        const std::array<std::uint32_t, 5> args{GPR_U32(&context, 4), GPR_U32(&context, 5),
            GPR_U32(&context, 6), GPR_U32(&context, 7), GPR_U32(&context, 8)};
        checkEntryBoundary(runtime,context,modes,stop,kernel_before,image,args);
        writeEntryReport(runtime,context,modes,stop,args,argv);
        printEntrySummary(context,modes,stop);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Entry probe failed: " << error.what() << '\n';
        return 1;
    }
}
