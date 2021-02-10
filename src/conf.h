#ifndef CONF_H
#define CONF_H

// Size of buffer used in l_char() and l_fmt()
#define STR_BUF_SZ 0x1000

// Default precision for floating point numbers in fmt()
#define DEFAULT_FLT_PREC 6

// Size of VM stack
// Currently statically allocated
#define VM_STACK_SIZE 0x1000

#endif
