// 60 fps battles (docs/plans/graphics/HIGH_FRAME_RATE_PLAN.md, way 4): a picture between each two fight
// frames, drawn from the game's own commands with the camera and the bones halfway. The game keeps
// stepping 30 times a second exactly as before; this only draws and never touches guest state.
//
// Each frame's GS input is recorded in order: VIF1 transfers (with the GS data their run produced) and
// all other GIF data. A second renderer on the main one's GPU device draws the in-between picture of the
// last two frames: the newer frame's input with its VIF1 lists blended against the older frame's, run on
// a copy of the VIF1/VU1 state at the newer frame's start, from the main renderer's GS state on the GPU as
// it was then (copied at each flip), and scans it out at the newer frame's flip. A frame without an
// in-between picture (not a fight, or lists that do not align) is replayed instead, so the second
// renderer still takes every register the game writes. The frame recorded up to flip F is on screen at
// flip F + 1 (checked pixel for pixel), so the picture between F - 1 and F can be shown a field after flip
// F, before F's own: no delay is added.
// A fight frame's VIF1 list (one transfer of about 600,000 words) comes right after the flip before it,
// about 33 ms before its own flip, and nothing follows it. So when the older frame was a fight frame, the
// newer one is drawn as it comes: each VIF1 transfer is handed, with the input before it, to a thread of
// its own that blends and runs it while the game goes on, and the second renderer's GS thread names and
// draws the result meanwhile (gs/gpu/gpu_gs_mirror.h); the second renderer submits after each, so the
// device's frame changes never wait for it. At the flip only the check that the lists paired and the
// scanout are left. The game's thread waits for the thread only where it needs the result (wait, for
// checks and snapshots); otherwise the thread may run up to a frame behind (keepPace): a picture it
// finishes after its field is left out, and the game's thread waits only when the thread is more than a
// frame behind, so a slower processor costs in-between pictures, not the game's pace.
#pragma once
#include "gs/cpu/gs_command_sink.h"
#include "gs/gpu/gpu_gs_mirror.h"
#include "gs/textures/gs_texture_names.h"
#include "vu/vif1.h"
#include "vu/vif1_blend.h"
#include <array>
#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>
#include <memory>
#include <optional>
#include <span>
#include <vector>

class InBetweenFrames {
public:
    // `vif1` is the VIF1 state now (the first frame's start). With HD textures the second renderer names
    // textures itself and draws with the primary's pack. `replay_only` (for checks): the frames are only
    // replayed (no in-between pictures) and each replay is scanned out, so the second renderer's picture
    // can be compared with the main one's.
    InBetweenFrames(GpuGsMirror& primary,const Vif1& vif1,unsigned super_sampling,bool hd_textures,bool replay_only=false);
    ~InBetweenFrames();

    // Recording, from the runtime's hooks: a VIF1 transfer before it runs, and each GS input (`from_vif1`:
    // produced by the VIF1 transfer that runs now).
    void vif1Transfer(std::span<const std::uint32_t> words);
    void gsInput(std::span<const std::uint8_t> data,bool from_vif1);

