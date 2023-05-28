#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <float.h>
#include <time.h>

#include "mm.h"
#include "memlib.h"
#include "config.h"

int main() {
    mm_init();
    void *bp = mm_malloc(8);
    mm_free(bp);
    return 0;
}