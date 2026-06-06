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

#include <Rr/Rr_Arena.h>

#define RR_LOG_MACRO_CATEGORY RR_LOG_CATEGORY_VULKAN
#include "Rr_LogMacro.h"
#include "Rr_System.h"

#include <Rr/Rr_Math.h>

#include <assert.h>
#include <string.h>

Rr_Arena *Rr_CreateArena(size_t ReserveSize, size_t CommitSize)
{
    size_t PageSize = Rr_GetSystem()->PageSize;
    ReserveSize = RR_ALIGN_POW2(ReserveSize, PageSize);
    CommitSize = RR_ALIGN_POW2(CommitSize, PageSize);

    Rr_Arena *Arena = Rr_ReserveMemory(ReserveSize);
    Rr_CommitMemory(Arena, CommitSize);
    *Arena = (Rr_Arena){
        .Position = sizeof(Rr_Arena),
        .ReserveSize = ReserveSize,
        .CommitSize = CommitSize,
        .Reserved = ReserveSize,
        .Commited = CommitSize,
    };

    return Arena;
}

static const size_t RR_ARENA_RESERVE_DEFAULT = RR_GIGABYTES(4);
static const size_t RR_ARENA_COMMIT_DEFAULT = RR_KILOBYTES(64);

Rr_Arena *Rr_CreateDefaultArena(void)
{
    return Rr_CreateArena(RR_ARENA_RESERVE_DEFAULT, RR_ARENA_COMMIT_DEFAULT);
}

void Rr_ResetArena(Rr_Arena *Arena)
{
    Arena->Position = sizeof(Rr_Arena);
}

void Rr_DestroyArena(Rr_Arena *Arena)
{
    if (Arena == NULL)
    {
        return;
    }

    Rr_ReleaseMemory((void *)Arena, Arena->ReserveSize);
}

void *Rr_AllocAlignedNoZero(size_t Size, size_t Align, Rr_Arena *Arena)
{
    if (Arena == NULL)
    {
        RR_LOG_ABORT("Allocating from NULL arena!");
    }

    if (Size == 0)
    {
        RR_LOG_ABORT("Allocating 0 bytes from an arena is not allowed!");
    }

    uintptr_t PositionAligned = RR_ALIGN_POW2(Arena->Position, Align);
    uintptr_t Target = PositionAligned + (uintptr_t)Size;

    if (Arena->Commited < Target)
    {
        uintptr_t CommitTarget = Target + Arena->CommitSize - 1;
        CommitTarget -= CommitTarget % Arena->CommitSize;
        CommitTarget = RR_MIN(CommitTarget, Arena->Reserved);
        uintptr_t CommitSize = CommitTarget - Arena->Commited;
        char *CommitPtr = (char *)Arena + Arena->Commited;
        Rr_CommitMemory(CommitPtr, CommitSize);
        Arena->Commited = CommitTarget;
    }

    char *Result = NULL;
    if (Arena->Commited >= Target)
    {
        Result = (char *)Arena + PositionAligned;
        Arena->Position = Target;
    }
    else
    {
        RR_LOG_ABORT("Arena reserved memory overflow!");
    }

    return Result;
}

void *Rr_AllocAligned(size_t Size, size_t Align, Rr_Arena *Arena)
{
    return memset(Rr_AllocAlignedNoZero(Size, Align, Arena), 0, Size);
}

void *Rr_AllocNoZero(size_t Size, Rr_Arena *Arena)
{
    return Rr_AllocAlignedNoZero(Size, RR_SAFE_ALIGNMENT, Arena);
}

void *Rr_Alloc(size_t Size, Rr_Arena *Arena)
{
    return memset(
        Rr_AllocAlignedNoZero(Size, RR_SAFE_ALIGNMENT, Arena),
        0,
        Size);
}

void *Rr_AllocCopy(void *Source, size_t Size, Rr_Arena *Arena)
{
    return memcpy(Rr_AllocNoZero(Size, Arena), Source, Size);
}

Rr_Scratch Rr_CreateScratch(Rr_Arena *Arena)
{
    return (Rr_Scratch){ .Arena = Arena, .Position = Arena->Position };
}

void Rr_DestroyScratch(Rr_Scratch Scratch)
{
    Scratch.Arena->Position = Scratch.Position;
}

static RR_THREAD_LOCAL Rr_Arena *ScratchArenas[2] = { 0 };
static RR_THREAD_LOCAL bool ScratchInitialized = false;

void Rr_CleanupScratchArena(void)
{
    if (!ScratchInitialized)
    {
        return;
    }
    for (size_t Index = 0; Index < 2; ++Index)
    {
        Rr_DestroyArena(ScratchArenas[Index]);
    }
}

void Rr_InitScratchArena(void)
{
    if (ScratchInitialized)
    {
        return;
    }
    for (size_t Index = 0; Index < 2; ++Index)
    {
        ScratchArenas[Index] = Rr_CreateDefaultArena();
    }
}

Rr_Scratch Rr_GetScratch(Rr_Arena *Conflict)
{
    assert(
        ScratchArenas[0] != NULL && "Did you forget to call Rr_InitScratch()?");
    if (Conflict == NULL)
    {
        return Rr_CreateScratch(ScratchArenas[0]);
    }
    else
    {
        for (size_t Index = 0; Index < 2; ++Index)
        {
            if (ScratchArenas[Index] != Conflict)
            {
                return Rr_CreateScratch(ScratchArenas[Index]);
            }
        }
    }

    RR_LOG_ABORT("Couldn't find appropriate arena for a scratch!");

    return (Rr_Scratch){ 0 };
}
