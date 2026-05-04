//===--- Nanvix.h - Nanvix ToolChain Implementations ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_NANVIX_H
#define LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_NANVIX_H

#include "Gnu.h"
#include "clang/Driver/Tool.h"
#include "clang/Driver/ToolChain.h"

namespace clang {
namespace driver {
namespace tools {

/// Directly call Nanvix assembler and linker
namespace nanvix {
class LLVM_LIBRARY_VISIBILITY Assembler final : public gnutools::Assembler {
public:
  Assembler(const ToolChain &TC) : gnutools::Assembler(TC) {}
};

class LLVM_LIBRARY_VISIBILITY Linker final : public gnutools::Linker {
public:
  Linker(const ToolChain &TC) : gnutools::Linker(TC) {}
};
} // end namespace nanvix
} // end namespace tools

namespace toolchains {

class LLVM_LIBRARY_VISIBILITY Nanvix : public Generic_ELF {
public:
  Nanvix(const Driver &D, const llvm::Triple &Triple,
         const llvm::opt::ArgList &Args);

  void
  AddClangSystemIncludeArgs(const llvm::opt::ArgList &DriverArgs,
                            llvm::opt::ArgStringList &CC1Args) const override;

  void AddClangCXXStdlibIncludeArgs(
      const llvm::opt::ArgList &DriverArgs,
      llvm::opt::ArgStringList &CC1Args) const override;

protected:
  Tool *buildLinker() const override;
  Tool *buildAssembler() const override;
};

} // end namespace toolchains
} // end namespace driver
} // end namespace clang

#endif // LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_NANVIX_H
