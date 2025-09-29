/*
 * MIT License
 *
 * Copyright (c) 2024-2025 Alexandr Semenov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#ifndef __cplusplus
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#else
#include <cstddef>
#include <cstdint>
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

#define RR_SAFE_ALIGNMENT 16
#define RR_ALIGN_POW2(Num, Alignment) \
    (((Num) + ((Alignment) - 1)) & ~((Alignment) - 1))
#define RR_IS_POW2(Num) (((Num - 1) & Num) == 0)
