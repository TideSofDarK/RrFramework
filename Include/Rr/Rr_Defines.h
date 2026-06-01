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

#ifdef __cplusplus
#define RR_EXTERN extern "C"
#else
#define RR_EXTERN extern
#endif

#define RR_UNUSED(Var) (void)Var

#define RR_HAS_BIT(Value, Bit) ((Value & Bit) != 0)

#define RR_ZERO(x)     memset(&(x), 0, sizeof((x)))
#define RR_ZERO_PTR(x) memset((x), 0, sizeof(*(x)))

#define RR_KILOBYTES(Value) ((Value) * 1024ull)
#define RR_MEGABYTES(Value) ((Value) * RR_KILOBYTES(1024))
#define RR_GIGABYTES(Value) ((Value) * RR_MEGABYTES(1024))

#define RR_ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

#ifndef RR_SAFE_ALIGNMENT
#define RR_SAFE_ALIGNMENT 16
#endif

#define RR_ALIGN_POW2(Num, Alignment) \
    (((Num) + ((Alignment) - 1)) & ~((Alignment) - 1))
#define RR_IS_POW2(Num) (((Num - 1) & Num) == 0)

#endif
