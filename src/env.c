#include "env.h"

void e_init(rf_env *e) {
    e->pname = NULL;
    e->src = NULL;
    e->argc = 0;
    e->arg0 = 0;
    e->argv = NULL;
    e->nf = 0;
    e->fcap = 0;
    e->fn = NULL;
}
