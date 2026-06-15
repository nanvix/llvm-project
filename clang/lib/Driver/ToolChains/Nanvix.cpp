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
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/DriverDiagnostic.h"
#include "clang/Driver/Options.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/VirtualFileSystem.h"

using namespace clang::driver;
using namespace clang::driver::toolchains;
using namespace clang::driver::tools;
using namespace clang;
using namespace llvm::opt;

/// Nanvix - Nanvix tool chain which can call as(1) and ld(1) directly.

Nanvix::Nanvix(const Driver &D, const llvm::Triple &Triple, const ArgList &Args)
    : Generic_ELF(D, Triple, Args) {

  // The Nanvix C runtime (crt0.o, libc.a, libm.a, and the empty
  // -ldl/-lpthread/-lrt stub archives) is staged into the single canonical
  // triple directory of the sysroot (doc/toolchain-migration.md §4.2,
  // decision 7).  Make it a library-search path so GetFilePath() finds crt0.o
  // and -lc/-lm resolve there, without probing any GCC lib/gcc/<ver> tree.
  getFilePaths().push_back(D.SysRoot + "/" + Triple.str() + "/lib");
}

Tool *Nanvix::buildLinker() const { return new tools::nanvix::Linker(*this); }

Tool *Nanvix::buildAssembler() const {
  return new tools::nanvix::Assembler(*this);
}

void tools::nanvix::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                                         const InputInfo &Output,
                                         const InputInfoList &Inputs,
                                         const ArgList &Args,
                                         const char *LinkingOutput) const {
  const ToolChain &ToolChain = getToolChain();
  const Driver &D = ToolChain.getDriver();
  const llvm::Triple &Triple = ToolChain.getEffectiveTriple();

  ArgStringList CmdArgs;

  // Silence warnings for "clang -g foo.o -o foo", "-emit-llvm", and "-w".
  Args.ClaimAllArgs(options::OPT_g_Group);
  Args.ClaimAllArgs(options::OPT_emit_llvm);
  Args.ClaimAllArgs(options::OPT_w);

  if (!D.SysRoot.empty())
    CmdArgs.push_back(Args.MakeArgString("--sysroot=" + D.SysRoot));

  if (Args.hasArg(options::OPT_s))
    CmdArgs.push_back("-s");

  // ELF emulation, selected by arch (OS-independent). Nanvix guests are i686.
  const char *Emulation = nullptr;
  switch (ToolChain.getArch()) {
  case llvm::Triple::x86:
    Emulation = "elf_i386";
    break;
  case llvm::Triple::x86_64:
    Emulation = "elf_x86_64";
    break;
  default:
    D.Diag(diag::err_target_unknown_triple) << Triple.str();
    return;
  }
  CmdArgs.push_back("-m");
  CmdArgs.push_back(Emulation);

  if (Args.hasArg(options::OPT_static))
    CmdArgs.push_back("-static");
  if (Args.hasArg(options::OPT_shared))
    CmdArgs.push_back("-shared");

  // Nanvix link-line contract (doc/toolchain-migration.md §4.1, §4.3): the
  // guest loader needs page-aligned LOAD segments and cannot handle GNU_RELRO;
  // no build-id note is emitted; and clang's autolink (-dependent-libraries)
  // hints for -lpthread/-ldl are ignored, since those symbols already live in
  // libc.a (empty stub archives satisfy any residual -l).
  CmdArgs.push_back("-z");
  CmdArgs.push_back("norelro");
  CmdArgs.push_back("--build-id=none");
  CmdArgs.push_back("--no-dependent-libraries");

  CmdArgs.push_back("-o");
  CmdArgs.push_back(Output.getFilename());

  // Startfile: crt0.o ONLY (no crt1/crti/crtbegin and no endfiles). The runtime
  // is brought up by __nanvix_libc_start_main in libc.a, which walks
  // .init_array; there is no GCC-style _init/_fini framing to link.
  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nostartfiles,
                   options::OPT_r))
    CmdArgs.push_back(Args.MakeArgString(ToolChain.GetFilePath("crt0.o")));

  Args.addAllArgs(CmdArgs, {options::OPT_L, options::OPT_u});
  ToolChain.AddFilePathLibArgs(Args, CmdArgs);

  AddLinkerInputs(ToolChain, Inputs, Args, CmdArgs, JA);

  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nodefaultlibs,
                   options::OPT_r)) {
    // One group resolves the crt0 <-> libc <-> libm <-> port back-references.
    // libc.a embeds the POSIX syscall backend, the allocator, and errno; libm.a
    // is the standalone math archive. Builtins come from compiler-rt
    // (decision 4), never libgcc.
    CmdArgs.push_back("--start-group");
    if (D.CCCIsCXX() && ToolChain.ShouldLinkCXXStdlib(Args))
      ToolChain.AddCXXStdlibLibArgs(Args, CmdArgs);
    AddRunTimeLibs(ToolChain, D, CmdArgs, Args);
    CmdArgs.push_back("-lm");
    CmdArgs.push_back("-lc");
    CmdArgs.push_back("--end-group");
  }

  // The guest linker script (ENTRY(_do_start) + the .init_array/.fini_array
  // blocks) is supplied by the port via -T; the Nanvix sysroot ships
  // lib/user.ld for that purpose.
  Args.addAllArgs(CmdArgs, {options::OPT_T, options::OPT_t});

  const char *Exec = Args.MakeArgString(ToolChain.GetLinkerPath());
  C.addCommand(std::make_unique<Command>(JA, *this,
                                         ResponseFileSupport::AtFileCurCP(),
                                         Exec, CmdArgs, Inputs, Output));
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
