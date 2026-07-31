#ifndef KYTY_COMMON_CONFIG_H_
#define KYTY_COMMON_CONFIG_H_

#define KYTY_PLATFORM_WINDOWS 1
#define KYTY_PLATFORM_MACOS   3
#define KYTY_PLATFORM_LINUX   5

#define KYTY_COMPILER_GCC   1
#define KYTY_COMPILER_CLANG 2

#define KYTY_LINKER_LD       1
#define KYTY_LINKER_LLD      2
#define KYTY_LINKER_LLD_LINK 3

#define KYTY_BUILD_DEBUG   1
#define KYTY_BUILD_RELEASE 2

#define KYTY_ENDIAN_BIG    1
#define KYTY_ENDIAN_LITTLE 2

#define KYTY_ARCH_X86_64 1
#define KYTY_ARCH_ARM64  2

#if defined(__x86_64__) || defined(_M_X64)
#define KYTY_ARCH KYTY_ARCH_X86_64
#elif defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM64)
#define KYTY_ARCH KYTY_ARCH_ARM64
#endif

#include "cmake_config.h"

#endif /* KYTY_COMMON_CONFIG_H_ */

