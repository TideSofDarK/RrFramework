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

#ifndef RR_ARENA_H
#define RR_ARENA_H

#include <Rr/Rr_Common.h>

typedef struct Rr_Arena Rr_Arena;
typedef struct Rr_TSArena Rr_TSArena;

typedef struct Rr_Scratch Rr_Scratch;
struct Rr_Scratch
{
    Rr_Arena *Arena;
    uintptr_t Position;
};

#ifdef __cplusplus
extern "C" {
#endif

extern Rr_Arena *RR_CC Rr_CreateArena(size_t Reserve, size_t Commit);

extern Rr_Arena *RR_CC Rr_CreateDefaultArena(void);

extern void RR_CC Rr_ResetArena(Rr_Arena *Arena);

extern void RR_CC Rr_DestroyArena(Rr_Arena *Arena);

extern void *RR_CC
Rr_AllocAlignedNoZero(size_t Size, size_t Align, Rr_Arena *Arena);

extern void *RR_CC Rr_AllocAligned(size_t Size, size_t Align, Rr_Arena *Arena);

extern void *RR_CC Rr_AllocNoZero(size_t Size, Rr_Arena *Arena);

extern void *RR_CC Rr_Alloc(size_t Size, Rr_Arena *Arena);

extern void *RR_CC
Rr_AllocCopy(void const *Source, size_t Size, Rr_Arena *Arena);

extern Rr_TSArena *RR_CC Rr_CreateTSArena(size_t Reserve, size_t Commit);

extern void RR_CC Rr_DestroyTSArena(Rr_TSArena *Arena);

extern void *RR_CC
Rr_TSAllocAlignedNoZero(size_t Size, size_t Align, Rr_TSArena *Arena);

extern void *RR_CC Rr_TSAlloc(size_t Size, Rr_TSArena *Arena);

extern Rr_Scratch RR_CC Rr_CreateScratch(Rr_Arena *Arena);

extern void RR_CC Rr_DestroyScratch(Rr_Scratch Scratch);

#ifdef __cplusplus
}
#endif

#endif
