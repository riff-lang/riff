#ifndef ENV_H
#define ENV_H

typedef struct {
    int          argc;
    char       **argv;
    const char  *src; // Source program
} rf_env;

#endif
