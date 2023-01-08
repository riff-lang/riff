#include "state.h"

void riff_state_init(riff_state *s) {
    *s = (riff_state) {
        .name = NULL,
        .src  = NULL,
        .argc = 0,
        .arg0 = 0,
        .argv = NULL,
    };
    riff_fn_init(&s->main, NULL);
    riff_vec_init(&s->fn);
}
