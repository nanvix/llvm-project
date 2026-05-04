//===--- Nanvix.cpp - Nanvix ToolChain Implementations --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Nanvix.h"
#include "CommonArgs.h"
#include "Gnu.h"
#include "clang/Config/config.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/VirtualFileSystem.h"

using namespace clang::driver;
using namespace clang::driver::toolchains;
using namespace clang;
using namespace llvm::opt;

/// Nanvix - Nanvix tool chain which can call as(1) and ld(1) directly.

Nanvix::Nanvix(const Driver &D, const llvm::Triple &Triple, const ArgList &Args)
    : Generic_ELF(D, Triple, Args) {

  // TODO: add supported libraries.
}

void Nanvix::AddClangSystemIncludeArgs(const ArgList &DriverArgs,
                                       ArgStringList &CC1Args) const {
  const Driver &D = getDriver();

  // Check for -nostdinc, which disables all system include paths.
  if (DriverArgs.hasArg(clang::driver::options::OPT_nostdinc)) {
    return;
  }

  if (!DriverArgs.hasArg(options::OPT_nobuiltininc)) {
    SmallString<128> P(D.ResourceDir);
    llvm::sys::path::append(P, "include");
    addSystemInclude(DriverArgs, CC1Args, P);
  }

  if (DriverArgs.hasArg(options::OPT_nostdlibinc))
    return;

  // Check for configure-time C include directories.
  StringRef CIncludeDirs(C_INCLUDE_DIRS);
  if (CIncludeDirs != "") {
    SmallVector<StringRef, 5> dirs;
    CIncludeDirs.split(dirs, ":");
    for (StringRef dir : dirs) {
      StringRef Prefix =
          llvm::sys::path::is_absolute(dir) ? "" : StringRef(D.SysRoot);
      addExternCSystemInclude(DriverArgs, CC1Args, Prefix + dir);
    }
    return;
  }
}

Tool *Nanvix::buildLinker() const {
  return new tools::nanvix::Linker(*this);
}

Tool *Nanvix::buildAssembler() const {
  return new tools::nanvix::Assembler(*this);
}

void Nanvix::AddClangCXXStdlibIncludeArgs(
    const llvm::opt::ArgList &DriverArgs,
    llvm::opt::ArgStringList &CC1Args) const {
  // Check for (1)
  // Get from '<install>/bin' to '<install>/include/c++/v1'.
  // Note that ToolchainDir can be relative, so we use '..' instead of
  // parent_path.
  llvm::SmallString<128> ToolchainDir(getDriver().Dir); // <install>/bin
  llvm::sys::path::append(ToolchainDir, "..");
  if (getVFS().exists(ToolchainDir)) {
    addSystemInclude(DriverArgs, CC1Args, ToolchainDir + "/include/c++/v1");
    // TODO: use triple to determine the correct path.
    addSystemInclude(DriverArgs, CC1Args,
                     ToolchainDir + "/include/i686-unknown-nanvix/c++/v1/");
    return;
  } else if (DriverArgs.hasArg(options::OPT_v)) {
    llvm::errs() << "ignoring nonexistent directory \"" << ToolchainDir
                 << "\"\n";
  }
}
