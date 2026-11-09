#include "probe_state.h"

// The run's inputs: the player's disc image. The console's state at the game's start is the runtime's
// (EeRuntime::bootState); nothing captured from the console is read.

void loadStartupEvidence(EeRuntime& runtime,char** argv) {
    runtime.local_disc_path=argv[2];
    runtime.bootState();
}
