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

#include <Rr/Rr_Asset.h>

#if defined(RR_USE_RC)

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

Rr_Asset Rr_LoadAsset(Rr_AssetRef AssetRef)
{
    HRSRC Resource = FindResource(NULL, AssetRef.Name, "RRDATA");
    HGLOBAL Memory = LoadResource(NULL, Resource);

    Rr_Asset Asset;
    Asset.Size = SizeofResource(NULL, Resource);
    Asset.Pointer = (char *)LockResource(Memory);
    return Asset;
}

#else

Rr_Asset Rr_LoadAsset(Rr_AssetRef AssetRef)
{
    Rr_Asset Asset = {
        .Size = (size_t)(AssetRef.End - AssetRef.Start),
        .Pointer = AssetRef.Start,
    };
    return Asset;
}

#endif
