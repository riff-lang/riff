// Opcodes
//
// Definition:
//   OPCODE(<macro suffix>, <arity>, <disas mnemonic>)
//
// Changing the order of ops here should have minimal impact, but
// it's ideal to match/adjust the order of VM ops accordingly.
//
// Use preprocessor magic to include and manipulate info as needed
// Ex: create a jump table for computed goto:
//   #define OPCODE(x,y,z) &&L_##x
//   static void *labels[] = {
//   #include "opcodes.h"
//   };
//   #undef OPCODE

// Lazy macro - stringize macro suffix as the mnemonic
#define OP_STR(x,y) OPCODE(x,y,#x)

// Simple jumps
OPCODE(JMP8,    1, "JMP"),
OPCODE(JMP16,   2, "JMP"),

// Condiitonal jumps
OPCODE(JNZ8,    1, "JNZ"),
OPCODE(JNZ16,   2, "JNZ"),
OPCODE(JZ8,     1, "JZ"),
OPCODE(JZ16,    2, "JZ"),

// "Exclusive" conditional jumps (pop stack only if jump not taken)
OPCODE(XJNZ8,   1, "XJNZ"),
OPCODE(XJNZ16,  2, "XJNZ"),
OPCODE(XJZ8,    1, "XJZ"),
OPCODE(XJZ16,   2, "XJZ"),

// Evaluate current iterator and jump
OPCODE(LOOP8,   1, "LOOP"),
OPCODE(LOOP16,  2, "LOOP"),

// Pop (destroy) current iterator
OP_STR(POPL,    0),

// Iterator intializers (either key/value pair or just value)
OP_STR(ITERV,   2),
OP_STR(ITERKV,  2),

// Prefix ops
OP_STR(LEN,     0),
OP_STR(LNOT,    0),
OP_STR(NEG,     0),
OP_STR(NOT,     0),
OP_STR(NUM,     0),

// Infix arithmetic
OP_STR(ADD,     0),
OP_STR(SUB,     0),
OP_STR(MUL,     0),
OP_STR(DIV,     0),
OP_STR(MOD,     0),
OP_STR(POW,     0),
OP_STR(AND,     0),
OP_STR(OR,      0),
OP_STR(XOR,     0),
OP_STR(SHL,     0),
OP_STR(SHR,     0),

// Equality/comparison
OP_STR(EQ,      0),
OP_STR(NE,      0),
OP_STR(GT,      0),
OP_STR(GE,      0),
OP_STR(LT,      0),
OP_STR(LE,      0),
OP_STR(CAT,     0),
OP_STR(MATCH,   0),
OP_STR(NMATCH,  0),

OP_STR(VIDXV,   0),

// Prefix/postfix increment/decrement
OP_STR(PREINC,  0),
OP_STR(PREDEC,  0),
OP_STR(POSTINC, 0),
OP_STR(POSTDEC, 0),

// Compound assignment
OP_STR(ADDX,    0),
OP_STR(SUBX,    0),
OP_STR(MULX,    0),
OP_STR(DIVX,    0),
OP_STR(MODX,    0),
OP_STR(CATX,    0),
OP_STR(POWX,    0),
OP_STR(ANDX,    0),
OP_STR(ORX,     0),
OP_STR(SHLX,    0),
OP_STR(SHRX,    0),
OP_STR(XORX,    0),

// Pop stack
OP_STR(POP,     0), // --SP
OPCODE(POPI,    1, "POP"), // SP -= (IP+1)

// Push `null` on the stack
OPCODE(NUL,     0, "NULL"),

// Push immediate value on the stack
OPCODE(IMM8,    1, "IMM"), // IP+1
OPCODE(IMM16,   2, "IMM"), // ((IP+1) << 8) + (IP+2)
OPCODE(IMM0,    0, "IMM     0"), // Literal `0`
OPCODE(IMM1,    0, "IMM     1"), // Literal `1`
OPCODE(IMM2,    0, "IMM     2"), // Literal `2`

// Push constant from constants table on stack as a value
OP_STR(CONST,   1), // K[IP+1]
OPCODE(CONST0,  0, "CONST   0"), // K[0]
OPCODE(CONST1,  0, "CONST   1"), // K[1]
OPCODE(CONST2,  0, "CONST   2"), // K[2]

// Push address of a global var on the stack
OP_STR(GBLA,    1), // K[IP+1]
OPCODE(GBLA0,   0, "GBLA    0"), // K[0]
OPCODE(GBLA1,   0, "GBLA    1"), // K[1]
OPCODE(GBLA2,   0, "GBLA    2"), // K[2]

// Copy value of global var onto stack
OP_STR(GBLV,    1), // K[IP+1]
OPCODE(GBLV0,   0, "GBLV    0"), // K[0]
OPCODE(GBLV1,   0, "GBLV    1"), // K[1]
OPCODE(GBLV2,   0, "GBLV    2"), // K[2]

// Push address of local var on the stack
OP_STR(LCLA,    1), // stack[FP+IP+1]
OPCODE(LCLA0,   0, "LCLA    0"), // stack[FP+0]
OPCODE(LCLA1,   0, "LCLA    1"), // stack[FP+1]
OPCODE(LCLA2,   0, "LCLA    2"), // stack[FP+2]

// Reserve and duplicate address of SP
OP_STR(DUPA,    0),

// Copy local value to top of stack
OP_STR(LCLV,    1), // stack[FP+IP+1]
OPCODE(LCLV0,   0, "LCLV    0"), // stack[FP+0]
OPCODE(LCLV1,   0, "LCLV    1"), // stack[FP+1]
OPCODE(LCLV2,   0, "LCLV    2"), // stack[FP+2]

// Function calls
OP_STR(TCALL,   1),
OP_STR(CALL,    1),

// Return
OP_STR(RET,     0), // Void return
OPCODE(RET1,    0, "RET"), // Return 1 value

// Table initialization
OPCODE(TAB0,    0, "TAB     0"), // Empty
OP_STR(TAB,     1), // IP+1 stack elements
OP_STR(TABK,    1), // K[IP+1] stack elements

// Index into multidimensional table e.g. t[x,y]
OP_STR(IDXA,    1),
OP_STR(IDXV,    1),

// Index into value
OPCODE(IDXA1,   0, "IDXA"),
OPCODE(IDXV1,   0, "IDXV"),
OP_STR(SIDXA,   1),
OP_STR(SIDXV,   1),

// Index into field table (fldv)
OP_STR(FLDA,    0),
OP_STR(FLDV,    0),

// Ranges
OP_STR(RNG,     0), // SP[-2]..SP[-1]
OP_STR(RNGF,    0), // SP[-1]..INT_MAX
OP_STR(RNGT,    0), // 0..SP[-1]
OP_STR(RNGI,    0), // .. (empty/infinite)
OP_STR(SRNG,    0), // SP[-3]..SP[-2] w/ interval SP[-1]
OP_STR(SRNGF,   0), // SP[-2]..INT_MAX w/ interval SP[-1]
OP_STR(SRNGT,   0), // 0..SP[-2] w/ interval SP[-1]
OP_STR(SRNGI,   0), // .. (empty/infinite) w/ interval SP[-1]

// Assignment
OP_STR(SET,     0),
OP_STR(SETP,    0),

#undef OP_STR
#undef OPCODE
