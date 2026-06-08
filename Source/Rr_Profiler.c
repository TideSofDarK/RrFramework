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
#include "Rr_Renderer.h"
#include "Rr_System.h"

Rr_Profiler *Rr_CreateProfiler(Rr_Arena *Arena)
{
    Rr_Profiler *Profiler = Rr_Alloc(sizeof(Rr_Profiler), Arena);
    Profiler->Arena = Arena;
    return Profiler;
}

static inline Rr_ProfilerSection **Rr_FindSection(
    Rr_ProfilerSection **SectionRef,
    size_t SectionNameLength,
    const char *SectionName)
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
    *SectionRef = Rr_Alloc(sizeof(Rr_ProfilerSection), Profiler->Arena);
    (*SectionRef)->Name = Rr_AllocNoZero(NameLength + 1, Profiler->Arena);
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

void Rr_BeginFrameSection(char const *Name)
{
    Rr_BeginSection(Rr_GetCurrentFrame()->Profiler, Name);
}

void Rr_EndFrameSection(char const *Name)
{
    Rr_EndSection(Rr_GetCurrentFrame()->Profiler, Name);
}

uint64_t Rr_GetFrameSectionTicks(char const *Name)
{
    return Rr_GetSectionTicks(Rr_GetPreviousFrame()->Profiler, Name);
}
