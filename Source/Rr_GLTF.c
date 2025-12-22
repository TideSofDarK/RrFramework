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

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "Rr_GLTF.h"

#include "Rr_Graph.h"
#include "Rr_Image.h"
#include "Rr_Log.h"

#include <stb/stb_image.h>

#define CGLTF_IMPLEMENTATION
#include <cgltf/cgltf.h>

#include <assert.h>
#include <string.h>

static inline Rr_IndexType Rr_CGLTFComponentTypeToIndexType(
    cgltf_component_type Type)
{
    switch (Type)
    {
        case cgltf_component_type_r_16u:
            return RR_INDEX_TYPE_UINT16;
        case cgltf_component_type_r_32u:
            return RR_INDEX_TYPE_UINT32;
        default:
            RR_ABORT("cGLTF: Unsupported index type!");
    }
}

static inline size_t Rr_GetGLTFAttributeSize(Rr_GLTFAttributeType Type)
{
    switch (Type)
    {
        case RR_GLTF_ATTRIBUTE_TYPE_TEXCOORD0:
            return 8;
        case RR_GLTF_ATTRIBUTE_TYPE_POSITION:
        case RR_GLTF_ATTRIBUTE_TYPE_NORMAL:
        case RR_GLTF_ATTRIBUTE_TYPE_COLOR:
        case RR_GLTF_ATTRIBUTE_TYPE_TANGENT:
            return 12;
        default:
            return cgltf_attribute_type_invalid;
    }
}

static inline cgltf_attribute_type Rr_GetCGLTFAttributeType(
    Rr_GLTFAttributeType Type)
{
    switch (Type)
    {
        case RR_GLTF_ATTRIBUTE_TYPE_POSITION:
            return cgltf_attribute_type_position;
        case RR_GLTF_ATTRIBUTE_TYPE_TEXCOORD0:
            return cgltf_attribute_type_texcoord;
        case RR_GLTF_ATTRIBUTE_TYPE_NORMAL:
            return cgltf_attribute_type_normal;
        case RR_GLTF_ATTRIBUTE_TYPE_COLOR:
            return cgltf_attribute_type_color;
        case RR_GLTF_ATTRIBUTE_TYPE_TANGENT:
            return cgltf_attribute_type_tangent;
        default:
            return cgltf_attribute_type_invalid;
    }
}

static inline Rr_GLTFAttributeType Rr_GetGLTFAttributeType(
    cgltf_attribute_type Type)
{
    switch (Type)
    {
        case cgltf_attribute_type_position:
            return RR_GLTF_ATTRIBUTE_TYPE_POSITION;
        case cgltf_attribute_type_texcoord:
            return RR_GLTF_ATTRIBUTE_TYPE_TEXCOORD0;
        case cgltf_attribute_type_normal:
            return RR_GLTF_ATTRIBUTE_TYPE_NORMAL;
        case cgltf_attribute_type_color:
            return RR_GLTF_ATTRIBUTE_TYPE_COLOR;
        case cgltf_attribute_type_tangent:
            return RR_GLTF_ATTRIBUTE_TYPE_TANGENT;
        default:
            return RR_GLTF_ATTRIBUTE_TYPE_INVALID;
    }
}

static inline Rr_Format Rr_GLTFAttributeTypeToFormat(Rr_GLTFAttributeType Type)
{
    switch (Type)
    {
        case RR_GLTF_ATTRIBUTE_TYPE_TEXCOORD0:
            return RR_FORMAT_VEC2;
        case RR_GLTF_ATTRIBUTE_TYPE_POSITION:
        case RR_GLTF_ATTRIBUTE_TYPE_NORMAL:
        case RR_GLTF_ATTRIBUTE_TYPE_COLOR:
        case RR_GLTF_ATTRIBUTE_TYPE_TANGENT:
            return RR_FORMAT_VEC3;
        default:
            RR_ABORT("Invalid GLTF attribute type!");
    }
}

