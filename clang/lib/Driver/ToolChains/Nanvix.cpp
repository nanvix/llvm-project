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
#include "llvm/ADT/SmallString.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/VirtualFileSystem.h"

using namespace clang::driver;
using namespace clang::driver::toolchains;
using namespace clang::driver::tools;
using namespace clang;
using namespace llvm::opt;

/// Nanvix Linker
void nanvix::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                                  const InputInfo &Output,
                                  const InputInfoList &Inputs,
                                  const ArgList &Args,
                                  const char *LinkingOutput) const {
  const auto &ToolChain = static_cast<const Nanvix &>(getToolChain());
  const Driver &D = ToolChain.getDriver();

  ArgStringList CmdArgs;

  // Silence warning for "clang -g foo.o -o foo"
  Args.ClaimAllArgs(options::OPT_g_Group);
  // and "clang -emit-llvm foo.o -o foo"
  Args.ClaimAllArgs(options::OPT_emit_llvm);
  // and for "clang -w foo.o -o foo".
  Args.ClaimAllArgs(options::OPT_w);

  if (!D.SysRoot.empty())
    CmdArgs.push_back(Args.MakeArgString("--sysroot=" + D.SysRoot));

  if (Args.hasArg(options::OPT_s))
    CmdArgs.push_back("-s");

  if (Args.hasArg(options::OPT_r)) {
    CmdArgs.push_back("-r");
  } else {
    // Nanvix's ELF loader requires all LOAD segments to be page-aligned.
    // LLD's default --build-id adds a .note.gnu.build-id orphan section that
    // can break segment alignment with the Nanvix linker script, and
    // GNU_RELRO creates an extra LOAD segment the loader cannot handle.
    CmdArgs.push_back("--build-id=none");
    CmdArgs.push_back("--no-rosegment");
    CmdArgs.push_back("-z");
    CmdArgs.push_back("norelro");
  }

  if (Args.hasArg(options::OPT_static))
    CmdArgs.push_back("-Bstatic");

  if (!Args.hasArg(options::OPT_r)) {
    CmdArgs.push_back("--eh-frame-hdr");
    CmdArgs.push_back("--gc-sections");

    // Nanvix does not provide a standalone libpthread; pthread symbols are
    // supplied by the OS library (libposix.a) or stubbed out.  Ignore the
    // dependent-library metadata that libc++/libc++abi embed
    // (e.g., "pthread").
    CmdArgs.push_back("--no-dependent-libraries");
  }

  CmdArgs.push_back("-o");
  CmdArgs.push_back(Output.getFilename());

  // CRT startup: Nanvix uses newlib's crt0.o plus GCC's crti/crtbegin.
  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nostartfiles,
                   options::OPT_r)) {
    CmdArgs.push_back(Args.MakeArgString(ToolChain.GetFilePath("crt0.o")));
    CmdArgs.push_back(Args.MakeArgString(ToolChain.GetFilePath("crti.o")));
    CmdArgs.push_back(Args.MakeArgString(ToolChain.GetFilePath("crtbegin.o")));
  }

  // Forward -L (library search paths) and -u (undefined symbol) flags.
  Args.addAllArgs(CmdArgs, {options::OPT_L, options::OPT_u});

  // Library search paths from the toolchain.
  ToolChain.AddFilePathLibArgs(Args, CmdArgs);

  // User inputs.
  addLinkerCompressDebugSectionsOption(ToolChain, Args, CmdArgs);
  AddLinkerInputs(ToolChain, Inputs, Args, CmdArgs, JA);

  // Standard libraries.
  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nodefaultlibs,
                   options::OPT_r)) {
    // Use --start-group/--end-group to resolve circular dependencies
    // between static archives (e.g., libc referencing compiler-rt builtins).
    CmdArgs.push_back("--start-group");

    // C++ standard library.
    if (D.CCCIsCXX() && ToolChain.ShouldLinkCXXStdlib(Args))
      ToolChain.AddCXXStdlibLibArgs(Args, CmdArgs);

    // Compiler runtime (compiler-rt builtins).
    AddRunTimeLibs(ToolChain, D, CmdArgs, Args);

    CmdArgs.push_back("-lm");
    CmdArgs.push_back("-lc");

    CmdArgs.push_back("--end-group");
  }

  // Linker scripts from -T flags (before CRT finalization so that scripts
  // can govern the placement of all sections including CRT objects).
  Args.addAllArgs(CmdArgs, {options::OPT_T});

  // CRT finalization: crtend.o and crtn.o.
  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nostartfiles,
                   options::OPT_r)) {
    CmdArgs.push_back(Args.MakeArgString(ToolChain.GetFilePath("crtend.o")));
    CmdArgs.push_back(Args.MakeArgString(ToolChain.GetFilePath("crtn.o")));
  }

  const char *Exec = Args.MakeArgString(ToolChain.GetLinkerPath());
  C.addCommand(std::make_unique<Command>(JA, *this,
                                         ResponseFileSupport::AtFileCurCP(),
                                         Exec, CmdArgs, Inputs, Output));
}

/// Nanvix ToolChain

