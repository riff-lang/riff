#ifndef ENV_H
#define ENV_H

#include "types.h"

typedef struct {
    const char  *src;   // Source program
    int          argc;
    int          ff;
    char       **argv;
} rf_env;

#endif
