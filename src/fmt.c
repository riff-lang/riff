#include "fmt.h"

#include "conf.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void err(const char *msg) {
    fprintf(stderr, "riff: [fmt] %s\n", msg);
    exit(1);
}

#define FMT_ZERO  1
#define FMT_SIGN  2
#define FMT_SPACE 4
#define FMT_LEFT  8

// %c (if c <= 0x7f)
#define fmt_char(b, n, c) \
    if (flags & FMT_LEFT) { \
        n += sprintf(b + n, "%-*c", width, c); \
    } else { \
        n += sprintf(b + n, "%*c", width, c); \
    }

// %s
#define fmt_str(b, n, s) \
    if (flags & FMT_LEFT) { \
        n += sprintf(b + n, "%-*.*s", width, prec, s); \
    } else { \
        n += sprintf(b + n, "%*.*s", width, prec, s); \
    }

// Signed fmt conversions: floats, decimal integers
#define fmt_signed(b, n, i, fmt) \
    if (flags & FMT_LEFT) { \
        if (flags & FMT_SIGN) { \
            n += sprintf(b + n, "%-+*.*"fmt, width, prec, i); \
        } else if (flags & FMT_SPACE) { \
            n += sprintf(b + n, "%- *.*"fmt, width, prec, i); \
        } else { \
            n += sprintf(b + n, "%-*.*"fmt, width, prec, i); \
        } \
    } else if ((prec < 0) && (flags & FMT_ZERO)) { \
        if (flags & FMT_SIGN) { \
            n += sprintf(b + n, "%+0*"fmt, width, i); \
        } else if (flags & FMT_SPACE) { \
            n += sprintf(b + n, "% 0*"fmt, width, i); \
        } else { \
            n += sprintf(b + n, "%0*"fmt, width, i); \
        } \
    } else if (flags & FMT_SIGN) { \
        n += sprintf(b + n, "%+*.*"fmt, width, prec, i); \
    } else if (flags & FMT_SPACE) { \
        n += sprintf(b + n, "% *.*"fmt, width, prec, i); \
    } else { \
        n += sprintf(b + n, "%*.*"fmt, width, prec, i); \
    }

// Unsigned fmt conversions: hex and octal integers
// Only difference is absence of space flag, since numbers are
// converted to unsigned anyway. clang also throws a warning
// about UB for octal/hex conversions with the space flag.
#define fmt_unsigned(b, n, i, fmt) \
    if (flags & FMT_LEFT) { \
        n += sprintf(b + n, "%-*.*"fmt, width, prec, i); \
    } else if ((prec < 0) && (flags & FMT_ZERO)) { \
        n += sprintf(b + n, "%0*"fmt, width, i); \
    } else { \
        n += sprintf(b + n, "%*.*"fmt, width, prec, i); \
    }

// %b
static int fmt_bin_itoa(char *buf, riff_int num, uint32_t flags, int width, int prec) {
    if (!num && !prec)
        return 0;

    int  size = width > 64 ? width : 64;
    char temp[size];
    int  len = 0;
    do {
        temp[size-(++len)] = '0' + (num & 1);
        num >>= 1;
    } while (num && len < size);

    while (len < prec) {
        temp[size-(++len)] = '0';
    }

    if (!(flags & FMT_LEFT)) {
        char padc = flags & FMT_ZERO ? '0' : ' ';
        while (len < width) {
            temp[size-(++len)] = padc;
        }
        memcpy(buf, temp + (size-len), len);
    } else {
        memcpy(buf, temp + (size-len), len);
        if (len < width) {
            memset(buf + len, ' ', width - len);
            len = width;
        }
    }
    return len;
}

