#ifndef STATE_H
#define STATE_H

#include "fn.h"
#include "types.h"

typedef struct {
    const char  *pname; // Name of program file
    const char  *src;   // Source program
    int          argc;
    int          arg0;
    char       **argv;
    int          nf;    // Number of user functions
    int          fcap;
    riff_fn      main;  // Entry point for execution
    riff_fn    **fn;    // Array of user functions
} riff_state;

void riff_state_init(riff_state *);

#endif
