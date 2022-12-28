#include "state.h"

void riff_state_init(riff_state *s) {
    *s = (riff_state) {
        .name = NULL,
        .src  = NULL,
        .argc = 0,
        .arg0 = 0,
        .argv = NULL,
        .nf   = 0,
        .fcap = 0,
        .fn   = NULL,
    };
}
