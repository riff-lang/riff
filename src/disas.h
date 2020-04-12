#ifndef DISAS_H
#define DISAS_H

#include "code.h"

void disas_block(codeblock_t *, const char *);
int disas_ins(codeblock_t *, int);

#endif
