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

#include "Rr_Renderer.h"
#include "Rr_System.h"

static RR_THREAD_LOCAL Rr_ThreadContext *ThreadContext = NULL;

void Rr_InitThreadContext(void)
{
    if (ThreadContext)
    {
        return;
    }

    Rr_Arena *Arena = Rr_CreateDefaultArena();

    ThreadContext = Rr_Alloc(sizeof(Rr_ThreadContext), Arena);
    ThreadContext->Arena = Arena;

    for (size_t Index = 0; Index < 2; ++Index)
    {
        ThreadContext->ScratchArenas[Index] = Rr_CreateDefaultArena();
    }
}

void Rr_CleanupThreadContext(void)
{
    if (!ThreadContext)
    {
        return;
    }

    if (!ThreadContext->Main)
    {
        Rr_ReleaseCommandPools();
    }

    for (size_t Index = 0; Index < 2; ++Index)
    {
        Rr_DestroyArena(ThreadContext->ScratchArenas[Index]);
    }

    Rr_DestroyArena(ThreadContext->Arena);

    ThreadContext = NULL;
}

Rr_ThreadContext *Rr_GetThreadContext(void)
{
    return ThreadContext;
}

Rr_Scratch Rr_GetScratch(Rr_Arena *Conflict)
{
    assert(ThreadContext && "Did you forget to call Rr_InitThreadContext()?");
    if (Conflict == NULL)
    {
        return Rr_CreateScratch(ThreadContext->ScratchArenas[0]);
    }
    else
    {
        for (size_t Index = 0; Index < 2; ++Index)
        {
            if (ThreadContext->ScratchArenas[Index] != Conflict)
            {
                return Rr_CreateScratch(ThreadContext->ScratchArenas[Index]);
            }
        }
    }

    RR_LOG_ABORT("Couldn't find appropriate arena for a scratch!");

    return (Rr_Scratch){ 0 };
}
