#ifndef VM_OPS_H
#define VM_OPS_H

#include "conf.h"
#include "string.h"
#include "value.h"

#include <math.h>

// Integer arithmetic (Bitwise ops)
#define INT_ARITH(l,r,op)                       \
    do {                                        \
        set_int(l, (intval(l) op intval(r)));   \
    } while (0)

// Floating-point arithmetic (div)
#define FLT_ARITH(l,r,op)                       \
    do {                                        \
        set_flt(l, (fltval(l) op fltval(r)));   \
    } while (0)

// "Polymorphic" arithmetic (add, sub, mul)
#define NUM_ARITH(l,r,op)                       \
    do {                                        \
        if (is_int(l) && is_int(r))             \
            l->i = (l->i op r->i);              \
        else                                    \
            FLT_ARITH(l,r,op);                  \
    } while (0)

// Return boolean result of value (0/1)
static inline int vm_test(riff_val *v) {
    switch (v->type) {
    case TYPE_INT: return !!(v->i);
    case TYPE_FLOAT: return !!(v->f);

    // If entire string is a numeric value, return logical result of the number.
    // Otherwise, return whether the string is longer than 0.
    case TYPE_STR: {
        char *end;
        riff_float f = riff_strtod(v->s->str, &end, 0);
        if (!*end) {
            // Check for literal '0' character in string
            return (f == 0.0 && riff_str_haszero(v->s)) ? 0 : !!f;
        }
        return !!riff_strlen(v->s);
    }
    case TYPE_TAB: return !!riff_tab_logical_size(v->t);
    case TYPE_REGEX:
    case TYPE_RANGE:
    case TYPE_RFN:
    case TYPE_CFN:
        return 1;
    default: return 0;
    }
}

#define VM_UOP(op)   static inline void vm_##op(riff_val *v)
#define VM_BINOP(op) static inline void vm_##op(riff_val *l, riff_val *r)

VM_BINOP(add) { NUM_ARITH(l,r,+); }
VM_BINOP(sub) { NUM_ARITH(l,r,-); }
VM_BINOP(mul) { NUM_ARITH(l,r,*); }

// Language comparison for division by zero:
// 0/0 = nan; 1/0 = inf : lua, mawk
// error: pretty much all others
VM_BINOP(div) { FLT_ARITH(l,r,/); }

// Language comparison for modulus by zero:
// `nan`: mawk
// error: pretty much all others
VM_BINOP(mod) {
    riff_float res = fmod(numval(l), numval(r));
    set_flt(l, res < 0 ? res + numval(r) : res);
}

VM_BINOP(pow) { set_flt(l, pow(fltval(l), fltval(r))); }

VM_BINOP(and) { INT_ARITH(l,r,&);  }
VM_BINOP(or)  { INT_ARITH(l,r,|);  }
VM_BINOP(xor) { INT_ARITH(l,r,^);  }
VM_BINOP(shl) { INT_ARITH(l,r,<<); }
VM_BINOP(shr) { INT_ARITH(l,r,>>); }

VM_UOP(num) {
    switch (v->type) {
    case TYPE_INT:
    case TYPE_FLOAT:
        break;
    case TYPE_STR:
        set_flt(v, str2flt(v->s));
        break;
    default:
        set_int(v, 0);
        break;
    }
}

VM_UOP(neg) {
    switch (v->type) {
    case TYPE_INT:   v->i = -v->i;               break;
    case TYPE_FLOAT: v->f = -v->f;               break;
    case TYPE_STR:   set_flt(v, -str2flt(v->s)); break;
    default:         set_int(v, 0);              break;
    }
}

VM_UOP(not) { set_int(v, ~intval(v)); }

// == and != operators
#define CMP_EQ(l,r,op)                                      \
    do {                                                    \
        if (is_int(l) && is_int(r)) {                       \
            l->i = (l->i op r->i);                          \
        } else if (is_null(l) ^ is_null(r)) {               \
            set_int(l, !(0 op 0));                          \
        } else if (is_str(l) && is_str(r)) {                \
            set_int(l, ((l->s) op (r->s)));                 \
        } else if (is_str(l) && !is_str(r)) {               \
            if (!riff_strlen(l->s)) {                       \
                set_int(l, !(0 op 0));                      \
                return;                                     \
            }                                               \
            char *end;                                      \
            riff_float f = riff_strtod(l->s->str, &end, 0); \
            if (*end)                                       \
                set_int(l, 0);                              \
            else                                            \
                set_int(l, (f op numval(r)));               \
        } else if (!is_str(l) && is_str(r)) {               \
            if (!riff_strlen(r->s)) {                       \
                set_int(l, !(0 op 0));                      \
                return;                                     \
            }                                               \
            char *end;                                      \
            riff_float f = riff_strtod(r->s->str, &end, 0); \
            if (*end)                                       \
                set_int(l, 0);                              \
            else                                            \
                set_int(l, (numval(l) op f));               \
        } else {                                            \
            NUM_ARITH(l,r,op);                              \
        }                                                   \
    } while (0)

// >, <, >= and <= operators
#define CMP_REL(l,r,op)                                     \
    do {                                                    \
        if (is_int(l) && is_int(r))                         \
            l->i = (l->i op r->i);                          \
        else                                                \
            set_int(l, (numval(l) op numval(r)));           \
    } while (0)

VM_BINOP(eq) { CMP_EQ(l,r,==);  }
VM_BINOP(ne) { CMP_EQ(l,r,!=);  }
VM_BINOP(gt) { CMP_REL(l,r,>);  }
VM_BINOP(ge) { CMP_REL(l,r,>=); }
VM_BINOP(lt) { CMP_REL(l,r,<);  }
VM_BINOP(le) { CMP_REL(l,r,<=); }