Nanvix::Nanvix(const Driver &D, const llvm::Triple &Triple, const ArgList &Args)
    : Generic_ELF(D, Triple, Args) {

  getProgramPaths().push_back(getDriver().Dir);

  if (!D.SysRoot.empty()) {
    // $SYSROOT/lib — libc++, libc++abi, libunwind (from LLVM stage 1 runtimes)
    SmallString<128> P(D.SysRoot);
    llvm::sys::path::append(P, "lib");
    getFilePaths().push_back(std::string(P));

    // Vendor-stripped triple used by the GCC cross-compiler
    // (e.g., "i686-nanvix").
    std::string GCCTriple =
        (Triple.getArchName() + "-" + Triple.getOSName()).str();

    // $SYSROOT/<target>/lib — libc, libm, crt0.o (from newlib/GCC sysroot)
    // Probe the full triple first (e.g., "i686-unknown-nanvix"), then fall
    // back to the vendor-stripped form used by the GCC cross-compiler.
    P = SmallString<128>(D.SysRoot);
    llvm::sys::path::append(P, Triple.str(), "lib");
    if (getVFS().exists(P)) {
      getFilePaths().push_back(std::string(P));
    } else {
      P = SmallString<128>(D.SysRoot);
      llvm::sys::path::append(P, GCCTriple, "lib");
      if (getVFS().exists(P))
        getFilePaths().push_back(std::string(P));
    }

    // $SYSROOT/lib/gcc/<gcc-triple>/<version>/ — crti.o, crtbegin.o,
    // crtend.o, crtn.o (from the GCC cross-compiler).
    // Select the highest version directory using GCCVersion comparison.
    SmallString<128> GCCDir(D.SysRoot);
    llvm::sys::path::append(GCCDir, "lib", "gcc", GCCTriple);
    if (getVFS().exists(GCCDir)) {
      std::error_code EC;
      Generic_GCC::GCCVersion BestVersion = {"" , -1, -1, -1, "", "", ""};
      std::string BestPath;
      for (llvm::vfs::directory_iterator
               DI = getVFS().dir_begin(GCCDir, EC),
               DE;
           !EC && DI != DE; DI.increment(EC)) {
        if (DI->type() != llvm::sys::fs::file_type::directory_file &&
            DI->type() != llvm::sys::fs::file_type::type_unknown)
          continue;
        StringRef VersionText = llvm::sys::path::filename(DI->path());
        auto CandidateVersion = Generic_GCC::GCCVersion::Parse(VersionText);
        if (CandidateVersion.Major == -1)
          continue;
        if (BestPath.empty() || BestVersion < CandidateVersion) {
          BestVersion = CandidateVersion;
          BestPath = std::string(DI->path());
        }
      }
      if (!BestPath.empty())
        getFilePaths().push_back(std::move(BestPath));
    }
  }
}

std::string Nanvix::getCompilerRTPath() const {
  // Compiler-rt builtins are installed in a per-target directory
  // (e.g., lib/clang/21/lib/i686-unknown-nanvix/) but may use the
  // old naming convention (libclang_rt.builtins-i386.a). Point the
  // fallback path at the per-target directory so the arch-suffixed
  // name is found.
  SmallString<128> Path(getDriver().ResourceDir);
  llvm::sys::path::append(Path, "lib", getTriple().str());
  return std::string(Path);
}

void Nanvix::addClangTargetOptions(const ArgList &DriverArgs,
                                   ArgStringList &CC1Args,
                                   Action::OffloadKind) const {
  // By default, use `.ctors` (not `.init_array`), as required by GCC's
  // crtbegin/crtend which handle constructors/destructors on Nanvix.
  if (!DriverArgs.hasFlag(options::OPT_fuse_init_array,
                          options::OPT_fno_use_init_array, false))
    CC1Args.push_back("-fno-use-init-array");
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
  // Get from '<install>/bin' to '<install>/include/c++/v1'.
  // Note that ToolchainDir can be relative, so we use '..' instead of
  // parent_path.
  llvm::SmallString<128> ToolchainDir(getDriver().Dir); // <install>/bin
  llvm::sys::path::append(ToolchainDir, "..");
  if (getVFS().exists(ToolchainDir)) {
    addSystemInclude(DriverArgs, CC1Args, ToolchainDir + "/include/c++/v1");
    // Target-specific libc++ headers.
    addSystemInclude(DriverArgs, CC1Args,
                     ToolchainDir + "/include/" +
                         getTriple().str() + "/c++/v1/");
    return;
  } else if (DriverArgs.hasArg(options::OPT_v)) {
    llvm::errs() << "ignoring nonexistent directory \"" << ToolchainDir
                 << "\"\n";
  }
}

void Nanvix::AddCXXStdlibLibArgs(const llvm::opt::ArgList &Args,
                                  llvm::opt::ArgStringList &CmdArgs) const {
  switch (GetCXXStdlibType(Args)) {
  case ToolChain::CST_Libcxx:
    CmdArgs.push_back("-lc++");
    CmdArgs.push_back("-lc++abi");
    CmdArgs.push_back("-lunwind");
    break;
  case ToolChain::CST_Libstdcxx:
    getDriver().Diag(diag::err_drv_invalid_stdlib_name)
        << Args.getLastArgValue(options::OPT_stdlib_EQ);
    break;
  }
}
