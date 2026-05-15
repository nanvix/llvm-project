// RUN: %clang -### %s --target=i686-unknown-nanvix \
// RUN:     -resource-dir=%S/Inputs/resource_dir_with_per_target_subdir \
// RUN:     --sysroot=%S/Inputs/basic_nanvix_tree 2>&1 \
// RUN:     | FileCheck -check-prefix=CHECK %s

// CHECK: "-cc1"
// CHECK: "-triple" "i686-unknown-nanvix"
// CHECK: {{.*}}ld.lld
// CHECK: "--sysroot=[[SYSROOT:[^"]+]]"
// CHECK: "--build-id=none"
// CHECK: "--no-rosegment"
// CHECK: "-z" "norelro"
// CHECK: "--eh-frame-hdr"
// CHECK: "--gc-sections"
// CHECK: "--no-dependent-libraries"
// CHECK: "-o"
// CHECK: "[[SYSROOT]]{{.*}}crt0.o"
// CHECK: "[[SYSROOT]]{{.*}}crti.o"
// CHECK: "[[SYSROOT]]{{.*}}crtbegin.o"
// CHECK: "--start-group"
// CHECK: "{{[^"]*}}libclang_rt.builtins.a"
// CHECK: "-lm"
// CHECK: "-lc"
// CHECK: "--end-group"
// CHECK: "[[SYSROOT]]{{.*}}crtend.o"
// CHECK: "[[SYSROOT]]{{.*}}crtn.o"

// RUN: %clang -### %s --target=i686-unknown-nanvix -nostdlib \
// RUN:     -resource-dir=%S/Inputs/resource_dir_with_per_target_subdir \
// RUN:     --sysroot=%S/Inputs/basic_nanvix_tree 2>&1 \
// RUN:     | FileCheck -check-prefix=CHECK-NOSTDLIB %s

// CHECK-NOSTDLIB: {{.*}}ld.lld
// CHECK-NOSTDLIB-NOT: "crt0.o"
// CHECK-NOSTDLIB-NOT: "--start-group"
// CHECK-NOSTDLIB-NOT: "-lc"
// CHECK-NOSTDLIB-NOT: "--end-group"
// CHECK-NOSTDLIB-NOT: "crtend.o"

// RUN: %clang -### %s --target=i686-unknown-nanvix -r \
// RUN:     -resource-dir=%S/Inputs/resource_dir_with_per_target_subdir \
// RUN:     --sysroot=%S/Inputs/basic_nanvix_tree 2>&1 \
// RUN:     | FileCheck -check-prefix=CHECK-RELOCATABLE %s

// CHECK-RELOCATABLE: {{.*}}ld.lld
// CHECK-RELOCATABLE: "-r"
// CHECK-RELOCATABLE-NOT: "--build-id=none"
// CHECK-RELOCATABLE-NOT: "crt0.o"
// CHECK-RELOCATABLE-NOT: "--start-group"
// CHECK-RELOCATABLE-NOT: "-lc"

// RUN: %clang -### %s --target=i686-unknown-nanvix -static \
// RUN:     -resource-dir=%S/Inputs/resource_dir_with_per_target_subdir \
// RUN:     --sysroot=%S/Inputs/basic_nanvix_tree 2>&1 \
// RUN:     | FileCheck -check-prefix=CHECK-STATIC %s

// CHECK-STATIC: {{.*}}ld.lld
// CHECK-STATIC: "-Bstatic"
