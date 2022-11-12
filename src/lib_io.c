#include "lib.h"

#include "buf.h"
#include "conf.h"
#include "fmt.h"
#include "parse.h"
#include "state.h"
#include "string.h"
#include "vm.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>

static void err(const char *msg) {
    fprintf(stderr, "riff: %s\n", msg);
    exit(1);
}

// I/O functions

// close(f)
LIB_FN(close) {
    if (is_file(fp))
        if (!(fp->fh->flags & FH_STD))
            fclose(fp->fh->p);
    return 0;
}

// eof(f)
LIB_FN(eof) {
    set_int(fp-1, !is_file(fp) || feof(fp->fh->p));
    return 1;
}

// eval(s)
LIB_FN(eval) {
    if (!is_str(fp)) {
        return 0;
    }
    riff_state s;
    riff_fn main;
    riff_code c;

    riff_state_init(&s);
    c_init(&c);
    main.code = &c;
    main.arity = 0;
    s.main = main;
    s.src = fp->s->str;
    main.name = NULL;
    riff_compile(&s);
    vm_exec_reenter(&s, (vm_stack *) fp);
    return 0;
}

// flush([f])
LIB_FN(flush) {
    FILE *f = argc && is_file(fp) ? fp->fh->p : stdout;
    if (fflush(f)) {
        err("error flushing stream");
    }
    return 0;
}

// get([n])
LIB_FN(get) {
    char buf[STR_BUF_SZ];
    if (argc) {
        size_t count = intval(fp);
        size_t nread = fread(buf, sizeof *buf, count, stdin);
        set_str(fp-1, riff_str_new(buf, nread));
        return 1;
    }
    if (!fgets(buf, sizeof buf, stdin)) {
        return 0;
    }
    set_str(fp-1, riff_str_new(buf, strcspn(buf, "\n")));
    return 1;
}

// getc([f])
LIB_FN(getc) {
    riff_int c;
    FILE *f = argc && is_file(fp) ? fp->fh->p : stdin;
    if ((c = fgetc(f)) != EOF) {
        set_int(fp-1, c);
        return 1;
    }
    return 0;
}


static int valid_fmode(char *mode) {
    return (*mode && memchr("rwa", *(mode++), 3) &&
           (*mode != '+' || ((void)(++mode), 1)) &&
           (strspn(mode, "b") == strlen(mode)));
}

// open(s[,m])
LIB_FN(open) {
    if (!is_str(fp))
        return 0;
    FILE *p;
    errno = 0;
    if (argc == 1 || !is_str(fp+1)) {
        p = fopen(fp[0].s->str, "r");
    } else {
        if (!valid_fmode(fp[1].s->str)) {
            fprintf(stderr, "riff: error opening '%s': invalid file mode: '%s'\n",
                    fp[0].s->str, fp[1].s->str);
            exit(1);
        }
        p = fopen(fp[0].s->str, fp[1].s->str);
    }
    if (!p) {
        fprintf(stderr, "riff: error opening '%s': %s\n",
                fp[0].s->str, strerror(errno));
        exit(1);
    }
    riff_file *fh = malloc(sizeof(riff_file));
    fh->p = p;
    fh->flags = 0;
    fp[-1] = (riff_val) {TYPE_FILE, .fh = fh};
    return 1;
}

LIB_FN(write);

// printf(s, ...)
LIB_FN(printf) {
    if (!is_str(fp)) {
        return l_write(fp, argc);
    }
    --argc;
    char buf[STR_BUF_SZ];
    int n = fmt_snprintf(buf, sizeof buf, fp->s->str, fp + 1, argc);
    buf[n] = '\0';
    fputs(buf, stdout);
    return 0;
}

// putc(...)
// Takes zero or more integers and prints a string composed of the
// character codes of each respective argument in order
// Ex:
//   putc(114, 105, 102, 102) -> "riff"
LIB_FN(putc) {
    if (UNLIKELY(!argc))
        return 0;
    char buf[STR_BUF_SZ];
    int n = build_char_str(fp, argc, buf);
    buf[n] = '\0';
    fputs(buf, stdout);
    set_int(fp-1, n);
    return 1;
}

#define READ_BUF_SZ 0x10000

