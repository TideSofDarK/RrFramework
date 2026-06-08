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

#pragma once

#include <Rr/Rr_System.h>

#if defined(_MSC_VER) && !defined(__clang__) && !defined(__INTEL_COMPILER)
#define RR_THREAD_LOCAL __declspec(thread)
#else
#define RR_THREAD_LOCAL __thread
#endif

typedef struct Rr_System Rr_System;
struct Rr_System
{
    bool Initialized;
    size_t PageSize;
    size_t AllocationGranularity;
    uint64_t PerformanceFrequency;
};

extern void Rr_InitSystem(void);

extern Rr_System *Rr_GetSystem(void);
