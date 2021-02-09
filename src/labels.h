// VM labels for computed goto

#define z_case(l)   L_##l:
#define z_break     dispatch()
#define dispatch()  goto *dispatch_labels[*ip]

static void *dispatch_labels[] = {
    &&L_JMP8,    // Jump (1-byte offset)
    &&L_JMP16,   // Jump (2-byte offset)
    &&L_JNZ8,    // Jump if non-zero (1-byte offset)
    &&L_JNZ16,   // Jump if non-zero (2-byte offset)
    &&L_JZ8,     // Jump if zero (1-byte offset)
    &&L_JZ16,    // Jump if zero (2-byte offset)
    &&L_XJNZ8,   // "Exclusive" jump if non-zero (1-byte offset)
    &&L_XJNZ16,  // "Exclusive" jump if non-zero (2-byte offset)
    &&L_XJZ8,    // "Exclusive" jump if zero (1-byte offset)
    &&L_XJZ16,   // "Exclusive" jump if zero (2-byte offset)
    &&L_LOOP8,   // Evaluate iterator and jump (1-byte offset)
    &&L_LOOP16,  // Evaluate iterator and jump (2-byte offset)
    &&L_POPL,    // Pop (destroy) current iterator
    &&L_ITERV,   // Initialize an iterator with only a value
    &&L_ITERKV,  // Initialize an iterator with key/value pair
    &&L_TEST,    // Logical test
    &&L_ADD,     // Add
    &&L_SUB,     // Substract
    &&L_MUL,     // Multiply
    &&L_DIV,     // Divide
    &&L_MOD,     // Modulus
    &&L_POW,     // Exponentiation
    &&L_AND,     // Bitwise AND
    &&L_OR,      // Bitwise OR
    &&L_XOR,     // Bitwise XOR
    &&L_SHL,     // Bitwise shift left
    &&L_SHR,     // Bitwise shift right
    &&L_NUM,     // Numeric coercion
    &&L_NEG,     // Negate
    &&L_NOT,     // Bitwise NOT
    &&L_EQ,      // Equality
    &&L_NE,      // Not equal
    &&L_GT,      // Greater-than
    &&L_GE,      // Greater-than or equal-to
    &&L_LT,      // Less-than
    &&L_LE,      // Less-than or equal-to
    &&L_LNOT,    // Logical NOT
    &&L_CAT,     // Concatenate
    &&L_PREINC,  // Pre-increment
    &&L_PREDEC,  // Pre-decrement
    &&L_POSTINC, // Post-increment
    &&L_POSTDEC, // Post-decrement
    &&L_LEN,     // Length
    &&L_ADDX,    // Add assign
    &&L_SUBX,    // Subtract assign
    &&L_MULX,    // Multiply assign
    &&L_DIVX,    // Divide assign
    &&L_MODX,    // Modulus assign
    &&L_CATX,    // Concatenate assign
    &&L_POWX,    // Exponentiation assign
    &&L_ANDX,    // Bitwise AND assign
    &&L_ORX,     // Bitwise OR assign
    &&L_SHLX,    // Bitwise shift left assign
    &&L_SHRX,    // Bitwise shift right assign
    &&L_XORX,    // Bitwise XOR assign
    &&L_POP,     // Pop (--SP)
    &&L_POPI,    // Pop IP+1 values from stack (SP -= (IP+1))
    &&L_NULL,    // Push null value on stack
    &&L_IMM8,    // Push (IP+1) as literal value on stack
    &&L_IMM16,   // Push ((IP+1)<<8) + (IP+2) as literal on stack
    &&L_IMM0,    // Push literal `0` on stack
    &&L_IMM1,    // Push literal `1` on stack
    &&L_IMM2,    // Push literal `2` on stack
    &&L_PUSHK,   // Push K[IP+1] on stack as value
    &&L_PUSHK0,  // Push K[0] on stack as value
    &&L_PUSHK1,  // Push K[1] on stack as value
    &&L_PUSHK2,  // Push K[2] on stack as value
    &&L_GBLA,    // Push address of global var K[IP+1] on stack
    &&L_GBLA0,   // Push address of global var K[0] on stack
    &&L_GBLA1,   // Push address of global var K[1] on stack
    &&L_GBLA2,   // Push address of global var K[2] on stack
    &&L_GBLV,    // Copy value of global var K[IP+1] onto stack
    &&L_GBLV0,   // Copy value of global var K[0] onto stack
    &&L_GBLV1,   // Copy value of global var K[1] onto stack
    &&L_GBLV2,   // Copy value of global var K[2] onto stack
    &&L_LCLA,    // Push address of stack[FP+IP+1] on stack
    &&L_LCLA0,   // Push address of stack[FP+0] on stack
    &&L_LCLA1,   // Push address of stack[FP+1] on stack
    &&L_LCLA2,   // Push address of stack[FP+2] on stack
    &&L_LCLV,    // Copy value of stack[FP+IP+1] onto stack
    &&L_LCLV0,   // Copy value of stack[FP+0] onto stack
    &&L_LCLV1,   // Copy value of stack[FP+1] onto stack
    &&L_LCLV2,   // Copy value of stack[FP+2] onto stack
    &&L_CALL,    // Function call
    &&L_RET,     // Return from call (void)
    &&L_RET1,    // Return from call 1 value
    &&L_ARRAY0,  // Create empty array
    &&L_ARRAY,   // Create array of the top (IP+1) stack elements
    &&L_ARRAYK,  // Create array of the top K[IP+1] stack elements
    &&L_IDXA,    // Index of a set, leaving address on stack (nD array)
    &&L_IDXA1,   // Index of a set, leaving address on stack
    &&L_IDXV,    // Index of a set, leaving value on stack (nD array)
    &&L_IDXV1,   // Index of a set, leaving value on stack
    &&L_ARGA,    // Index of the argv, leaving address on stack
    &&L_ARGV,    // Index of the argv, leaving value on stack
    &&L_SEQ,     // Sequence: SP[-2]..SP[-1]
    &&L_SEQF,    // Sequence: SP[-1]..INT_MAX
    &&L_SEQT,    // Sequence: 0..SP[-1]
    &&L_SEQE,    // Sequence: .. (empty/infinite)
    &&L_SSEQ,    // Sequence: SP[-3]..SP[-2] w/ interval SP[-1]
    &&L_SSEQF,   // Sequence: SP[-2]..INT_MAX w/ interval SP[-1]
    &&L_SSEQT,   // Sequence: 0..SP[-2] w/ interval SP[-1]
    &&L_SSEQE,   // Sequence: .. (empty/infinite) w/ interval SP[-1]
    &&L_SET,     // Assignment
    &&L_PRINT1,  // Print value at SP[-1]
    &&L_PRINT,   // Print (IP+1) values from stack
    &&L_EXIT     // exit(0)
};
