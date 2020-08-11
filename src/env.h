#ifndef ENV_H
#define ENV_H

#include "types.h"

typedef struct {
    const char *src;    // Source program
    rf_arr     *argv;   // User-provided arguments
} rf_env;

#endif
