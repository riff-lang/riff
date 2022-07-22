#include "state.h"

void riff_state_init(riff_state *s) {
    s->pname = NULL;
    s->src = NULL;
    s->argc = 0;
    s->arg0 = 0;
    s->argv = NULL;
    s->nf = 0;
    s->fcap = 0;
    s->fn = NULL;
}
