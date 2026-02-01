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

#include "Rr_Memory.h"

typedef struct Rr_ProfilerSection Rr_ProfilerSection;
struct Rr_ProfilerSection
{
    char *Name;
    uint64_t TotalTicks;
    uint64_t LastTicks;
    Rr_ProfilerSection *Children[4];
};

typedef struct Rr_Profiler Rr_Profiler;
struct Rr_Profiler
{
    Rr_ProfilerSection *Section;
    Rr_Arena *Arena;
};

extern Rr_Profiler *Rr_CreateProfiler(Rr_Arena *Arena);

extern void Rr_BeginSection(Rr_Profiler *Profiler, char const *Name);

extern void Rr_EndSection(Rr_Profiler *Profiler, char const *Name);

extern uint64_t Rr_GetSectionTicks(Rr_Profiler *Profiler, char const *Name);

extern void Rr_BeginFrameSection(char const *Name);

extern void Rr_EndFrameSection(char const *Name);

extern uint64_t Rr_GetFrameSectionTicks(char const *Name);
