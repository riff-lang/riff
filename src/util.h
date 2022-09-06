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

#define FOREACH(a,i) \
    for (int (i) = 0; i < ((sizeof (a)) / (sizeof (a)[0])); ++(i))

#define rol(x,n)       (((x)<<(n)) | ((x)>>(-(int)(n)&(8*sizeof(x)-1))))

#define riff_lltostr(i,b)  snprintf(b, sizeof b, "%"PRId64, i)
#define riff_dtostr(d,b)   snprintf(b, sizeof b, "%g", d)
#define riff_strchr(s,c)  (!!memchr(s, c, strlen(s)))

double  riff_strtod(const char *, char **, int);
int64_t riff_strtoll(const char *, char **, int);
int64_t riff_utf8tounicode(const char *, char **);
int     riff_unicodetoutf8(char *, uint32_t);

#endif
