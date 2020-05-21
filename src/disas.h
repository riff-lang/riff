#ifndef DISAS_H
#define DISAS_H

#include "code.h"

void disas_chunk(chunk_t *, const char *);
int disas_ins(chunk_t *, int);

#endif
