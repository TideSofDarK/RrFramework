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

extern void Rr_BeginSection(Rr_Profiler *Profiler, const char *SectionName);

extern void Rr_EndSection(Rr_Profiler *Profiler, const char *SectionName);

extern uint64_t Rr_GetSectionTicks(
    Rr_Profiler *Profiler,
    const char *SectionName);
