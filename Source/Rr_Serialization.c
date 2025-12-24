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

#include <Rr/Rr_Serialization.h>

#include <Rr/Rr_Pipeline.h>

#include <string.h>

#define RR_ADVANCE_PTR(Amount) \
    *(uintptr_t *)&Ptr_ =      \
        RR_ALIGN_POW2((uintptr_t)Ptr_ + Amount, (size_t)RR_SAFE_ALIGNMENT)

#define RR_SERIALIZE_FIELD(Func, Field, Count)                              \
    if (Original_->Field)                                                   \
    {                                                                       \
        size_t Size;                                                        \
        if (Dst)                                                            \
        {                                                                   \
            Size = Func(Ptr_, Original_->Field, Count);                     \
        }                                                                   \
        else                                                                \
        {                                                                   \
            Size = Func(NULL, Original_->Field, Count);                     \
        }                                                                   \
        if (Copies_)                                                        \
        {                                                                   \
            *(uintptr_t *)&Copy_->Field = (uintptr_t)Ptr_ - (uintptr_t)Dst; \
        }                                                                   \
        RR_ADVANCE_PTR(Size);                                               \
    }                                                                       \
    else                                                                    \
    {                                                                       \
        if (Copies_)                                                        \
            *(uintptr_t *)&Copy_->Field = SIZE_MAX;                         \
    }

#define RR_BEGIN_SERIALIZE_BODY(StructType)            \
    char *Ptr_ = Dst;                                  \
                                                       \
    StructType *Copies_ = NULL;                        \
    if (Ptr_)                                          \
    {                                                  \
        memcpy(Ptr_, Src, sizeof(StructType) * Count); \
        Copies_ = (void *)Ptr_;                        \
    }                                                  \
    RR_ADVANCE_PTR(sizeof(StructType) * Count);        \
                                                       \
    for (size_t Index_ = 0; Index_ < Count; ++Index_)  \
    {                                                  \
        StructType const *Original_ = Src + Index_;    \
        StructType *Copy_ = Copies_ + Index_;

#define RR_END_SERIALIZE_BODY() \
    }                           \
    return (uintptr_t)Ptr_ - (uintptr_t)Dst;

#define RR_DESERIALIZE_FIELD(Func, Field, Count)                             \
    if ((uintptr_t)Struct_->Field == SIZE_MAX)                               \
    {                                                                        \
        Struct_->Field = NULL;                                               \
    }                                                                        \
    else                                                                     \
    {                                                                        \
        void *Ptr = (void *)((uintptr_t)Buffer + (uintptr_t)Struct_->Field); \
        Struct_->Field = Func(Ptr, Count);                                   \
    }

#define RR_BEGIN_DESERIALIZE_BODY(StructType)         \
    StructType *Structs_ = Buffer;                    \
                                                      \
    for (size_t Index_ = 0; Index_ < Count; ++Index_) \
    {                                                 \
        StructType *Struct_ = Structs_ + Index_;

#define RR_END_DESERIALIZE_BODY() \
    }                             \
    return Structs_;

static size_t Rr_SerializeBuffer(void *Dst, void const *Src, size_t Count)
{
    if (Dst)
    {
        memcpy(Dst, Src, Count);
    }
    return RR_ALIGN_POW2(Count, (size_t)RR_SAFE_ALIGNMENT);
}

static void *Rr_DeserializeBuffer(void *Buffer, size_t Count)
{
    return Buffer;
}

/* Rr_PipelineSpecialization */

size_t Rr_SerializePipelineSpecialization(
    void *Dst,
    Rr_PipelineSpecialization const *Src,
    size_t Count)
{
    RR_BEGIN_SERIALIZE_BODY(Rr_PipelineSpecialization);

    RR_SERIALIZE_FIELD(Rr_SerializeBuffer, Data, Original_->Size);

    RR_END_SERIALIZE_BODY();
}

Rr_PipelineSpecialization *Rr_DeserializePipelineSpecialization(
    void *Buffer,
    size_t Count)
{
    RR_BEGIN_DESERIALIZE_BODY(Rr_PipelineSpecialization);

    RR_DESERIALIZE_FIELD(Rr_DeserializeBuffer, Data, 1);

    RR_END_DESERIALIZE_BODY();
}

