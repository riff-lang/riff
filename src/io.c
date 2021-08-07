#include "types.h"

#include <stdio.h>

rf_fh *io_fopen(char *filename, char *mode) {
    FILE *fp = fopen(filename, mode);
    if (!fp) {
        perror("riff: open() failure");
        exit(1);
    }
    rf_fh *fh = malloc(sizeof(rf_fh));
    fh->p = fp;
    fh->flags = 0;

    return fh;
}