    // After a display flip's own work (the main renderer's flip and picture): the frame recorded since
    // the last flip is complete; `fight` says it was a fight frame (it waited for the fight's 30 fps step).
    // `vif1` is the VIF1 state as the next frame starts, and the main renderer's state then is in its
    // keepState slot `next_slot`. The thread finishes the in-between picture of the last two frames when
    // both were fight frames whose lists pair, scanning it out with `privileged` (display registers that
    // show the newer frame's buffer); otherwise it replays the newer frame.
    void start(bool fight,const Vif1& vif1,const std::array<std::uint64_t,15>& privileged,unsigned crt_mode,unsigned field,unsigned next_slot);
    // Waits for the frame (rethrowing its failure); returns whether an in-between picture is
    // ready. Called before the main renderer's flip, before showing or reading the second renderer's
    // picture, and harmless when nothing is running.
    bool wait();
    // At a flip, instead of wait: returns when the thread is at most one frame behind (rethrowing a failure).
    // A thread that keeps falling behind (more than a quarter of the last 64 flips waited for) means a
    // processor too slow for the in-between pictures: it throws, and the caller turns them off.
    void keepPace();
    // Whether the frame is done (without waiting) and its in-between picture is ready.
    bool ready();
    bool busy(); // Whether the frame is still to be drawn (without waiting).
    // Whether to show a picture that is `ready` at its field: only while the in-between pictures keep up
    // (at least 29 of the last 32 were ready). Shown now and then they would make an uneven cadence that
    // looks worse than a steady 30 (Tenkaichi3Decomp likewise shows only as many as fit).
    bool keepsUp(bool ready);
    std::pair<unsigned,unsigned> pictureSize() const {return picture_;}
    // The jobs by number (the n-th start is job n - 1). For pictures shown a field later than drawn (the
    // delayed fights): whether job `job` is done with its in-between picture, held in the renderer's slot
    // job % held_slots (GpuGsMirror::showHeld), and its size; the last job started.
    bool readyFor(std::uint64_t job);
    std::pair<unsigned,unsigned> pictureSizeFor(std::uint64_t job);
    std::uint64_t lastJob() const {return started_-1;} // The game's thread's, after a start.
    GpuGsMirror& renderer() {return mirror_;}
    const bool replay_only;
    // For checks: the "in-between" picture is drawn all the way to the newer frame (blend 1), so it must
    // equal the main renderer's picture of that frame.
    bool scan_replay=false;
    unsigned drawn=0,unpaired=0,not_fights=0,shown=0,camera_cuts=0,late=0; // shown and late: counted by the caller.
    std::uint64_t refused_pairs=0,blended_words=0; // Changed unpack pairs left as they are, words blended (vu/vif1_blend.h).
    double waited_seconds=0; // The game's thread waiting for the frame.
    unsigned paced_waits=0; // Flips at which keepPace waited (more than 2 ms).
    double drawing_seconds=0,slowest_seconds=0; // The thread's time on the in-between pictures drawn, and the longest.

private:
    // An event's GS data: the transfers one after another, `ends` where each ends.
    struct Event {
        bool vif1=false;std::vector<std::uint32_t> words;std::vector<std::uint8_t> output;std::vector<std::size_t> ends;
        // The list's unpacks, walked when first needed (as the newer frame, then kept for its turn as the older).
        mutable bool walked=false;mutable std::optional<std::vector<Vif1Unpack>> unpacks;
        const std::vector<Vif1Unpack>* walk() const {if(!walked) {unpacks=vif1Unpacks(words);walked=true;}return unpacks?&*unpacks:nullptr;}
        template<class Each> void eachOutput(Each each) const {
            std::size_t from=0;
            for(const auto end:ends) {each(std::span<const std::uint8_t>(output.data()+from,end-from));from=end;}
        }
    };
    // Frames are reused, so their buffers keep their size from frame to frame: `used` events are this
    // frame's. A deque: the thread reads the events handed to it while the game's thread adds more.
    // `handed`: the events already handed to the thread (drawn as they came); `slot`: the keepState slot
    // holding the main renderer's state as the frame started.
    struct Frame {std::deque<Event> events;std::size_t used=0,handed=0;std::unique_ptr<Vif1> start;bool fight=false,streamed=false;unsigned slot=0;Event& next(bool vif1);};
    Frame recording_,previous_; // recording_ is the game's thread's; previous_ (the older frame) the thread's.
    std::vector<Frame> free_; // Frames the thread is done with, to record into again (under lock_).
    bool previous_fight_=false; // The game's thread's copy of the last finished frame's `fight`.
    // The thread's job at a flip: the finished frame and how to scan it out.
    struct Job {Frame frame;std::array<std::uint64_t,15> privileged{};unsigned crt_mode=0,field=0;std::uint64_t id=0;};
    struct Held {std::uint64_t job=~0ull;bool ready=false;std::pair<unsigned,unsigned> size{};};
    std::array<Held,GpuGsBackend::held_slots> held_; // By job % held_slots (under lock_).
    // Jobs in order (the thread takes the front one with its task); `started` and `finished` count them.
    std::deque<Job> jobs_;std::uint64_t started_=0,finished_=0;bool ready_=false;std::exception_ptr failure_;
    std::uint64_t recent_waits_=0; // keepPace: whether each of the last 64 flips waited (newest in bit 0).
    std::mutex lock_;std::condition_variable done_;
    void run(); // The task: finishes job_.
    bool draw(Frame finished,const Job& job);
    // The frame being drawn as it comes (the thread's): the VIF1/VU1 copy it runs on, the older frame's VIF1
    // events in order and how many were used, whether the lists paired so far, the time spent.
    struct Stream {std::optional<Vif1> vif1;std::vector<const Event*> older;std::size_t next=0;bool paired=true;double seconds=0;};
    Stream stream_;
    void hand(); // The game's thread: the recorded events not yet handed go to the thread.
    void drawEvents(const std::vector<const Event*>& events,bool first,const Vif1* start,unsigned slot); // The thread: blends and runs them.
    GpuGsMirror& primary_;
    GsCommandSink sink_;
    GsTextureNames names_;
    GsRenderThread thread_; // Tasks only (thread index 1, as the renderer's GS thread); destroyed after the renderer.
    GpuGsMirror mirror_;
    std::pair<unsigned,unsigned> picture_{0,0};
    std::uint32_t recent_=~0u; // Whether each of the last 32 pictures was ready at its field (newest in bit 0).
    void send(std::span<const std::uint8_t> data);
};
