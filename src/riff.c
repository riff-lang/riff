#include <stdio.h>

#include "code.h"
#include "disas.h"

int main(int argc, char **argv) {
    codeblock_t block;
    block_init(&block);
    block_append(&block, OP_RETURN);
    disas_block(&block, "test");
    return 0;
}
