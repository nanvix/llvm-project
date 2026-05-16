# Platform definition for the Nanvix operating system.
#
# This module is loaded by CMake when CMAKE_SYSTEM_NAME is set to "Nanvix".
# It describes the target platform conventions for cross-compilation.

set(NANVIX 1)
set(UNIX 1)

# Nanvix uses static ELF executables; shared libraries are not supported.
# Individual runtime builds disable shared targets via their own options
# (e.g. LIBUNWIND_ENABLE_SHARED=OFF) rather than globally, to avoid
# duplicate-rule conflicts when both STATIC and SHARED targets exist.
set(CMAKE_EXECUTABLE_SUFFIX "")
set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
set(CMAKE_STATIC_LIBRARY_SUFFIX ".a")

# Ensure CMAKE_MODULE_PATH is visible to try_compile() sub-projects so they
# can locate this platform file.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES CMAKE_MODULE_PATH)

# Search host paths for programs (e.g. the native compiler), but restrict
# libraries, includes, and packages to the sysroot.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
