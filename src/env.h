#ifndef ENV_H
#define ENV_H

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
    rf_fn        main;  // Entry point for execution
    rf_fn      **fn;    // Array of user functions
} rf_env;

void e_init(rf_env *);

#endif
