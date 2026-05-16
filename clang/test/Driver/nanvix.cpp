// RUN: %clangxx -### %s --target=i686-unknown-nanvix \
// RUN:     -resource-dir=%S/Inputs/resource_dir_with_per_target_subdir \
// RUN:     --sysroot=%S/Inputs/basic_nanvix_tree \
// RUN:     -stdlib=libc++ 2>&1 \
// RUN:     | FileCheck -check-prefix=CHECK %s

// CHECK: "-cc1"
// CHECK: "-triple" "i686-unknown-nanvix"
// CHECK: "-internal-isystem" "{{[^"]*}}/include/c++/v1"
// CHECK: {{.*}}ld.lld
// CHECK: "--start-group"
// CHECK: "-lc++"
// CHECK: "-lc++abi"
// CHECK: "-lunwind"
// CHECK: "{{[^"]*}}libclang_rt.builtins.a"
// CHECK: "-lm"
// CHECK: "-lc"
// CHECK: "--end-group"
