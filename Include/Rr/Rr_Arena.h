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

#include <Rr/Rr_Defines.h>

typedef struct Rr_Arena Rr_Arena;
struct Rr_Arena
{
    uintptr_t Position;
    uintptr_t ReserveSize;
    uintptr_t CommitSize;
    uintptr_t Reserved;
    uintptr_t Commited;
};

RR_EXTERN Rr_Arena *Rr_CreateArena(size_t Reserve, size_t Commit);

RR_EXTERN Rr_Arena *Rr_CreateDefaultArena(void);

RR_EXTERN void Rr_ResetArena(Rr_Arena *Arena);

RR_EXTERN void Rr_DestroyArena(Rr_Arena *Arena);

RR_EXTERN void *Rr_AllocNoZero(
    size_t Size,
    size_t Align,
    size_t Count,
    Rr_Arena *Arena);

RR_EXTERN void *Rr_Alloc(
    size_t Size,
    size_t Align,
    size_t Count,
    Rr_Arena *Arena);

#define RR_ALLOC(Size, Arena) Rr_Alloc(Size, RR_SAFE_ALIGNMENT, 1, Arena)

#define RR_ALLOC_NO_ZERO(Size, Arena) \
    Rr_AllocNoZero(Size, RR_SAFE_ALIGNMENT, 1, Arena)

#define RR_ALLOC_TYPE(Type, Arena) \
    (Type *)Rr_Alloc(sizeof(Type), RR_SAFE_ALIGNMENT, 1, Arena)

#define RR_ALLOC_TYPE_COUNT(Type, Count, Arena) \
    (Type *)Rr_Alloc(sizeof(Type), RR_SAFE_ALIGNMENT, Count, Arena)

#define RR_ALLOC_COPY(Src, Size, Arena) \
    (memcpy(RR_ALLOC_NO_ZERO(Size, Arena), Src, Size))

/*
 * Scratch Arena
 */

typedef struct Rr_Scratch Rr_Scratch;
struct Rr_Scratch
{
    Rr_Arena *Arena;
    uintptr_t Position;
};

RR_EXTERN Rr_Scratch Rr_CreateScratch(Rr_Arena *Arena);

RR_EXTERN void Rr_DestroyScratch(Rr_Scratch Scratch);

RR_EXTERN void Rr_InitScratchArena(void);

RR_EXTERN void Rr_CleanupScratchArena(void);

RR_EXTERN Rr_Scratch Rr_GetScratch(Rr_Arena *Conflict);

#endif