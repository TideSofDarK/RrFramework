/*
 * Copyright (C) 2024-2026 Alexandr Semenov <tidesmain@gmail.com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#ifndef RR_DEFINES_H
#define RR_DEFINES_H

#ifndef __cplusplus
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#else
#include <cstddef>
#include <cstdint>
#endif

#ifndef RR_CC
#define RR_CC
#endif

#if defined(_MSC_VER)
#define RR_MSVC
#if defined(_M_AMD64)
#define RR_X86
#define RR_MSVC_X86
#elif defined(_M_ARM64)
#define RR_ARM
#define RR_MSVC_ARM
#else
#error Unknown architecture!
#endif
#else
#if defined(__clang__)
#define RR_CLANG
#elif defined(__GNUC__)
#define RR_GNU
#else
#error Unknown compiler!
#endif
#define RR_COMPILER_GNU_OR_CLANG
#if defined(__x86_64__)
#define RR_X86
#elif defined(__arm__) || defined(__aarch64__)
#define RR_ARM
#else
#error Unknown architecture!
#endif
#endif

#define RR_UNUSED(Var) (void)Var

#define RR_ZERO(x)     memset(&(x), 0, sizeof((x)))
#define RR_ZERO_PTR(x) memset((x), 0, sizeof(*(x)))

#define RR_KIBIBYTES(Value) ((Value) * 1024ull)
#define RR_MEBIBYTES(Value) ((Value) * RR_KIBIBYTES(1024))
#define RR_GIBIBYTES(Value) ((Value) * RR_MEBIBYTES(1024))

#define RR_ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

#ifndef RR_SAFE_ALIGNMENT
#define RR_SAFE_ALIGNMENT 16
#endif

#define RR_ALIGN_POW2(Num, Alignment) \
    (((Num) + ((Alignment) - 1)) & ~((Alignment) - 1))
#define RR_IS_POW2(Num) (((Num - 1) & Num) == 0)

#endif