/* Rr_ShaderInfo */

size_t Rr_SerializeShaderInfo(void *Dst, Rr_ShaderInfo const *Src, size_t Count)
{
    RR_BEGIN_SERIALIZE_BODY(Rr_ShaderInfo);

    RR_SERIALIZE_FIELD(Rr_SerializeBuffer, SPVData, Original_->SPVSize);
    RR_SERIALIZE_FIELD(
        Rr_SerializeBuffer,
        EntryPoint,
        strlen(Original_->EntryPoint) + 1);
    RR_SERIALIZE_FIELD(
        Rr_SerializePipelineSpecialization,
        Specializations,
        Original_->SpecializationCount);

    RR_END_SERIALIZE_BODY();
}

Rr_ShaderInfo *Rr_DeserializeShaderInfo(void *Buffer, size_t Count)
{
    RR_BEGIN_DESERIALIZE_BODY(Rr_ShaderInfo);

    RR_DESERIALIZE_FIELD(Rr_DeserializeBuffer, SPVData, 1);
    RR_DESERIALIZE_FIELD(Rr_DeserializeBuffer, EntryPoint, 1);
    RR_DESERIALIZE_FIELD(
        Rr_DeserializePipelineSpecialization,
        Specializations,
        Struct_->SpecializationCount);

    RR_END_DESERIALIZE_BODY();
}

/* Rr_VertexInputBinding */

size_t Rr_SerializeVertexInputBinding(
    void *Dst,
    Rr_VertexInputBinding const *Src,
    size_t Count)
{
    RR_BEGIN_SERIALIZE_BODY(Rr_VertexInputBinding);

    RR_SERIALIZE_FIELD(
        Rr_SerializeBuffer,
        Attributes,
        Original_->AttributeCount * sizeof(Rr_VertexInputAttribute));

    RR_END_SERIALIZE_BODY();
}

Rr_VertexInputBinding *Rr_DeserializeVertexInputBinding(
    void *Buffer,
    size_t Count)
{
    RR_BEGIN_DESERIALIZE_BODY(Rr_VertexInputBinding);

    RR_DESERIALIZE_FIELD(Rr_DeserializeBuffer, Attributes, 1);

    RR_END_DESERIALIZE_BODY();
}

/* Rr_GraphicsPipelineCreateInfo */

size_t Rr_SerializeGraphicsPipelineCreateInfo(
    void *Dst,
    Rr_GraphicsPipelineCreateInfo const *Src,
    size_t Count)
{
    RR_BEGIN_SERIALIZE_BODY(Rr_GraphicsPipelineCreateInfo);

    RR_SERIALIZE_FIELD(Rr_SerializeShaderInfo, VertexShaderInfo, 1);
    RR_SERIALIZE_FIELD(Rr_SerializeShaderInfo, FragmentShaderInfo, 1);
    RR_SERIALIZE_FIELD(
        Rr_SerializeVertexInputBinding,
        VertexInputBindings,
        Original_->VertexInputBindingCount);
    RR_SERIALIZE_FIELD(
        Rr_SerializeBuffer,
        ColorTargets,
        Original_->ColorTargetCount * sizeof(Rr_ColorTargetInfo));

    RR_END_SERIALIZE_BODY();
}

struct Rr_GraphicsPipelineCreateInfo *Rr_DeserializeGraphicsPipelineCreateInfo(
    void *Buffer,
    size_t Count)
{
    RR_BEGIN_DESERIALIZE_BODY(Rr_GraphicsPipelineCreateInfo);

    RR_DESERIALIZE_FIELD(Rr_DeserializeShaderInfo, VertexShaderInfo, 1);
    RR_DESERIALIZE_FIELD(Rr_DeserializeShaderInfo, FragmentShaderInfo, 1);
    RR_DESERIALIZE_FIELD(
        Rr_DeserializeVertexInputBinding,
        VertexInputBindings,
        Struct_->VertexInputBindingCount);
    RR_DESERIALIZE_FIELD(Rr_DeserializeBuffer, ColorTargets, 1);

    RR_END_DESERIALIZE_BODY();
}
