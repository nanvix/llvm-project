// Copyright(c) The Maintainers of Nanvix.
// Licensed under the MIT License.

// Smoke test: verify that a C++ program can compile and link against libc++,
// libc++abi, and libunwind for the i686-unknown-nanvix target.

#include <cstdio>

int main() {
    int *p = new int(42);
    std::printf("value = %d\n", *p);
    delete p;
    return 0;
}
