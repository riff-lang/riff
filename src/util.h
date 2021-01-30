#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

char     *u_file2str(const char *);
uint32_t  u_strhash(const char *);
int       u_decval(int);
int       u_hexval(int);
int       u_baseval(int, int);
double    u_str2d(const char *, char **, int);
int64_t   u_str2i64(const char *, char **, int);

#endif
