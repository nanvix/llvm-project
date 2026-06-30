// Driver tests for the Nanvix toolchain (i686-unknown-nanvix).
//
// The Nanvix toolchain drives ld.lld directly (instead of going through a gcc
// wrapper) and emits a fixed set of flags required by the Nanvix ELF loader and
// in-source C library. These tests pin that link line.

// Default executable link (C): ld.lld is invoked directly with the Nanvix
// loader/runtime flags, the single in-source startfile (crt0.o), and the
// archive-grouped default libraries.
// RUN: %clang -### --target=i686-unknown-nanvix \
// RUN:     --sysroot=%t %s 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-LD %s
// CHECK-LD: "{{[^"]*}}ld.lld"
// CHECK-LD-SAME: "--sysroot={{[^"]+}}"
// CHECK-LD-SAME: "--build-id=none"
// CHECK-LD-SAME: "--no-rosegment"
// CHECK-LD-SAME: "-z" "norelro"
// CHECK-LD-SAME: "-Bstatic"
// CHECK-LD-SAME: "--eh-frame-hdr"
// CHECK-LD-SAME: "--gc-sections"
// CHECK-LD-SAME: "--no-dependent-libraries"
// CHECK-LD-SAME: "{{[^"]*}}crt0.o"
// CHECK-LD-SAME: "--start-group"
// CHECK-LD-SAME: "-lm" "-lc"
// CHECK-LD-SAME: "--end-group"

// C++ link: the C++ runtime (libc++/libc++abi/libunwind) is added inside the
// archive group, ahead of -lm/-lc.
// RUN: %clangxx -### --target=i686-unknown-nanvix \
// RUN:     --sysroot=%t %s 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-LDXX %s
// CHECK-LDXX: "{{[^"]*}}ld.lld"
// CHECK-LDXX-SAME: "{{[^"]*}}crt0.o"
// CHECK-LDXX-SAME: "--start-group"
// CHECK-LDXX-SAME: "-lc++" "-lc++abi" "-lunwind"
// CHECK-LDXX-SAME: "-lm" "-lc"
// CHECK-LDXX-SAME: "--end-group"

// Shared object: emit a shared library with text relocations allowed and no
// ELF interpreter, and without the crt0 startfile.
// RUN: %clang -### --target=i686-unknown-nanvix -shared \
// RUN:     --sysroot=%t %s 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-SHARED %s
// CHECK-SHARED: "{{[^"]*}}ld.lld"
// CHECK-SHARED-SAME: "-shared"
// CHECK-SHARED-SAME: "-z" "notext"
// CHECK-SHARED-SAME: "--no-dynamic-linker"
// CHECK-SHARED-NOT: crt0.o

// Relocatable link (-r): forward -r and link nothing else (no crt0, no default
// libraries, no archive group).
// RUN: %clang -### --target=i686-unknown-nanvix -r \
// RUN:     --sysroot=%t %s 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-RELOC %s
// CHECK-RELOC: "{{[^"]*}}ld.lld"
// CHECK-RELOC-SAME: "-r"
// CHECK-RELOC-NOT: crt0.o
// CHECK-RELOC-NOT: "--start-group"

int main(void) { return 0; }
