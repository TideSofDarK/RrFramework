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

#ifndef RR_SERIALIZATION_H
#define RR_SERIALIZATION_H

#include <Rr/Rr_Defines.h>

struct Rr_PipelineSpecialization;
struct Rr_ShaderInfo;
struct Rr_GraphicsPipelineCreateInfo;

RR_EXTERN size_t Rr_SerializePipelineSpecialization(
    void *Dst,
    struct Rr_PipelineSpecialization const *Src,
    size_t Count);

RR_EXTERN struct Rr_PipelineSpecialization *
Rr_DeserializePipelineSpecialization(void *Buffer, size_t Count);

RR_EXTERN size_t Rr_SerializeShaderInfo(
    void *Dst,
    struct Rr_ShaderInfo const *Src,
    size_t Count);

RR_EXTERN struct Rr_ShaderInfo *Rr_DeserializeShaderInfo(
    void *Buffer,
    size_t Count);

RR_EXTERN size_t Rr_SerializeGraphicsPipelineCreateInfo(
    void *Dst,
    struct Rr_GraphicsPipelineCreateInfo const *Src,
    size_t Count);

RR_EXTERN struct Rr_GraphicsPipelineCreateInfo *
Rr_DeserializeGraphicsPipelineCreateInfo(void *Buffer, size_t Count);

#endif
