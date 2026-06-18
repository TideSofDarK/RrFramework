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

#include "Rr_Thread.h"

#define RR_LOG_MACRO_CATEGORY RR_LOG_CATEGORY_THREAD
#include "Rr_LogMacro.h"

#include "Rr_Arena.h"
#include "Rr_RHI.h"

#include <assert.h>
#include <string.h>

static RR_THREAD_LOCAL Rr_ThreadContext gThreadContext = { 0 };

static Rr_Arena *gPermanentFreeList = NULL;
static Rr_Spinlock gPermanentLock = { 0 };

static Rr_Arena *gScratchFreeList = NULL;
static Rr_Spinlock gScratchLock = { 0 };

static inline Rr_Arena *Rr_GetFreeArena(
    Rr_Arena **FreeList,
    Rr_Spinlock *Lock,
    size_t ReserveSize,
    size_t CommitSize)
{
    Rr_Arena *Arena = NULL;

    Rr_LockSpinlock(Lock);

    if (*FreeList)
    {
        Arena = *FreeList;
        *FreeList = Arena->Next;
        Arena->Next = NULL;
    }

    Rr_UnlockSpinlock(Lock);

    if (!Arena)
    {
        return Rr_CreateArena(ReserveSize, CommitSize);
    }

    return Arena;
}

static inline void Rr_ReturnFreeArena(
    Rr_Arena **FreeList,
    Rr_Arena *Arena,
    Rr_Spinlock *Lock)
{
    if (!Arena)
    {
        return;
    }

    Rr_LockSpinlock(Lock);

    Arena->Next = *FreeList;
    *FreeList = Arena;

    Rr_UnlockSpinlock(Lock);
}

static inline void Rr_DestroyArenaFreeList(Rr_Arena *Head)
{
    while (Head)
    {
        Rr_Arena *Next = Head->Next;
        Rr_DestroyArena(Head);
        Head = Next;
    }
}

void Rr_InitThreadContext(void)
{
    if (gThreadContext.Initialized)
    {
        return;
    }

    for (size_t Index = 0; Index < 2; ++Index)
    {
        gThreadContext.ScratchArenas[Index] = Rr_GetFreeArena(
            &gScratchFreeList,
            &gScratchLock,
            RR_ARENA_RESERVE_DEFAULT,
            RR_ARENA_COMMIT_DEFAULT);
    }

    gThreadContext.Initialized = true;
}

void Rr_CleanupThreadContext(void)
{
    if (!gThreadContext.Initialized)
    {
        return;
    }

    if (!gThreadContext.Main)
    {
        Rr_ReleaseCommandPools();
    }

    Rr_ReturnFreeArena(
        &gScratchFreeList,
        gThreadContext.ScratchArenas[0],
        &gScratchLock);
    Rr_ReturnFreeArena(
        &gScratchFreeList,
        gThreadContext.ScratchArenas[1],
        &gScratchLock);

    Rr_ReturnFreeArena(
        &gPermanentFreeList,
        gThreadContext.PermanentArena,
        &gPermanentLock);

    if (gThreadContext.Main)
    {
        Rr_DestroyArenaFreeList(gScratchFreeList);
        Rr_DestroyArenaFreeList(gPermanentFreeList);
    }

    RR_ZERO(gThreadContext);
}

void Rr_SetMainThread(void)
{
    gThreadContext.Main = true;
}

Rr_ThreadContext *Rr_GetThreadContext(void)
{
    return &gThreadContext;
}

Rr_Arena *Rr_GetPermanent(void)
{
    assert(gThreadContext.Initialized);

    if (!gThreadContext.PermanentArena)
    {
        gThreadContext.PermanentArena = Rr_GetFreeArena(
            &gPermanentFreeList,
            &gPermanentLock,
            RR_ARENA_RESERVE_DEFAULT,
            RR_ARENA_COMMIT_DEFAULT);
    }

    return gThreadContext.PermanentArena;
}

Rr_Scratch Rr_GetScratch(Rr_Arena *Conflict)
{
    assert(
        gThreadContext.Initialized &&
        "Did you forget to call Rr_InitThreadContext()?");
    if (Conflict == NULL)
    {
        return Rr_CreateScratch(gThreadContext.ScratchArenas[0]);
    }
    else
    {
        for (size_t Index = 0; Index < 2; ++Index)
        {
            if (gThreadContext.ScratchArenas[Index] != Conflict)
            {
                return Rr_CreateScratch(gThreadContext.ScratchArenas[Index]);
            }
        }
    }

    RR_LOG_ABORT("Couldn't find appropriate arena for a scratch!");

    return (Rr_Scratch){ 0 };
}
