// Copyright(c) The Maintainers of Nanvix.
// Licensed under the MIT License.

// Smoke test: verify that a C program can compile and link against libc.a for the
// i686-unknown-nanvix target.

#include <stdio.h>

int main(void) {
    printf("value = 42\n");
    return 0;
}
