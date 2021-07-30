#ifndef UTIL_H
#define UTIL_H

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#define u_int2str(i,b,n) snprintf(b, n, "%"PRId64, i)
#define u_flt2str(f,b,n) snprintf(b, n, "%g", f)

char     *u_file2str(const char *);
uint32_t  u_strhash(const char *);
int       u_decval(int);
int       u_hexval(int);
int       u_baseval(int, int);
double    u_str2d(const char *, char **, int);
int64_t   u_str2i64(const char *, char **, int);
int64_t   u_utf82unicode(const char *, char **);
int       u_unicode2utf8(char *, uint32_t);

#endif