Rr_GLTFContext *Rr_CreateGLTFContext(
    size_t VertexInputBindingCount,
    const Rr_VertexInputBinding *VertexInputBindings,
    const Rr_GLTFVertexInputBinding *GLTFVertexInputBindings,
    size_t GLTFTextureMappingCount,
    const Rr_GLTFTextureMapping *GLTFTextureMappings)
{
    assert(VertexInputBindingCount != 0);
    assert(VertexInputBindings != NULL);
    assert(GLTFVertexInputBindings != NULL);
#if defined(RR_DEBUG)
    for (size_t BindingIndex = 0; BindingIndex < VertexInputBindingCount;
         ++BindingIndex)
    {
        const Rr_VertexInputBinding *VertexInputBinding =
            VertexInputBindings + BindingIndex;
        const Rr_GLTFVertexInputBinding *GLTFVertexInputBinding =
            GLTFVertexInputBindings + BindingIndex;
        assert(
            VertexInputBinding->AttributeCount ==
            GLTFVertexInputBinding->AttributeTypeCount);
        for (size_t AttributeIndex = 0;
             AttributeIndex < VertexInputBinding->AttributeCount;
             ++AttributeIndex)
        {
            const Rr_VertexInputAttribute *Attribute =
                VertexInputBinding->Attributes + AttributeIndex;
            Rr_GLTFAttributeType GLTFAttributeType =
                GLTFVertexInputBinding->AttributeTypes[AttributeIndex];
            assert(
                Attribute->Format ==
                Rr_GLTFAttributeTypeToFormat(GLTFAttributeType));
        }
    }
#endif

    Rr_Arena *Arena = Rr_CreateDefaultArena();

    Rr_GLTFContext *GLTFContext = RR_ALLOC_TYPE(Rr_GLTFContext, Arena);
    GLTFContext->Arena = Arena;

    GLTFContext->VertexInputStrides =
        RR_ALLOC_TYPE_COUNT(size_t, VertexInputBindingCount, Arena);
    GLTFContext->VertexInputBindingCount = VertexInputBindingCount;
    GLTFContext->VertexInputBindings = RR_ALLOC_COPY(
        GLTFVertexInputBindings,
        sizeof(Rr_GLTFVertexInputBinding) * VertexInputBindingCount,
        Arena);
    for (size_t BindingIndex = 0; BindingIndex < VertexInputBindingCount;
         ++BindingIndex)
    {
        Rr_GLTFVertexInputBinding *GLTFVertexInputBinding =
            GLTFContext->VertexInputBindings + BindingIndex;
        GLTFVertexInputBinding->AttributeTypes = RR_ALLOC_COPY(
            GLTFVertexInputBindings[BindingIndex].AttributeTypes,
            sizeof(Rr_GLTFAttributeType) *
                GLTFVertexInputBindings[BindingIndex].AttributeTypeCount,
            Arena);

        for (size_t AttributeIndex = 0;
             AttributeIndex < GLTFVertexInputBinding->AttributeTypeCount;
             ++AttributeIndex)
        {
            GLTFContext->VertexInputStrides[BindingIndex] +=
                Rr_GetGLTFAttributeSize(
                    GLTFVertexInputBinding->AttributeTypes[AttributeIndex]);
        }
    }

    if (GLTFTextureMappingCount > 0)
    {
        GLTFContext->TextureMappings = RR_ALLOC_COPY(
            GLTFTextureMappings,
            sizeof(Rr_GLTFTextureMapping) * GLTFTextureMappingCount,
            Arena);
    }

    return GLTFContext;
}

void Rr_ReleaseGLTFContext(Rr_GLTFContext *GLTFContext)
{
    for (size_t Index = 0; Index < GLTFContext->Buffers.Count; ++Index)
    {
        Rr_ReleaseBuffer(GLTFContext->Buffers.Data[Index]);
    }

    for (size_t Index = 0; Index < GLTFContext->Images.Count; ++Index)
    {
        Rr_ReleaseImage(GLTFContext->Images.Data[Index]);
    }

    Rr_DestroyArena(GLTFContext->Arena);
}

