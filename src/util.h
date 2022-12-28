#ifndef UTIL_H
#define UTIL_H

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __GNUC__
#define riff_likely(x)   (__builtin_expect(!!(x), 1))
#define riff_unlikely(x) (__builtin_expect(!!(x), 0))
#else
#define riff_likely(x)   (x)
#define riff_unlikely(x) (x)
#endif

#define FOREACH(a,i) \
    for (int (i) = 0; i < ((sizeof (a)) / (sizeof (a)[0])); ++(i))

#define rol(x,n)       (((x)<<(n)) | ((x)>>(-(int)(n)&(8*sizeof(x)-1))))

#define riff_lltostr(i,b)  snprintf(b, sizeof b, "%"PRId64, i)
#define riff_dtostr(d,b)   snprintf(b, sizeof b, "%g", d)
#define riff_strchr(s,c)  (!!memchr(s, c, strlen(s)))

// Generic dynamic array utilities
#define VEC_GROWTH_FACTOR 2
#define VEC_INITIAL_CAP   8

#define RIFF_VEC(T)                                                 \
    struct {                                                        \
        T      *list;                                               \
        size_t  n;                                                  \
        size_t  cap;                                                \
    }

#define RIFF_VEC_FOREACH(v,var)                                     \
    for (int (var) = 0; (var) < (v)->n; ++(var))

#define RIFF_VEC_GET(v,idx)                                         \
    ((v)->list[idx])

#define riff_vec_init(v)                                            \
    do {                                                            \
        (v)->list = NULL;                                           \
        (v)->n    = 0;                                              \
        (v)->cap  = 0;                                              \
    } while (0)

#define riff_vec_add(v,item)                                        \
    do {                                                            \
        if (riff_unlikely((v)->cap <= (v)->n)) {                    \
            (v)->cap = (v)->cap == 0                                \
                ? VEC_INITIAL_CAP                                   \
                : (v)->cap * VEC_GROWTH_FACTOR;                     \
            (v)->list = realloc((v)->list, (v)->cap * sizeof *(v)); \
        }                                                           \
        (v)->list[(v)->n++] = (item);                               \
    } while (0)

double  riff_strtod(const char *, char **, int);
int64_t riff_strtoll(const char *, char **, int);
int64_t riff_utf8tounicode(const char *, char **);
int     riff_unicodetoutf8(char *, uint32_t);

#endif
