#include "Rr_GLTF.h"

#include "Rr_Buffer.h"
#include "Rr_Image.h"
#include "Rr_Log.h"
#include "Rr_UploadContext.h"

#include <stb/stb_image.h>

#include <cgltf/cgltf.h>

#include <assert.h>
#include <string.h>

static inline Rr_IndexType Rr_CGLTFComponentTypeToIndexType(
    cgltf_component_type Type)
{
    switch(Type)
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
    switch(Type)
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
    switch(Type)
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
    switch(Type)
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
    switch(Type)
    {
        case RR_GLTF_ATTRIBUTE_TYPE_TEXCOORD0:
            return RR_FORMAT_VEC2;
        case RR_GLTF_ATTRIBUTE_TYPE_POSITION:
        case RR_GLTF_ATTRIBUTE_TYPE_NORMAL:
        case RR_GLTF_ATTRIBUTE_TYPE_COLOR:
        case RR_GLTF_ATTRIBUTE_TYPE_TANGENT:
            return RR_FORMAT_VEC3;
        default:
            return RR_FORMAT_INVALID;
    }
}

Rr_GLTFContext *Rr_CreateGLTFContext(
    Rr_Renderer *Renderer,
    size_t VertexInputBindingCount,
    Rr_VertexInputBinding *VertexInputBindings,
    Rr_GLTFVertexInputBinding *GLTFVertexInputBindings,
    size_t GLTFTextureMappingCount,
    Rr_GLTFTextureMapping *GLTFTextureMappings)
{
    assert(VertexInputBindingCount != 0);
    assert(VertexInputBindings != NULL);
    assert(GLTFVertexInputBindings != NULL);
#if defined(RR_DEBUG)
    for(size_t BindingIndex = 0; BindingIndex < VertexInputBindingCount;
        ++BindingIndex)
    {
        Rr_VertexInputBinding *VertexInputBinding =
            VertexInputBindings + BindingIndex;
        Rr_GLTFVertexInputBinding *GLTFVertexInputBinding =
            GLTFVertexInputBindings + BindingIndex;
        assert(
            VertexInputBinding->AttributeCount ==
            GLTFVertexInputBinding->AttributeTypeCount);
        for(size_t AttributeIndex = 0;
            AttributeIndex < VertexInputBinding->AttributeCount;
            ++AttributeIndex)
        {
            Rr_VertexInputAttribute *Attribute =
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

    Rr_GLTFContext *GLTFContext = RR_ALLOC_TYPE(Arena, Rr_GLTFContext);
    GLTFContext->Arena = Arena;
    GLTFContext->Renderer = Renderer;

    GLTFContext->VertexInputStrides =
        RR_ALLOC_TYPE_COUNT(Arena, size_t, VertexInputBindingCount);
    GLTFContext->VertexInputBindingCount = VertexInputBindingCount;
    RR_ALLOC_COPY(
        Arena,
        GLTFContext->VertexInputBindings,
        GLTFVertexInputBindings,
        sizeof(Rr_GLTFVertexInputBinding) * VertexInputBindingCount);
    for(size_t BindingIndex = 0; BindingIndex < VertexInputBindingCount;
        ++BindingIndex)
    {
        Rr_GLTFVertexInputBinding *GLTFVertexInputBinding =
            GLTFContext->VertexInputBindings + BindingIndex;
        RR_ALLOC_COPY(
            Arena,
            GLTFVertexInputBinding->AttributeTypes,
            GLTFVertexInputBindings[BindingIndex].AttributeTypes,
            sizeof(Rr_GLTFAttributeType) *
                GLTFVertexInputBindings[BindingIndex].AttributeTypeCount);

        for(size_t AttributeIndex = 0;
            AttributeIndex < GLTFVertexInputBinding->AttributeTypeCount;
            ++AttributeIndex)
        {
            GLTFContext->VertexInputStrides[BindingIndex] +=
                Rr_GetGLTFAttributeSize(
                    GLTFVertexInputBinding->AttributeTypes[AttributeIndex]);
        }
    }

    RR_ALLOC_COPY(
        Arena,
        GLTFContext->TextureMappings,
        GLTFTextureMappings,
        sizeof(Rr_GLTFTextureMapping) * GLTFTextureMappingCount);

    return GLTFContext;
}

void Rr_DestroyGLTFContext(Rr_GLTFContext *GLTFContext)
{
    for(size_t Index = 0; Index < GLTFContext->Buffers.Count; ++Index)
    {
        Rr_DestroyBuffer(
            GLTFContext->Renderer,
            GLTFContext->Buffers.Data[Index]);
    }

    for(size_t Index = 0; Index < GLTFContext->Images.Count; ++Index)
    {
        Rr_DestroyImage(GLTFContext->Renderer, GLTFContext->Images.Data[Index]);
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
    for(size_t BindingIndex = 0;
        BindingIndex < Context->VertexInputBindingCount;
        ++BindingIndex)
    {
        Rr_GLTFVertexInputBinding *Binding =
            Context->VertexInputBindings + BindingIndex;
        size_t Offset = 0;
        for(size_t Index = 0; Index < Binding->AttributeTypeCount; ++Index)
        {
            Rr_GLTFAttributeType Type = Binding->AttributeTypes[Index];
            if(Rr_GetCGLTFAttributeType(Type) == AttributeType)
            {
                if(Found)
                {
                    RR_ABORT("GLTF: Multiple mappings found for the same "
                             "attribute type!");
                }
                if(Out)
                {
                    Out->Binding = BindingIndex;
                    Out->Offset = Offset;
                }
                Found = true;
            }
            Offset += Rr_GetGLTFAttributeSize(Type);
        }
        if(Found)
        {
            if(Out)
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
    for(size_t BindingIndex = 0;
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
    for(size_t Index = 0; Index < BindingIndex; ++Index)
    {
        Result += Context->VertexInputStrides[Index] * VertexCount;
    }
    return Result;
}

Rr_GLTFAsset *Rr_CreateGLTFAsset(
    Rr_GLTFContext *GLTFContext,
    Rr_UploadContext *UploadContext,
    Rr_AssetRef AssetRef,
    Rr_Arena *Arena)
{
    Rr_Scratch Scratch = Rr_GetScratch(Arena);

    Rr_Renderer *Renderer = GLTFContext->Renderer;
    Rr_Asset Asset = Rr_LoadAsset(AssetRef);

    cgltf_options Options = {
        .memory =
            (cgltf_memory_options){
                .alloc_func = Rr_GenericArenaAlloc,
                .free_func = Rr_GenericArenaFree,
                .user_data = Scratch.Arena,
            },
    };
    cgltf_data *Data = NULL;
    cgltf_result Result =
        cgltf_parse(&Options, Asset.Pointer, Asset.Size, &Data);
    if(Result != cgltf_result_success)
    {
        RR_ABORT("GLTF: Parsing failed!");
    }
    cgltf_load_buffers(&Options, Data, NULL);

    Rr_GLTFAsset *GLTFAsset = RR_ALLOC_TYPE(GLTFContext->Arena, Rr_GLTFAsset);

    /* Create staging structures. */

    size_t StagingDataSize = 0;
    size_t VertexDataOffset = 0;
    size_t VertexDataSize = 0;
    size_t IndexDataSize = 0;
    size_t MaxIndexSize = 0;
    size_t TotalIndexCount = 0;

    /* Calculate how much memory is needed for vertices and indices. */

    for(size_t MeshIndex = 0; MeshIndex < Data->meshes_count; ++MeshIndex)
    {
        cgltf_mesh *Mesh = Data->meshes + MeshIndex;
        for(size_t PrimitiveIndex = 0; PrimitiveIndex < Mesh->primitives_count;
            ++PrimitiveIndex)
        {
            cgltf_primitive *Primitive = Mesh->primitives + PrimitiveIndex;
            assert(Primitive->attributes_count > 0);

            size_t VertexCount = Primitive->attributes->data->count;

            /* Quick check to make sure every attribute has the same count. */

            for(size_t AttributeIndex = 1;
                AttributeIndex < Primitive->attributes_count;
                ++AttributeIndex)
            {
                cgltf_attribute *Attribute =
                    Primitive->attributes + AttributeIndex;
                if(VertexCount != Attribute->data->count)
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
    if(MaxIndexSize == 1)
    {
        GLTFAsset->IndexType = RR_INDEX_TYPE_UINT8;
    }
    else if(MaxIndexSize == 2)
    {
        GLTFAsset->IndexType = RR_INDEX_TYPE_UINT16;
    }
    else if(MaxIndexSize == 4)
    {
        GLTFAsset->IndexType = RR_INDEX_TYPE_UINT32;
    }
    else
    {
        RR_ABORT("GLTF: Unsupported index type!");
    }

    static const int SafeAlignment = 64;
    GLTFAsset->VertexBufferOffset = 0;
    GLTFAsset->IndexBufferOffset = RR_ALIGN_POW2(VertexDataSize, SafeAlignment);
    StagingDataSize = GLTFAsset->IndexBufferOffset +
                      RR_ALIGN_POW2(IndexDataSize, SafeAlignment);

    Rr_Buffer *StagingBuffer = Rr_CreateBuffer(
        Renderer,
        StagingDataSize,
        RR_BUFFER_FLAGS_STAGING_INCOHERENT_BIT | RR_BUFFER_FLAGS_MAPPED_BIT);
    *RR_PUSH_SLICE(&UploadContext->StagingBuffers, UploadContext->Arena) =
        StagingBuffer;
    char *StagingData = Rr_GetMappedBufferData(Renderer, StagingBuffer);

    /* Preallocate materials. */

    if(Data->materials)
    {
        GLTFAsset->Materials = RR_ALLOC_TYPE_COUNT(
            GLTFContext->Arena,
            Rr_GLTFMaterial,
            Data->materials_count);
    }

    if(Data->images)
    {
        GLTFAsset->Images = RR_ALLOC_TYPE_COUNT(
            GLTFContext->Arena,
            Rr_Image *,
            Data->images_count);
    }

    /* Process meshes. */

    GLTFAsset->MeshCount = Data->meshes_count;
    GLTFAsset->Meshes = RR_ALLOC_TYPE_COUNT(
        GLTFContext->Arena,
        Rr_GLTFMesh,
        GLTFAsset->MeshCount);
    size_t FirstIndex = 0;
    size_t VertexOffset = 0;
    for(size_t MeshIndex = 0; MeshIndex < Data->meshes_count; ++MeshIndex)
    {
        cgltf_mesh *Mesh = Data->meshes + MeshIndex;

        Rr_GLTFMesh *GLTFMesh = GLTFAsset->Meshes + MeshIndex;
        GLTFMesh->PrimitiveCount = Mesh->primitives_count;
        GLTFMesh->Primitives = RR_ALLOC_TYPE_COUNT(
            GLTFContext->Arena,
            Rr_GLTFPrimitive,
            GLTFMesh->PrimitiveCount);

        if(Mesh->name)
        {
            RR_ALLOC_COPY(
                GLTFContext->Arena,
                GLTFMesh->Name,
                Mesh->name,
                strlen(Mesh->name));
        }

        for(size_t PrimitiveIndex = 0;
            PrimitiveIndex < GLTFMesh->PrimitiveCount;
            ++PrimitiveIndex)
        {
            cgltf_primitive *Primitive = Mesh->primitives + PrimitiveIndex;

            Rr_GLTFPrimitive *GLTFPrimitive =
                GLTFMesh->Primitives + PrimitiveIndex;
            GLTFPrimitive->AttributeCount = Primitive->attributes_count;
            GLTFPrimitive->Attributes = RR_ALLOC_TYPE_COUNT(
                GLTFContext->Arena,
                Rr_GLTFAttribute,
                GLTFPrimitive->AttributeCount);

            if(Primitive->material)
            {
                GLTFPrimitive->Material =
                    GLTFAsset->Materials +
                    cgltf_material_index(Data, Primitive->material);
            }

            size_t VertexCount = Primitive->attributes->data->count;

            GLTFPrimitive->VertexCount = VertexCount;
            GLTFPrimitive->IndexCount = Primitive->indices->count;
            GLTFPrimitive->FirstIndex = FirstIndex;
            GLTFPrimitive->VertexOffset = VertexOffset;

            for(size_t AttributeIndex = 0;
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

                Rr_GLTFVertexInputInfo Info;
                bool Found = Rr_GetGLTFVertexInputInfoForAttribute(
                    GLTFContext,
                    Attribute->type,
                    &Info);
                if(Found)
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

                    for(size_t VertexIndex = 0; VertexIndex < VertexCount;
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
            for(size_t IndexIndex = 0; IndexIndex < GLTFPrimitive->IndexCount;
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

    Rr_FlushBufferRange(Renderer, StagingBuffer, 0, StagingDataSize);

    GLTFAsset->Buffer = Rr_CreateBuffer(
        Renderer,
        StagingDataSize,
        RR_BUFFER_FLAGS_INDEX_BIT | RR_BUFFER_FLAGS_VERTEX_BIT);
    *RR_PUSH_SLICE(&GLTFContext->Buffers, GLTFContext->Arena) =
        GLTFAsset->Buffer;

    Rr_UploadStagingBuffer(
        Renderer,
        UploadContext,
        GLTFAsset->Buffer,
        (Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        },
        (Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
            .AccessMask =
                VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
        },
        StagingBuffer,
        0,
        StagingDataSize);

    /* Process materials, textures and images. */

    for(size_t MaterialIndex = 0; MaterialIndex < Data->materials_count;
        ++MaterialIndex)
    {
        cgltf_material *Material = Data->materials + MaterialIndex;

        size_t TextureCount = 0;

        if(Material->has_pbr_metallic_roughness &&
           Material->pbr_metallic_roughness.base_color_texture.texture != NULL)
        {
            TextureCount++;
        }

        Rr_GLTFMaterial *GLTFMaterial = GLTFAsset->Materials + MaterialIndex;
        GLTFMaterial->TextureCount = TextureCount;
        GLTFMaterial->Textures =
            RR_ALLOC_TYPE_COUNT(GLTFContext->Arena, size_t, TextureCount);
        GLTFMaterial->TextureTypes = RR_ALLOC_TYPE_COUNT(
            GLTFContext->Arena,
            Rr_GLTFTextureType,
            TextureCount);

        size_t CurrentTextureIndex = 0;

        if(Material->has_pbr_metallic_roughness &&
           Material->pbr_metallic_roughness.base_color_texture.texture != NULL)
        {
            cgltf_texture *Texture =
                Material->pbr_metallic_roughness.base_color_texture.texture;
            if(strcmp(Texture->image->mime_type, "image/png") == 0 ||
               strcmp(Texture->image->mime_type, "image/jpeg") == 0)
            {
                GLTFMaterial->TextureTypes[CurrentTextureIndex] =
                    RR_GLTF_TEXTURE_TYPE_COLOR;
                if(GLTFAsset->Images[CurrentTextureIndex] == NULL)
                {
                    int32_t ImageDataSize = Texture->image->buffer_view->size;
                    char *ImageData =
                        (char *)Texture->image->buffer_view->buffer->data +
                        Texture->image->buffer_view->offset;

                    GLTFAsset->Images[CurrentTextureIndex] =
                        Rr_CreateImageRGBA8FromPNG(
                            Renderer,
                            UploadContext,
                            ImageDataSize,
                            ImageData);

                    *RR_PUSH_SLICE(&GLTFContext->Images, GLTFContext->Arena) =
                        GLTFAsset->Images[CurrentTextureIndex];
                }
                CurrentTextureIndex++;
            }
        }
    }

    cgltf_free(Data);

    Rr_DestroyScratch(Scratch);

    return GLTFAsset;
}