// static void Rr_CalculateTangents(size_t IndexCount, const Rr_MeshIndexType
// *Indices, Rr_Vertex *OutVertices)
// {
//     for(uint32_t Index = 3; Index < IndexCount; Index += 3)
//     {
//         uint32_t V0Index = Indices[Index - 3];
//         uint32_t V1Index = Indices[Index - 2];
//         uint32_t V2Index = Indices[Index - 1];
//
//         Rr_Vertex *Vertex0 = &OutVertices[V0Index];
//         Rr_Vertex *Vertex1 = &OutVertices[V1Index];
//         Rr_Vertex *Vertex2 = &OutVertices[V2Index];
//
//         Rr_Vec3 Tangent = Rr_V3(0.0f, 0.0f, 0.0f);
//         Rr_Vec3 Edge0 = Rr_SubV3(Vertex1->Position, Vertex0->Position);
//         Rr_Vec3 Edge1 = Rr_SubV3(Vertex2->Position, Vertex0->Position);
//         Rr_Vec2 DeltaUV0 = { Vertex1->TexCoordX - Vertex0->TexCoordX,
//         Vertex1->TexCoordY - Vertex0->TexCoordY }; Rr_Vec2 DeltaUV1 = {
//         Vertex2->TexCoordX - Vertex0->TexCoordX, Vertex2->TexCoordY -
//         Vertex0->TexCoordY };
//
//         float Denominator = DeltaUV0.X * DeltaUV1.Y - DeltaUV1.X *
//         DeltaUV0.Y; if(fabsf(Denominator) <= 0.0000001f)
//         {
//             Tangent = Rr_SubV3(Vertex1->Position, Vertex2->Position);
//         }
//         else
//         {
//             float F = 1.0f / Denominator;
//
//             Tangent.X = F * (DeltaUV1.Y * Edge0.X - DeltaUV0.Y * Edge1.X);
//             Tangent.Y = F * (DeltaUV1.Y * Edge0.Y - DeltaUV0.Y * Edge1.Y);
//             Tangent.Z = F * (DeltaUV1.Y * Edge0.Z - DeltaUV0.Y * Edge1.Z);
//         }
//
//         Vertex0->Tangent = Rr_AddV3(Vertex0->Tangent, Tangent);
//         Vertex1->Tangent = Rr_AddV3(Vertex1->Tangent, Tangent);
//         Vertex2->Tangent = Rr_AddV3(Vertex2->Tangent, Tangent);
//     }
// }

static inline void *Rr_GetCGLTFAccessorValueAt(
    cgltf_accessor *Accessor,
    size_t Index)
{
    char *Buffer = Accessor->buffer_view->buffer->data;
    char *BufferView = Buffer + Accessor->buffer_view->offset;
    return BufferView + Accessor->offset + (Accessor->stride * Index);
}

typedef struct Rr_GLTFVertexInputInfo Rr_GLTFVertexInputInfo;
struct Rr_GLTFVertexInputInfo
{
    size_t Binding;
    size_t Offset;
    size_t Stride;
};

static inline bool Rr_GetGLTFVertexInputInfoForAttribute(
    Rr_GLTFContext *Context,
    cgltf_attribute_type AttributeType,
    Rr_GLTFVertexInputInfo *Out)
{
    bool Found = false;
    for (size_t BindingIndex = 0;
         BindingIndex < Context->VertexInputBindingCount;
         ++BindingIndex)
    {
        Rr_GLTFVertexInputBinding *Binding =
            Context->VertexInputBindings + BindingIndex;
        size_t Offset = 0;
        for (size_t Index = 0; Index < Binding->AttributeTypeCount; ++Index)
        {
            Rr_GLTFAttributeType Type = Binding->AttributeTypes[Index];
            if (Rr_GetCGLTFAttributeType(Type) == AttributeType)
            {
                if (Found)
                {
                    RR_ABORT(
                        "GLTF: Multiple mappings found for the same "
                        "attribute type!");
                }
                if (Out)
                {
                    Out->Binding = BindingIndex;
                    Out->Offset = Offset;
                }
                Found = true;
            }
            Offset += Rr_GetGLTFAttributeSize(Type);
        }
        if (Found)
        {
            if (Out)
            {
                Out->Stride = Offset;
            }
            return true;
        }
    }

    return false;
}

static inline size_t Rr_GetStagingSizeForVertexCount(
    Rr_GLTFContext *Context,
    size_t VertexCount)
{
    size_t StagingSize = 0;
    for (size_t BindingIndex = 0;
         BindingIndex < Context->VertexInputBindingCount;
         ++BindingIndex)
    {
        StagingSize += Context->VertexInputStrides[BindingIndex] * VertexCount;
    }
    return StagingSize;
}

static inline size_t Rr_GetFlatBindingOffset(
    Rr_GLTFContext *Context,
    size_t BindingIndex,
    size_t VertexCount)
{
    size_t Result = 0;
    for (size_t Index = 0; Index < BindingIndex; ++Index)
    {
        Result += Context->VertexInputStrides[Index] * VertexCount;
    }
    return Result;
}