static inline int read_bytes(FILE *f, riff_int n, riff_str **ret) {
    if (n) {
        riff_buf buf;
        riff_buf_init_size(&buf, n);
        size_t nr = fread(buf.buf, sizeof (char), n, f);
        *ret = riff_str_new(buf.buf, nr);
        riff_buf_free(&buf);
        return nr > 0;
    } else {
        int c = getc(f);
        ungetc(c, f);
        return -(c != EOF);
    }
}

static inline int read_line(FILE *f, riff_str **ret) {
    riff_buf buf;
    size_t m = READ_BUF_SZ;
    riff_buf_init_size(&buf, m);
    while (1) {
        riff_buf_resize(&buf, m);
        if (fgets(buf.buf + buf.n, m - buf.n, f) == NULL) {
            break;
        }
        buf.n += (size_t) strlen(buf.buf + buf.n);
        if (buf.n && buf.buf[buf.n-1] == '\n') {
            --buf.n;
            break;
        }
        if (buf.n >= m - 64) {
            m += m;
        }
    }
    *ret = riff_str_new(buf.buf, buf.n);
    riff_buf_free(&buf);
    return 1;
}

static inline int read_all(FILE *f, riff_str **ret) {
    riff_buf buf;
    size_t m = READ_BUF_SZ;
    riff_buf_init_size(&buf, m);
    do {
        riff_buf_resize(&buf, m);
        buf.n += fread(buf.buf + buf.n, sizeof (char), m - buf.n, f);
        m += m;
    } while (buf.n == m);
    *ret = riff_str_new(buf.buf, buf.n);
    riff_buf_free(&buf);
    return 1;
}

static inline int read_file_mode(FILE *f, char *mode, riff_str **ret) {
    switch (*mode) {
    case 'A':
    case 'a':
        return read_all(f, ret);
    case 'L':
    case 'l':
    default:
        return read_line(f, ret);
    }
}

// read([a[,b]])
LIB_FN(read) {
    riff_str *ret = NULL;
    int res = 0;
    if (!argc) {
        res = read_line(stdin, &ret);
    } else if (!is_file(fp)) {
        if (is_str(fp)) {
            res = read_file_mode(stdin, fp->s->str, &ret);
        } else {
            res = read_bytes(stdin, intval(fp), &ret);
        }
    } else if (is_file(fp)) {
        if (argc == 1) {
            res = read_line(fp->fh->p, &ret);
        } else {
            if (is_str(fp+1)) {
                res = read_file_mode(fp->fh->p, fp[1].s->str, &ret);
            } else {
                res = read_bytes(fp->fh->p, intval(fp+1), &ret);
            }
        }
    }

    if (LIKELY(res > 0)) {
        set_str(fp-1, ret);
    } else {
        set_int(fp-1, !!res);
    }
    return 1;
}

// write(v[,f])
LIB_FN(write) {
    if (UNLIKELY(!argc)) {
        return 0;
    }
    FILE *f = argc > 1 && is_file(fp+1) ? fp[1].fh->p : stdout;
    fputs_val(f, fp);
    return 0;
}

static riff_lib_fn_reg iolib[] = {
    LIB_FN_REG(close,  1),
    LIB_FN_REG(eof,    0),
    LIB_FN_REG(eval,   1),
    LIB_FN_REG(flush,  0),
    LIB_FN_REG(get,    0),
    LIB_FN_REG(getc,   0),
    LIB_FN_REG(open,   1),
    LIB_FN_REG(printf, 1),
    LIB_FN_REG(putc,   0),
    LIB_FN_REG(read,   0),
    LIB_FN_REG(write,  0),
};

// NOTE: Standard streams can't be cleanly declared in a static struct like the
// lib functions since the names (e.g. stdin) aren't compile-time constants
#define REGISTER_LIB_STREAM(name) \
    riff_file *name##_fh = malloc(sizeof(riff_file)); \
    *name##_fh = (riff_file) {name, FH_STD}; \
    riff_htab_insert_cstr(g, #name, &(riff_val){TYPE_FILE, .fh = name##_fh});

static void register_streams(riff_htab *g) {
    REGISTER_LIB_STREAM(stdin);
    REGISTER_LIB_STREAM(stdout);
    REGISTER_LIB_STREAM(stderr);
}

void riff_lib_register_io(riff_htab *g) {
    FOREACH(iolib, i) {
        riff_htab_insert_cstr(g, iolib[i].name, &(riff_val) {TYPE_CFN, .cfn = &iolib[i].fn});
    }
    register_streams(g);
}
