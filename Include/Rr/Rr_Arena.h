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

#include <Rr/Rr_Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_Arena Rr_Arena;
struct Rr_Arena
{
    uintptr_t Position;
    uintptr_t ReserveSize;
    uintptr_t CommitSize;
    uintptr_t Reserved;
    uintptr_t Commited;
};

extern Rr_Arena *Rr_CreateArena(size_t Reserve, size_t Commit);

extern Rr_Arena *Rr_CreateDefaultArena(void);

extern void Rr_ResetArena(Rr_Arena *Arena);

extern void Rr_DestroyArena(Rr_Arena *Arena);

extern void *Rr_AllocArenaNoZero(
    Rr_Arena *Arena,
    size_t Size,
    size_t Align,
    size_t Count);

extern void *Rr_AllocArena(
    Rr_Arena *Arena,
    size_t Size,
    size_t Align,
    size_t Count);

#define RR_ALLOC(Arena, Size) Rr_AllocArena(Arena, Size, RR_SAFE_ALIGNMENT, 1)
#define RR_ALLOC_NO_ZERO(Arena, Size) \
    Rr_AllocArenaNoZero(Arena, Size, RR_SAFE_ALIGNMENT, 1)
#define RR_ALLOC_TYPE(Arena, Type) \
    (Type *)Rr_AllocArena(Arena, sizeof(Type), RR_SAFE_ALIGNMENT, 1)
#define RR_ALLOC_TYPE_COUNT(Arena, Type, Count) \
    (Type *)Rr_AllocArena(Arena, sizeof(Type), RR_SAFE_ALIGNMENT, Count)
#define RR_ALLOC_COPY(Arena, Src, Size) \
    (memcpy(RR_ALLOC_NO_ZERO(Arena, Size), Src, Size))

extern void Rr_PopArena(Rr_Arena *Arena, size_t Amount);

/*
 * Scratch Arena
 */

typedef struct Rr_Scratch Rr_Scratch;
struct Rr_Scratch
{
    Rr_Arena *Arena;
    uintptr_t Position;
};

extern Rr_Scratch Rr_CreateScratch(Rr_Arena *Arena);

extern void Rr_DestroyScratch(Rr_Scratch Scratch);

extern void Rr_InitScratchArena(void);

extern void Rr_CleanupScratchArena(void);

extern Rr_Scratch Rr_GetScratch(Rr_Arena *Conflict);

#ifdef __cplusplus
}
#endif
