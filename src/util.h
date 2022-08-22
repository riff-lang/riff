#ifndef UTIL_H
#define UTIL_H

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __GNUC__
#define LIKELY(x)   (__builtin_expect(!!(x), 1))
#define UNLIKELY(x) (__builtin_expect(!!(x), 0))
#else
#define LIKELY(x)   (x)
#define UNLIKELY(x) (x)
#endif

#define FOREACH(a, i) \
    for (int (i) = 0; i < ((sizeof (a)) / (sizeof (a)[0])); ++(i))

#define rol(x,n) (((x)<<(n)) | ((x)>>(-(int)(n)&(8*sizeof(x)-1))))

#define u_int2str(i,b) snprintf(b, sizeof b, "%"PRId64, i)
#define u_flt2str(f,b) snprintf(b, sizeof b, "%g", f)

int     u_decval(int);
int     u_hexval(int);
int     u_baseval(int, int);
double  u_str2d(const char *, char **, int);
int64_t u_str2i64(const char *, char **, int);
int64_t u_utf82unicode(const char *, char **);
int     u_unicode2utf8(char *, uint32_t);

#endif