// Optional modifiers (zero or more):
//   +              | Prepend with sign
//   -              | Left-justified
//   <space>        | Space-padded (ignored if left-justified)
//   0              | Zero-padded (ignored if left-justified)
//   <integer> / *  | Field width
//   .              | Precision specifier
//
// Format specifiers:
//   %              | Literal `%`
//   a / A          | Hex float (exp notation; lowercase/uppercase)
//   b              | Binary integer
//   c              | Single character
//   d / i          | Decimal integer
//   e / E          | Decimal float (exp notation)
//   f / F          | Decimal float
//   g              | `e` or `f` conversion (whichever is shorter)
//   G              | `E` or `F` conversion (whichever is shorter)
//   m              | Multichar string (reverse of multichar literals)
//   o              | Octal integer
//   s              | String
//   x / X          | Hex integer (lowercase/uppercase)
int fmt_snprintf(char *buf, size_t bufsz, const char *fstr, riff_val *argv, int argc) {
    int arg = 0;
    int n   = 0;

    while (*fstr && argc && n <= bufsz) {
        if (*fstr != '%') {
            buf[n++] = *fstr++;
            continue;
        }

        // Advance pointer and check for literal '%'
        if (*++fstr == '%') {
            buf[n++] = '%';
            ++fstr;
            continue;
        }

        // Flags and specifiers
        uint32_t flags = 0;
        int width = -1;

        // Both clang and gcc seem to allow -1 to be used as a
        // precision modifier without throwing warnings, so this is a
        // useful default
        int prec = -1;

capture_flags:
        switch (*fstr) {
        case '0': flags |= FMT_ZERO;  ++fstr; goto capture_flags;
        case '+': flags |= FMT_SIGN;  ++fstr; goto capture_flags;
        case ' ': flags |= FMT_SPACE; ++fstr; goto capture_flags;
        case '-': flags |= FMT_LEFT;  ++fstr; goto capture_flags;
        default:  break;
        }

        // Evaluate field width
        if (isdigit(*fstr)) {
            char *end;
            width = (int) strtol(fstr, &end, 10);
            fstr = end;
        } else if (*fstr == '*') {
            ++fstr;
            if (argc--) {
                width = (int) intval(argv+arg);
                ++arg;
            }
        }

        // Evaluate precision modifier
        if (*fstr == '.') {
            ++fstr;
            if (isdigit(*fstr)) {
                char *end;
                prec = (int) strtol(fstr, &end, 10);
                fstr = end;
            } else if (*fstr == '*') {
                ++fstr;
                if (argc--) {
                    prec = (int) intval(argv+arg);
                    ++arg;
                }
            } else {
                prec = 0;
            }
        }

        riff_int i;
        riff_float f;

        // Evaluate format specifier
        switch (*fstr++) {
        case 'c': {
            if (argc--) {
                uint32_t c = (uint32_t) intval(argv+arg);
                if (c <= 0x7f) {
                    fmt_char(buf, n, c);
                } else {
                    int n1 = 0;
                    char ubuf[8];
                    char tbuf[9];
                    int j = 8 - u_unicode2utf8(ubuf, c);
                    for (; j < 8; ++j) {
                        tbuf[n1++] = ubuf[j];
                    }
                    tbuf[n1] = '\0';
                    fmt_str(buf, n, tbuf);
                }
                ++arg;
            }
            break;
        }
        case 'm': {
            if (argc--) {
                riff_uint m = (riff_uint) intval(argv+arg);
                if (!m) {
                    ++arg;
                    break;
                }
                char mbuf[9];
                int k = 7;
                for (int j = 7; j >= 0; --j) {
                    mbuf[j] = (char) m & 0xff;
                    m >>= 8;
                    k = mbuf[j] ? j : k;
                }
                int n1 = 0;
                for (int j = 0; k < 8; ++j, ++k) {
                    mbuf[j] = mbuf[k];
                    n1 = j + 1;
                }
                mbuf[n1] = '\0';
                fmt_str(buf, n, mbuf);
                ++arg;
            }
            break;
        }
        case 'd': case 'i': {
            if (argc--) {
redir_int:
                i = intval(argv+arg);
                fmt_signed(buf, n, i, PRId64);
                ++arg;
            }
            break;
        }
        case 'o':
            if (argc--) {
                i = intval(argv+arg);
                fmt_unsigned(buf, n, i, PRIo64);
                ++arg;
            }
            break;
        case 'x':
            if (argc--) {
                i = intval(argv+arg);
                fmt_unsigned(buf, n, i, PRIx64);
                ++arg;
            }
            break;
        case 'X':
            if (argc--) {
                i = intval(argv+arg);
                fmt_unsigned(buf, n, i, PRIX64);
                ++arg;
            }
            break;
        case 'b':
            if (argc--) {
                n += fmt_bin_itoa(buf + n, intval(argv+arg), flags, width, prec);
                ++arg;
            }
            break;
        case 'a':
            if (argc--) {
                f = fltval(argv+arg);
                // Default precision left as -1 for `a`
                fmt_signed(buf, n, f, "a");
                ++arg;
            }
            break;
        case 'A':
            if (argc--) {
                f = fltval(argv+arg);
                // Default precision left as -1 for `A`
                fmt_signed(buf, n, f, "A");
                ++arg;
            }
            break;
        case 'e':
            if (argc--) {
                f = fltval(argv+arg);
                prec = prec < 0 ? DEFAULT_FLT_PREC : prec;
                fmt_signed(buf, n, f, "e");
                ++arg;
            }
            break;
        case 'E':
            if (argc--) {
                f = fltval(argv+arg);
                prec = prec < 0 ? DEFAULT_FLT_PREC : prec;
                fmt_signed(buf, n, f, "E");
                ++arg;
            }
            break;
        case 'f': case 'F':
            if (argc--) {
                f = fltval(argv+arg);
                prec = prec < 0 ? DEFAULT_FLT_PREC : prec;
                fmt_signed(buf, n, f, "f");
                ++arg;
            }
            break;
        case 'g':
            if (argc--) {
redir_flt:
                f = fltval(argv+arg);
                prec = prec < 0 ? DEFAULT_FLT_PREC : prec;
                fmt_signed(buf, n, f, "g");
                ++arg;
            }
            break;
        case 'G':
            if (argc--) {
                f = fltval(argv+arg);
                prec = prec < 0 ? DEFAULT_FLT_PREC : prec;
                fmt_signed(buf, n, f, "G");
                ++arg;
            }
            break;

        // %s should accept any type; redirect as needed
        case 's':
            if (argc--) {
                if (is_str(argv+arg)) {
                    fmt_str(buf, n, argv[arg].s->str);
                } else if (is_int(argv+arg)) {
                    goto redir_int;
                } else if (is_flt(argv+arg)) {
                    goto redir_flt;
                }

                // TODO handle other types
                else {
                    fmt_str(buf, n, "");
                }
                ++arg;
            }
            break;
        default:
            // Throw error
            err("invalid format specifier");
        }

        if (n >= bufsz)
            err("string length exceeds maximum buffer size");
    }

    // Copy rest of string after exhausting user-provided args
    while (*fstr && n <= bufsz) {
        buf[n++] = *fstr++;
    }

    return n;
}
