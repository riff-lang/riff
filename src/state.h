#ifndef STATE_H
#define STATE_H

#include "fn.h"
#include "util.h"
#include "value.h"

typedef struct {
    const char           *name;
    const char           *src;
    int                   argc;
    int                   arg0;
    char                **argv;
    riff_fn               main;
    RIFF_VEC(riff_fn *)   fn;
} riff_state;

void riff_state_init(riff_state *);

#endif
