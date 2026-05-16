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

// Verify the linker is NOT the gcc wrapper.
// RUN: %clang -### %s --target=i686-unknown-nanvix \
// RUN:     -resource-dir=%S/Inputs/resource_dir_with_per_target_subdir \
// RUN:     --sysroot=%S/Inputs/basic_nanvix_tree 2>&1 \
// RUN:     | FileCheck -check-prefix=CHECK-NO-GCC %s

// CHECK-NO-GCC-NOT: i686-unknown-nanvix-gcc
// CHECK-NO-GCC-NOT: i686-nanvix-gcc

// Verify the assembler is invoked directly (not via gcc).
// RUN: %clang -### %s --target=i686-unknown-nanvix -c -no-integrated-as \
// RUN:     -resource-dir=%S/Inputs/resource_dir_with_per_target_subdir \
// RUN:     --sysroot=%S/Inputs/basic_nanvix_tree 2>&1 \
// RUN:     | FileCheck -check-prefix=CHECK-AS %s

// CHECK-AS: "{{.*}}as"
// CHECK-AS-NOT: "{{.*}}gcc"

// Verify system include paths are resolved (resource-dir builtin headers).
// RUN: %clang -### %s --target=i686-unknown-nanvix -fsyntax-only \
// RUN:     -resource-dir=%S/Inputs/resource_dir_with_per_target_subdir \
// RUN:     --sysroot=%S/Inputs/basic_nanvix_tree 2>&1 \
// RUN:     | FileCheck -check-prefix=CHECK-INCLUDES %s

// CHECK-INCLUDES: "-internal-isystem" "{{.*}}/resource_dir_with_per_target_subdir/include"

// Verify -nostdinc disables all system include paths.
// RUN: %clang -### %s --target=i686-unknown-nanvix -fsyntax-only \
// RUN:     -nostdinc \
// RUN:     -resource-dir=%S/Inputs/resource_dir_with_per_target_subdir \
// RUN:     --sysroot=%S/Inputs/basic_nanvix_tree 2>&1 \
// RUN:     | FileCheck -check-prefix=CHECK-NOSTDINC %s

// CHECK-NOSTDINC: "-cc1"
// CHECK-NOSTDINC-NOT: "-internal-isystem"

// Verify -nostdlibinc keeps builtin includes but suppresses sysroot includes.
// RUN: %clang -### %s --target=i686-unknown-nanvix -fsyntax-only \
// RUN:     -nostdlibinc \
// RUN:     -resource-dir=%S/Inputs/resource_dir_with_per_target_subdir \
// RUN:     --sysroot=%S/Inputs/basic_nanvix_tree 2>&1 \
// RUN:     | FileCheck -check-prefix=CHECK-NOSTDLIBINC %s

// CHECK-NOSTDLIBINC: "-internal-isystem" "{{.*}}/resource_dir_with_per_target_subdir/include"

// Verify -nobuiltininc suppresses builtin includes from resource dir.
// RUN: %clang -### %s --target=i686-unknown-nanvix -fsyntax-only \
// RUN:     -nobuiltininc \
// RUN:     -resource-dir=%S/Inputs/resource_dir_with_per_target_subdir \
// RUN:     --sysroot=%S/Inputs/basic_nanvix_tree 2>&1 \
// RUN:     | FileCheck -check-prefix=CHECK-NOBUILTININC %s

// CHECK-NOBUILTININC: "-cc1"
// CHECK-NOBUILTININC-NOT: "-internal-isystem" "{{.*}}/resource_dir_with_per_target_subdir/include"