VM_UOP(lnot) { set_int(v, !vm_test(v)); }

VM_UOP(len) {
    riff_int l = 0;
    switch (v->type) {

    // For integers:
    //   #x = ⌊log10(x)⌋  + 1 for x > 0
    //        ⌊log10(-x)⌋ + 2 for x < 0
    case TYPE_INT:
        if (v->i == INT64_MIN) {
            l = 20;
        } else {
            l = v->i > 0 ? (riff_int) log10(v->i)  + 1 :
                v->i < 0 ? (riff_int) log10(-v->i) + 2 : 1;
        }
        v->i = l;
        return;
    case TYPE_FLOAT:
        l = (riff_int) snprintf(NULL, 0, "%g", v->f);
        break;
    case TYPE_STR: l = riff_strlen(v->s); break;
    case TYPE_TAB: l = riff_tab_logical_size(v->t); break;
    case TYPE_REGEX: // TODO - extract something from PCRE pattern?
    case TYPE_RANGE: // TODO
    case TYPE_RFN:
    case TYPE_CFN:
        l = 1;
        break;
    default: break;
    }
    set_int(v, l);
}

VM_BINOP(cat) {
    char lbuf[STR_BUF_SZ];
    char rbuf[STR_BUF_SZ];
    char *ls = lbuf, *rs = rbuf;
    size_t llen = riff_tostr(l, &ls);
    size_t rlen = riff_tostr(r, &rs);
    set_str(l, riff_strcat(ls, rs, llen, rlen));
}

static inline void vm_catn(vm_stack *fp, int n) {
    char buf[STR_BUF_SZ];
    char tbuf[STR_BUF_SZ];
    size_t len = 0;
    for (int i = -n; i <= -1; ++i) {
        char *p = tbuf;
        size_t tlen = riff_tostr(&fp[i].v, &p);
        memcpy(buf + len, p, tlen);
        len += tlen;
    }
    set_str(&fp[-n].v, riff_str_new(buf, len));
}

static inline riff_int match(riff_val *l, riff_val *r) {
    // Common case: LHS string, RHS regex
    if (riff_likely(is_str(l) && is_regex(r)))
        return re_match(l->s->str, riff_strlen(l->s), r->r, 1);
    char *lhs;
    size_t len = 0;
    char temp_lhs[32];
    char temp_rhs[32];
    if (!is_str(l)) {
        switch (l->type) {
        case TYPE_INT:   len = riff_lltostr(l->i, temp_lhs); break;
        case TYPE_FLOAT: len = riff_dtostr(l->f, temp_lhs);  break;
        default:         temp_lhs[0] = '\0';                 break;
        }
        lhs = temp_lhs;
    } else {
        lhs = l->s->str;
        len = riff_strlen(l->s);
    }
    if (!is_regex(r)) {
        riff_regex *temp_re;
        riff_int res;
        int errcode;
        int capture = 0;
        switch (r->type) {
        case TYPE_INT:   riff_lltostr(r->i, temp_rhs); break;
        case TYPE_FLOAT: riff_dtostr(r->f, temp_rhs);  break;
        case TYPE_STR:
            capture = 1;
            temp_re = re_compile(r->s->str, riff_strlen(r->s), 0, &errcode);
            goto do_match;
        default: temp_rhs[0] = '\0'; break;
        }
        temp_re = re_compile(temp_rhs, PCRE2_ZERO_TERMINATED, 0, &errcode);
do_match:
        // Check for invalid regex in RHS
        // TODO treat as literal string in this case? (PCRE2_LITERAL)
        if (riff_unlikely(errcode != 100)) {
            PCRE2_UCHAR errstr[0x200];
            pcre2_get_error_message(errcode, errstr, 0x200);
            err((const char *) errstr);
        }
        res = re_match(lhs, len, temp_re, capture);
        re_free(temp_re);
        return res;
    } else {
        return re_match(lhs, len, r->r, 1);
    }
}

VM_BINOP(match)  { set_int(l,  match(l, r)); }
VM_BINOP(nmatch) { set_int(l, !match(l, r)); }

VM_BINOP(idx) {
    switch (l->type) {
    case TYPE_INT:
    case TYPE_FLOAT: {
        char temp[32];
        char *p = temp;
        riff_int len = (riff_int) riff_tostr(l, &p);
        if (is_range(r)) {
            set_str(l, riff_substr(temp, (size_t) len, r->q->from, r->q->to, r->q->itvl));
        } else {
            riff_int r1  = intval(r);
            if (r1 < 0)
                r1 += len;
            if (r1 > len - 1 || r1 < 0)
                set_null(l);
            else
                set_str(l, riff_str_new(temp + r1, 1));
        }
        break;
    }
    case TYPE_STR: {
        if (is_range(r)) {
            l->s = riff_substr(l->s->str, riff_strlen(l->s), r->q->from, r->q->to, r->q->itvl);
        } else {
            riff_int r1  = intval(r);
            riff_int len = (riff_int) riff_strlen(l->s);
            if (r1 < 0)
                r1 += len;
            if (r1 > len - 1 || r1 < 0)
                set_null(l);
            else
                l->s = riff_str_new(&l->s->str[r1], 1);
        }
        break;
    }
    case TYPE_NULL:
        *l = *v_newtab(0);
        // Fall-through
    case TYPE_TAB:
        *l = *riff_tab_lookup(l->t, r);
        break;
    default:
        set_null(l);
        break;
    }
}

#endif
