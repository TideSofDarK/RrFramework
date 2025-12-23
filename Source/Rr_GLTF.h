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

#include <Rr/Rr_GLTF.h>

#include <Rr/Rr_Asset.h>

#include "Rr_Memory.h"
#include "Rr_Platform.h"

struct Rr_UploadContext;
struct Rr_Buffer;

struct Rr_GLTFContext
{
    Rr_Renderer *Renderer;

    RR_ARRAY(struct Rr_Buffer *) Buffers;
    RR_ARRAY(struct Rr_Image *) Images;

    size_t VertexInputBindingCount;
    Rr_GLTFVertexInputBinding *VertexInputBindings;
    size_t *VertexInputStrides;

    size_t TextureMappingCount;
    Rr_GLTFTextureMapping *TextureMappings;

    Rr_Spinlock Lock;
    Rr_Arena *Arena;
};