static void *Rr_GenericArenaAlloc(void *Arena, size_t Size)
{
    return RR_ALLOC_NO_ZERO(Size, (Rr_Arena *)Arena);
}

static void Rr_GenericArenaFree(void *Arena, void *Ptr)
{
}

Rr_GLTFAsset *Rr_CreateGLTFAsset(
    Rr_GLTFContext *GLTFContext,
    struct Rr_Graph *Graph,
    size_t GLBDataSize,
    const void *GLBData)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    cgltf_options Options = {
        .memory =
            (cgltf_memory_options){
                .alloc_func = Rr_GenericArenaAlloc,
                .free_func = Rr_GenericArenaFree,
                .user_data = Scratch.Arena,
            },
    };
    cgltf_data *Data = NULL;
    cgltf_result Result = cgltf_parse(&Options, GLBData, GLBDataSize, &Data);
    if (Result != cgltf_result_success)
    {
        RR_ABORT("GLTF: Parsing failed!");
    }
    cgltf_load_buffers(&Options, Data, NULL);

    Rr_GLTFAsset *GLTFAsset = RR_ALLOC_TYPE(Rr_GLTFAsset, GLTFContext->Arena);

    /* Create staging structures. */

    size_t StagingDataSize = 0;
    size_t VertexDataOffset = 0;
    size_t VertexDataSize = 0;
    size_t IndexDataSize = 0;
    size_t MaxIndexSize = 0;
    size_t TotalIndexCount = 0;

    /* Calculate how much memory is needed for vertices and indices. */

    for (size_t MeshIndex = 0; MeshIndex < Data->meshes_count; ++MeshIndex)
    {
        cgltf_mesh *Mesh = Data->meshes + MeshIndex;
        for (size_t PrimitiveIndex = 0; PrimitiveIndex < Mesh->primitives_count;
             ++PrimitiveIndex)
        {
            cgltf_primitive *Primitive = Mesh->primitives + PrimitiveIndex;
            assert(Primitive->attributes_count > 0);

            size_t VertexCount = Primitive->attributes->data->count;

            /* Quick check to make sure every attribute has the same count. */

            for (size_t AttributeIndex = 1;
                 AttributeIndex < Primitive->attributes_count;
                 ++AttributeIndex)
            {
                cgltf_attribute *Attribute =
                    Primitive->attributes + AttributeIndex;
                if (VertexCount != Attribute->data->count)
                {
                    RR_ABORT("GLTF: Attributes with different counts!");
                }
            }

            VertexDataSize +=
                Rr_GetStagingSizeForVertexCount(GLTFContext, VertexCount);

            MaxIndexSize = RR_MAX(
                cgltf_calc_size(
                    Primitive->indices->type,
                    Primitive->indices->component_type),
                MaxIndexSize);
            TotalIndexCount += Primitive->indices->count;
        }
    }
    IndexDataSize = TotalIndexCount * MaxIndexSize;
    if (MaxIndexSize == 1)
    {
        GLTFAsset->IndexType = RR_INDEX_TYPE_UINT8;
    }
    else if (MaxIndexSize == 2)
    {
        GLTFAsset->IndexType = RR_INDEX_TYPE_UINT16;
    }
    else if (MaxIndexSize == 4)
    {
        GLTFAsset->IndexType = RR_INDEX_TYPE_UINT32;
    }
    else
    {
        RR_ABORT("GLTF: Unsupported index type!");
    }

    /* @TODO: The fuck is this? */
    static const size_t SafeAlignment = 64;
    GLTFAsset->VertexBufferOffset = 0;
    GLTFAsset->IndexBufferOffset = RR_ALIGN_POW2(VertexDataSize, SafeAlignment);
    StagingDataSize = GLTFAsset->IndexBufferOffset +
                      RR_ALIGN_POW2(IndexDataSize, SafeAlignment);

    Rr_Buffer *StagingBuffer = Rr_CreateBuffer(
        StagingDataSize,
        RR_BUFFER_FLAGS_STAGING_INCOHERENT_BIT | RR_BUFFER_FLAGS_MAPPED_BIT);
    Rr_ReleaseBuffer(StagingBuffer);
    char *StagingData = Rr_GetMappedBufferData(StagingBuffer);

    /* Preallocate materials. */

    if (Data->materials)
    {
        GLTFAsset->Materials = RR_ALLOC_TYPE_COUNT(
            Rr_GLTFMaterial,
            Data->materials_count,
            GLTFContext->Arena);
    }

    if (Data->images)
    {
        GLTFAsset->Images = RR_ALLOC_TYPE_COUNT(
            Rr_Image2D *,
            Data->images_count,
            GLTFContext->Arena);
    }

    /* Process meshes. */

    GLTFAsset->MeshCount = Data->meshes_count;
    GLTFAsset->Meshes = RR_ALLOC_TYPE_COUNT(
        Rr_GLTFMesh,
        GLTFAsset->MeshCount,
        GLTFContext->Arena);
    size_t FirstIndex = 0;
    size_t VertexOffset = 0;
    for (size_t MeshIndex = 0; MeshIndex < Data->meshes_count; ++MeshIndex)
    {
        cgltf_mesh *Mesh = Data->meshes + MeshIndex;

        Rr_GLTFMesh *GLTFMesh = GLTFAsset->Meshes + MeshIndex;
        GLTFMesh->PrimitiveCount = Mesh->primitives_count;
        GLTFMesh->Primitives = RR_ALLOC_TYPE_COUNT(
            Rr_GLTFPrimitive,
            GLTFMesh->PrimitiveCount,
            GLTFContext->Arena);

        if (Mesh->name)
        {
            GLTFMesh->Name = RR_ALLOC_COPY(
                Mesh->name,
                strlen(Mesh->name),
                GLTFContext->Arena);
        }

        for (size_t PrimitiveIndex = 0;
             PrimitiveIndex < GLTFMesh->PrimitiveCount;
             ++PrimitiveIndex)
        {
            cgltf_primitive *Primitive = Mesh->primitives + PrimitiveIndex;

            Rr_GLTFPrimitive *GLTFPrimitive =
                GLTFMesh->Primitives + PrimitiveIndex;
            GLTFPrimitive->AttributeCount = Primitive->attributes_count;
            GLTFPrimitive->Attributes = RR_ALLOC_TYPE_COUNT(
                Rr_GLTFAttribute,
                GLTFPrimitive->AttributeCount,
                GLTFContext->Arena);

            if (Primitive->material)
            {
                GLTFPrimitive->Material =
                    GLTFAsset->Materials +
                    cgltf_material_index(Data, Primitive->material);
            }

            size_t VertexCount = Primitive->attributes->data->count;

            GLTFPrimitive->VertexCount = (uint32_t)VertexCount;
            GLTFPrimitive->IndexCount = (uint32_t)Primitive->indices->count;
            GLTFPrimitive->FirstIndex = (uint32_t)FirstIndex;
            GLTFPrimitive->VertexOffset = (int32_t)VertexOffset;

            for (size_t AttributeIndex = 0;
                 AttributeIndex < GLTFPrimitive->AttributeCount;
                 ++AttributeIndex)
            {
                cgltf_attribute *Attribute =
                    Primitive->attributes + AttributeIndex;
                assert(Attribute->data->is_sparse == false);

                Rr_GLTFAttribute *GLTFAttribute =
                    GLTFPrimitive->Attributes + AttributeIndex;
                GLTFAttribute->Type = Rr_GetGLTFAttributeType(Attribute->type);
                assert(GLTFAttribute->Type != RR_GLTF_ATTRIBUTE_TYPE_INVALID);

                Rr_GLTFVertexInputInfo Info = { 0 };
                bool Found = Rr_GetGLTFVertexInputInfoForAttribute(
                    GLTFContext,
                    Attribute->type,
                    &Info);
                if (Found)
                {
                    /* Copy attribute values to staging data. */

                    size_t AttributeSize = cgltf_calc_size(
                        Attribute->data->type,
                        Attribute->data->component_type);

                    size_t BindingOffset = Rr_GetFlatBindingOffset(
                        GLTFContext,
                        Info.Binding,
                        VertexCount);

                    char *DstBase =
                        (char *)StagingData + VertexDataOffset + BindingOffset;

                    for (size_t VertexIndex = 0; VertexIndex < VertexCount;
                         ++VertexIndex)
                    {
                        void *Dst =
                            DstBase + Info.Offset + (Info.Stride * VertexIndex);
                        void *Src = Rr_GetCGLTFAccessorValueAt(
                            Attribute->data,
                            VertexIndex);
                        memcpy(Dst, Src, AttributeSize);
                    }
                }
            }

            VertexDataOffset += Rr_GetFlatBindingOffset(
                GLTFContext,
                GLTFContext->VertexInputBindingCount,
                VertexCount);

            size_t IndexSize =
                cgltf_component_size(Primitive->indices->component_type);
            char *DstBase = (char *)StagingData + GLTFAsset->IndexBufferOffset +
                            (FirstIndex * MaxIndexSize);
            for (size_t IndexIndex = 0; IndexIndex < GLTFPrimitive->IndexCount;
                 ++IndexIndex)
            {
                void *Dst = DstBase + (MaxIndexSize * IndexIndex);
                void *Src =
                    Rr_GetCGLTFAccessorValueAt(Primitive->indices, IndexIndex);
                memcpy(Dst, Src, IndexSize);
            }

            /* cgltf_accessor_unpack_indices(
                Primitive->indices,
                (char *)StagingData + GLTFAsset->IndexBufferOffset +
                    (FirstIndex * MaxIndexSize),
                MaxIndexSize,
                GLTFPrimitive->IndexCount); */

            FirstIndex += GLTFPrimitive->IndexCount;
            VertexOffset += VertexCount;
        }
    }

    // assert(StagingDataOffset == GeometryDataSize);

    Rr_FlushBufferRange(StagingBuffer, 0, StagingDataSize);

    GLTFAsset->Buffer = Rr_CreateBuffer(
        StagingDataSize,
        RR_BUFFER_FLAGS_INDEX_BIT | RR_BUFFER_FLAGS_VERTEX_BIT);
    *RR_PUSH_INTO_ARRAY(&GLTFContext->Buffers, GLTFContext->Arena) =
        GLTFAsset->Buffer;

    Rr_TransferNode *TransferNode = Rr_AddTransferNode(Graph);
    Rr_TransferBufferData(
        TransferNode,
        StagingDataSize,
        StagingBuffer,
        0,
        GLTFAsset->Buffer,
        0);

    /* Process materials, textures and images. */

    if (Data->materials != NULL)
    {
        for (size_t MaterialIndex = 0; MaterialIndex < Data->materials_count;
             ++MaterialIndex)
        {
            cgltf_material *Material = Data->materials + MaterialIndex;
            if (Material == NULL)
            {
                continue;
            }

            size_t TextureCount = 0;

            if (Material->has_pbr_metallic_roughness &&
                Material->pbr_metallic_roughness.base_color_texture.texture !=
                    NULL)
            {
                TextureCount++;
            }

            Rr_GLTFMaterial *GLTFMaterial =
                GLTFAsset->Materials + MaterialIndex;
            GLTFMaterial->TextureCount = TextureCount;
            GLTFMaterial->Textures =
                RR_ALLOC_TYPE_COUNT(size_t, TextureCount, GLTFContext->Arena);
            GLTFMaterial->TextureTypes = RR_ALLOC_TYPE_COUNT(
                Rr_GLTFTextureType,
                TextureCount,
                GLTFContext->Arena);

            size_t CurrentTextureIndex = 0;

            if (Material->has_pbr_metallic_roughness &&
                Material->pbr_metallic_roughness.base_color_texture.texture !=
                    NULL)
            {
                cgltf_texture *Texture =
                    Material->pbr_metallic_roughness.base_color_texture.texture;
                if (strcmp(Texture->image->mime_type, "image/png") == 0 ||
                    strcmp(Texture->image->mime_type, "image/jpeg") == 0)
                {
                    GLTFMaterial->TextureTypes[CurrentTextureIndex] =
                        RR_GLTF_TEXTURE_TYPE_COLOR;
                    if (GLTFAsset->Images[CurrentTextureIndex] == NULL)
                    {
                        size_t ImageDataSize =
                            (size_t)Texture->image->buffer_view->size;
                        char *ImageData =
                            (char *)Texture->image->buffer_view->buffer->data +
                            Texture->image->buffer_view->offset;

                        GLTFAsset->Images[CurrentTextureIndex] =
                            Rr_CreateSTBImage2D(
                                Graph,
                                RR_IMAGE_FORMAT_R8G8B8A8_SRGB,
                                ImageDataSize,
                                ImageData);

                        *RR_PUSH_INTO_ARRAY(
                            &GLTFContext->Images,
                            GLTFContext->Arena) =
                            GLTFAsset->Images[CurrentTextureIndex];
                    }
                    CurrentTextureIndex++;
                }
            }
        }
    }

    /* Process nodes. */

    GLTFAsset->NodeCount = Data->nodes_count;
    GLTFAsset->Nodes = RR_ALLOC_TYPE_COUNT(
        Rr_GLTFNode,
        GLTFAsset->NodeCount,
        GLTFContext->Arena);
    for (uint32_t NodeIndex = 0; NodeIndex < GLTFAsset->NodeCount; ++NodeIndex)
    {
        cgltf_node *Node = &Data->nodes[NodeIndex];
        Rr_GLTFNode *GLTFNode = &GLTFAsset->Nodes[NodeIndex];

        size_t NameLength = strlen(Node->name);
        GLTFNode->Name = RR_ALLOC_NO_ZERO(NameLength + 1, GLTFContext->Arena);
        strcpy(GLTFNode->Name, Node->name);

        if (Node->has_matrix)
        {
            memcpy(&GLTFNode->Transform, Node->matrix, sizeof(Rr_Mat4));
        }
        else
        {
            Rr_Mat4 Transform = Rr_M4D(1.0f);
            if (Node->has_scale)
            {
                Rr_Vec3 Scale;
                memcpy(&Scale, Node->scale, sizeof(Rr_Vec3));
                Transform = Rr_MulM4(Rr_Scale(Scale), Transform);
            }
            if (Node->has_rotation)
            {
                Rr_Quat Quat;
                memcpy(&Quat, Node->rotation, sizeof(Rr_Quat));
                Transform = Rr_MulM4(Rr_QToM4(Quat), Transform);
            }
            if (Node->has_translation)
            {
                Rr_Vec3 Translation;
                memcpy(&Translation, Node->translation, sizeof(Rr_Vec3));
                Transform = Rr_MulM4(Rr_Translate(Translation), Transform);
            }
            GLTFNode->Transform = Transform;
        }

        if (Node->mesh)
        {
            size_t MeshIndex = cgltf_mesh_index(Data, Node->mesh);
            GLTFNode->Mesh = &GLTFAsset->Meshes[MeshIndex];
        }

        if (Node->parent)
        {
            size_t ParentIndex = cgltf_node_index(Data, Node->parent);
            GLTFNode->Parent = &GLTFAsset->Nodes[ParentIndex];
        }

        if (Node->children)
        {
            GLTFNode->ChildrenCount = Node->children_count;
            GLTFNode->Children = RR_ALLOC_TYPE_COUNT(
                Rr_GLTFNode *,
                Node->children_count,
                GLTFContext->Arena);
            for (uint32_t ChildIndex = 0; ChildIndex < Node->children_count;
                 ++ChildIndex)
            {
                GLTFNode->Children[ChildIndex] =
                    &GLTFAsset->Nodes[cgltf_node_index(
                        Data,
                        Node->children[ChildIndex])];
            }
        }
    }

    /* Process scenes. */

    GLTFAsset->SceneCount = Data->scenes_count;
    GLTFAsset->Scenes = RR_ALLOC_TYPE_COUNT(
        Rr_GLTFScene,
        GLTFAsset->SceneCount,
        GLTFContext->Arena);
    for (uint32_t SceneIndex = 0; SceneIndex < Data->scenes_count; ++SceneIndex)
    {
        cgltf_scene *Scene = &Data->scenes[SceneIndex];
        Rr_GLTFScene *GLTFScene = &GLTFAsset->Scenes[SceneIndex];

        size_t NameLength = strlen(Scene->name);
        GLTFScene->Name = RR_ALLOC_NO_ZERO(NameLength + 1, GLTFContext->Arena);
        strcpy(GLTFScene->Name, Scene->name);

        GLTFScene->NodeCount = Scene->nodes_count;
        GLTFScene->Nodes = RR_ALLOC_TYPE_COUNT(
            Rr_GLTFNode *,
            GLTFScene->NodeCount,
            GLTFContext->Arena);

        for (uint32_t NodeIndex = 0; NodeIndex < Scene->nodes_count;
             ++NodeIndex)
        {
            GLTFScene->Nodes[NodeIndex] =
                &GLTFAsset
                     ->Nodes[cgltf_node_index(Data, Scene->nodes[NodeIndex])];
        }
    }

    cgltf_free(Data);

    Rr_DestroyScratch(Scratch);

    return GLTFAsset;
}
