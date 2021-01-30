#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "util.h"

char *u_file2str(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "file not found: %s\n", path);
        exit(1);
    }
    fseek(file, 0L, SEEK_END);
    size_t s = ftell(file);
    rewind(file);
    char *buf  = malloc(s + 1);
    size_t end = fread(buf, sizeof(char), s, file);
    buf[end]   = '\0';
    return buf;
}

// djb2
// source: http://www.cse.yorku.ca/~oz/hash.html
uint32_t u_strhash(const char *str) {
    uint32_t h = 5381;
    unsigned int c;
    while ((c = *str++))
        h = ((h << 5) + h) + c;
    return h;
}

int u_decval(int c) {
    return c - '0';
}

int u_hexval(int c) {
    return isdigit(c) ? c - '0' : tolower(c) - 'a' + 10;
}

// Returns the numeric value of the character `c` for a given radix.
// Returns -1 if the character is outside the range of the base.
int u_baseval(int c, int base) {
    int val = -1;
    if (isdigit(c))
        val = c - '0';
    else if (isalpha(c))
        val = tolower(c) - 'a' + 10;
    if (val >= 0 && val > (base - 1))
        val = -1;
    return val;
}

#define MAXNUMSTR 128

double u_str2d(const char *str, char **end, int base) {

    // Eat leading whitespace
    while (isspace(*str))
        ++str;

    char buf[MAXNUMSTR];
    int n    = 0;
    int frac = 0;

    // Leading unary '+' or '-'
    if (*str == '+' || *str == '-')
        buf[n++] = *str++;

    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        if (!base)
            base = 16;
        buf[n++] = *str++;
        buf[n++] = *str++;
    } else if (base == 16) {
        buf[n++] = '0';
        buf[n++] = 'x';
    }

parse_digits:
    // Parse digits for integer and fractional part (if applicable)
    if (base == 16) {
        for (; *str && (n < (MAXNUMSTR - 1)) && (isxdigit(*str) || *str == '_'); ++str) {
            if (*str == '_')
                continue;
            buf[n++] = *str;
        }
    } else {
        for (; *str && (n < (MAXNUMSTR - 1)) && (isdigit(*str) || *str == '_'); ++str) {
            if (*str == '_')
                continue;
            buf[n++] = *str;
        }
    }

    if (!frac && *str == '.') {
        buf[n++] = *str++;
        frac = 1;
        goto parse_digits;
    }

    if (base == 16) {
        if (str[0] == 'p' || str[0] == 'P') {
            if (str[1] == '-' || str[1] == '+') {
                // Digits after `[Pp]` are expected to be base-10
                if (isdigit(str[2])) {
                    buf[n++] = 'p';
                    buf[n++] = str[1];
                    str += 2;
                    goto parse_exp;
                }
            } else if (isdigit(str[1])) {
                buf[n++] = 'p';
                ++str;
                goto parse_exp;
            }
        } else
            goto parse_end;
    } else {
        if (str[0] == 'e' || str[0] == 'E') {
            if (str[1] == '-' || str[1] == '+') {
                if (isdigit(str[2])) {
                    buf[n++] = 'e';
                    buf[n++] = str[1];
                    str += 2;
                    goto parse_exp;
                }
            } else if (isdigit(str[1])) {
                buf[n++] = 'e';
                ++str;
                goto parse_exp;
            }
        } else
            goto parse_end;
    }

// Digits following [EePp][+-]? are expected to be base-10
parse_exp:
    for (; *str && (n < (MAXNUMSTR - 1)) && (isdigit(*str) || *str == '_'); ++str) {
        if (*str == '_')
            continue;
        buf[n++] = *str;
    }

parse_end:
    buf[n] = '\0';
    *end = (char *) str;
    char *dummy;
    return strtod(buf, &dummy);
}

// Riff's integer literal format
// Wrapper for strtoll()/strtoull()
// - Allows arbitrary underscores in numeric strings
// - Prevents numbers prefixed with `0` from being interpreted as
//   octal by default (base 0).
int64_t u_str2i64(const char *str, char **end, int base) {

    // Reset base if outside valid range
    if (base < 2 || base > 36)
        base = 0;

    char buf[MAXNUMSTR];
    int n = 0;

    // Eat leading whitespace
    while (isspace(*str))
        ++str;

    // Leading unary '+' or '-'
    if (*str == '+' || *str == '-')
        buf[n++] = *str++;

    if (*str == '0') {
        if (str[1] == 'x' || str[1] == 'X') {
            if (!base)
                base = 16;
            if (base == 16)
                str += 2;
        } else if (str[1] == 'b' || str[1] == 'B') {
            if (!base)
                base = 2;
            if (base == 2)
                str += 2;
        }
    }

    // Set default base to 10 if binary or hex was not detected. This
    // prevents unintentional octal conversion.
    if (!base) base = 10;

    // Copy valid digits to buffer, ignoring underscores
    for (; *str && (n < (MAXNUMSTR - 1)) && (*str == '_' || (u_baseval(*str, base) >= 0)); ++str) {
        if (*str == '_')
            continue;
        buf[n++] = *str;
    }
    buf[n] = '\0';

    // Set the end pointer and pass a dummy pointer to strtoll()
    *end = (char *) str;
    char *dummy;
    return base == 10 ? strtoll(buf, &dummy, base)
                      : (int64_t) strtoull(buf, &dummy, base);
}
