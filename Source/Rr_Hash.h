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

#pragma once

#if defined(__x86_64__) || defined(__i386__) || defined(_M_IX86) || defined(_M_X64)
#include <xxHash/xxh_x86dispatch.h>
#else
#include <xxHash/xxhash.h>
#endif

static inline uint64_t Rr_Hash64(size_t Size, void const *Data)
{
    return XXH3_64bits(Data, Size);
}

static inline uint64_t Rr_Hash64WithSeed(size_t Size, void const *Data, size_t Seed)
{
    return XXH3_64bits_withSeed(Data, Size, Seed);
}
