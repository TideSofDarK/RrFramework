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

#include "Rr_Profiler.h"

#if defined(__x86_64__) && !defined(__APPLE__)
#include <xxHash/xxh_x86dispatch.h>
#else
#include <xxHash/xxhash.h>
#endif

Rr_Profiler *Rr_CreateProfiler(Rr_Arena *Arena)
{
    Rr_Profiler *Profiler = RR_ALLOC_TYPE(Arena, Rr_Profiler);
    Profiler->Arena = Arena;
    return Profiler;
}

static inline Rr_ProfilerSection **Rr_FindSection(
    Rr_ProfilerSection **SectionRef,
    size_t SectionNameLength,
    const char *SectionName)
{
    for (uint64_t Hash = XXH64(SectionName, SectionNameLength, 0); *SectionRef;
         Hash <<= 2)
    {
        if (strcmp(SectionName, (*SectionRef)->Name) == 0)
        {
            return SectionRef;
        }
        SectionRef = &(*SectionRef)->Children[Hash >> 62];
    }
    return SectionRef;
}

void Rr_BeginSection(Rr_Profiler *Profiler, const char *SectionName)
{
    size_t NameLength = strlen(SectionName);
    Rr_ProfilerSection **SectionRef =
        Rr_FindSection(&Profiler->Section, NameLength, SectionName);
    if (*SectionRef)
    {
        (*SectionRef)->LastTicks = Rr_GetPerformanceCounter();
        return;
    }
    *SectionRef = RR_ALLOC_TYPE(Profiler->Arena, Rr_ProfilerSection);
    (*SectionRef)->Name = RR_ALLOC_NO_ZERO(Profiler->Arena, NameLength + 1);
    memcpy((*SectionRef)->Name, SectionName, NameLength + 1);
    (*SectionRef)->LastTicks = Rr_GetPerformanceCounter();
}

void Rr_EndSection(Rr_Profiler *Profiler, const char *SectionName)
{
    size_t NameLength = strlen(SectionName);
    Rr_ProfilerSection **SectionRef =
        Rr_FindSection(&Profiler->Section, NameLength, SectionName);
    if (*SectionRef)
    {
        (*SectionRef)->TotalTicks +=
            Rr_GetPerformanceCounter() - (*SectionRef)->LastTicks;
        return;
    }
}

uint64_t Rr_GetSectionTicks(Rr_Profiler *Profiler, const char *SectionName)
{
    if (Profiler)
    {
        size_t NameLength = strlen(SectionName);
        Rr_ProfilerSection **SectionRef =
            Rr_FindSection(&Profiler->Section, NameLength, SectionName);
        if (*SectionRef)
        {
            return (*SectionRef)->TotalTicks;
        }
    }

    return 0;
}
