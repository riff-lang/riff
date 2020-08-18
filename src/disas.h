#ifndef DISAS_H
#define DISAS_H

#include "code.h"
#include "env.h"
#include "lex.h"

void d_prog(rf_env *);
void d_tk_stream(const char *);

#endif
