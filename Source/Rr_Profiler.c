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

#include "Rr_Profiler.h"

#include "Rr_Hash.h"
#include "Rr_System.h"

#include <string.h>

Rr_Profiler *Rr_CreateProfiler(Rr_Arena *Arena)
{
    Rr_Profiler *Profiler = Rr_Alloc(sizeof(Rr_Profiler), Arena);
    Profiler->Arena = Arena;

    return Profiler;
}

static inline Rr_ProfilerSection **Rr_FindSection(
    Rr_ProfilerSection **SectionRef,
    size_t SectionNameLength,
    char const *SectionName)
{
    for (uint64_t Hash = Rr_Hash64(SectionNameLength, SectionName); *SectionRef;
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

void Rr_BeginSection(Rr_Profiler *Profiler, char const *SectionName)
{
    Rr_Arena *Arena = Profiler->Arena;
    size_t NameLength = strlen(SectionName);
    Rr_ProfilerSection **SectionRef =
        Rr_FindSection(&Profiler->Section, NameLength, SectionName);
    if (*SectionRef)
    {
        (*SectionRef)->LastTicks = Rr_GetPerformanceCounter();

        return;
    }
    *SectionRef = Rr_Alloc(sizeof(Rr_ProfilerSection), Arena);
    (*SectionRef)->Name = Rr_AllocCopy(SectionName, NameLength + 1, Arena);
    (*SectionRef)->LastTicks = Rr_GetPerformanceCounter();
}

void Rr_EndSection(Rr_Profiler *Profiler, char const *SectionName)
{
    size_t NameLength = strlen(SectionName);
    Rr_ProfilerSection **SectionRef =
        Rr_FindSection(&Profiler->Section, NameLength, SectionName);
    if (*SectionRef)
    {
        (*SectionRef)->TotalTicks +=
            Rr_GetPerformanceCounter() - (*SectionRef)->LastTicks;
    }
}

uint64_t Rr_GetSectionTicks(Rr_Profiler *Profiler, char const *SectionName)
{
    size_t NameLength = strlen(SectionName);
    Rr_ProfilerSection **SectionRef =
        Rr_FindSection(&Profiler->Section, NameLength, SectionName);
    if (*SectionRef)
    {
        return (*SectionRef)->TotalTicks;
    }

    return 0;
}

double Rr_GetSectionMS(Rr_Profiler *Profiler, char const *Name)
{
    return (double)Rr_GetSectionNS(Profiler, Name) / 1000000.0;
}

uint64_t Rr_GetSectionNS(Rr_Profiler *Profiler, char const *Name)
{
    uint64_t Elapsed = Rr_GetSectionTicks(Profiler, Name);

    return Elapsed * Rr_GetSystem()->QPCToNS;
}
