#include "Rr_GLTF.h"

#include "Rr_App.h"
#include "Rr_Buffer.h"
#include "Rr_Image.h"
#include "Rr_Log.h"
#include "Rr_UploadContext.h"

#include <stb/stb_image.h>

#include <cgltf/cgltf.h>

#include <assert.h>

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
    Rr_App *App,
    size_t VertexInputBindingCount,
    Rr_VertexInputBinding *VertexInputBindings,
    Rr_GLTFVertexInputBinding *GLTFVertexInputBindings,
    size_t GLTFTextureMappingCount,
    Rr_GLTFTextureMapping *GLTFTextureMappings)
{
    assert(VertexInputBindingCount != 0);
    assert(VertexInputBindings != NULL);
    assert(GLTFVertexInputBindings != NULL);
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

    Rr_Arena *Arena = Rr_CreateDefaultArena();

    Rr_GLTFContext *GLTFContext = RR_ALLOC_TYPE(Arena, Rr_GLTFContext);
    GLTFContext->Arena = Arena;

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

void Rr_DestroyGLTFContext(Rr_App *App, Rr_GLTFContext *GLTFContext)
{
    for(size_t Index = 0; Index < GLTFContext->Buffers.Count; ++Index)
    {
        Rr_DestroyBuffer(App, GLTFContext->Buffers.Data[Index]);
    }

    for(size_t Index = 0; Index < GLTFContext->Images.Count; ++Index)
    {
        Rr_DestroyImage(App, GLTFContext->Images.Data[Index]);
    }

    Rr_DestroyArena(GLTFContext->Arena);
}

// static Rr_RawMesh Rr_CreateRawMeshFromGLTFPrimitive(cgltf_primitive
// *Primitive, Rr_Arena *Arena)
// {
//     Rr_Scratch Scratch = Rr_GetScratch(Arena);
//
//     size_t VertexCount = Primitive->attributes->data->count;
//     size_t IndexCount = Primitive->indices->count;
//
//     Rr_RawMesh RawMesh = { 0 };
//     RR_RESERVE_SLICE(&RawMesh.VerticesSlice, VertexCount, Arena);
//     RR_RESERVE_SLICE(&RawMesh.IndicesSlice, IndexCount, Arena);
//
//     char *IndexData = (char *)Primitive->indices->buffer_view->buffer->data +
//     Primitive->indices->buffer_view->offset;
//     if(Primitive->indices->component_type == cgltf_component_type_r_16u)
//     {
//         uint16_t *Indices = (uint16_t *)IndexData;
//         for(size_t Index = 0; Index < IndexCount; ++Index)
//         {
//             uint32_t Converted = *(Indices + Index);
//             *RR_PUSH_SLICE(&RawMesh.IndicesSlice, NULL) = Converted;
//         }
//     }
//     else
//     {
//         RR_ABORT("Unsupported index type!");
//     }
//
//     for(size_t VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
//     {
//         Rr_Vertex NewVertex = { 0 };
//
//         for(size_t AttributeIndex = 0; AttributeIndex <
//         Primitive->attributes_count; ++AttributeIndex)
//         {
//             cgltf_attribute *Attribute = Primitive->attributes +
//             AttributeIndex; cgltf_accessor *Accessor = Attribute->data; char
//             *AttributeData = (char *)Accessor->buffer_view->buffer->data +
//             Accessor->buffer_view->offset; switch(Attribute->type)
//             {
//                 case cgltf_attribute_type_position:
//                 {
//                     Rr_Vec3 *Position = (Rr_Vec3 *)(AttributeData +
//                     Accessor->stride * VertexIndex); NewVertex.Position =
//                     *Position;
//                 }
//                 break;
//                 case cgltf_attribute_type_normal:
//                 {
//                     Rr_Vec3 *Normal = (Rr_Vec3 *)(AttributeData +
//                     Accessor->stride * VertexIndex); NewVertex.Normal =
//                     *Normal;
//                 }
//                 break;
//                 case cgltf_attribute_type_tangent:
//                 {
//                     Rr_Vec3 *Tangent = (Rr_Vec3 *)(AttributeData +
//                     Accessor->stride * VertexIndex); NewVertex.Tangent =
//                     *Tangent;
//                 }
//                 break;
//                 case cgltf_attribute_type_texcoord:
//                 {
//                     Rr_Vec2 *TexCoord = (Rr_Vec2 *)(AttributeData +
//                     Accessor->stride * VertexIndex); NewVertex.TexCoordX =
//                     TexCoord->X; NewVertex.TexCoordY = TexCoord->Y;
//                 }
//                 break;
//                 default:
//                     break;
//             }
//         }
//
//         *RR_PUSH_SLICE(&RawMesh.VerticesSlice, NULL) = NewVertex;
//     }
//
//     Rr_DestroyScratch(Scratch);
//
//     return RawMesh;
// }
//
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
//
// static Rr_RawMesh Rr_CreateRawMeshFromOBJ(Rr_Asset *Asset, Rr_Arena *Arena)
// {
//     Rr_Scratch Scratch = Rr_GetScratch(Arena);
//
//     RR_SLICE(Rr_Vec3) ScratchPositions = { 0 };
//     RR_SLICE(Rr_Vec4) ScratchColors = { 0 };
//     RR_SLICE(Rr_Vec2) ScratchTexCoords = { 0 };
//     RR_SLICE(Rr_Vec3) ScratchNormals = { 0 };
//     RR_SLICE(Rr_IntVec3) ScratchIndices = { 0 };
//
//     Rr_RawMesh RawMesh = { 0 };
//
//     size_t CurrentIndex = 0;
//     char *EndPos;
//     while(CurrentIndex < Asset->Size)
//     {
//         switch(Asset->Data[CurrentIndex])
//         {
//             case 'v':
//             {
//                 CurrentIndex++;
//                 switch(Asset->Data[CurrentIndex])
//                 {
//                     case ' ':
//                     {
//                         Rr_Vec3 NewPosition;
//                         NewPosition.X = (float)SDL_strtod(Asset->Data +
//                         CurrentIndex, &EndPos); NewPosition.Y =
//                         (float)SDL_strtod(EndPos, &EndPos); NewPosition.Z =
//                         (float)SDL_strtod(EndPos, &EndPos);
//
//                         *RR_PUSH_SLICE(&ScratchPositions, Scratch.Arena) =
//                         NewPosition;
//
//                         if(*EndPos == ' ')
//                         {
//                             Rr_Vec4 NewColor = { 0 };
//                             NewColor.X = (float)SDL_strtod(EndPos, &EndPos);
//                             NewColor.Y = (float)SDL_strtod(EndPos, &EndPos);
//                             NewColor.Z = (float)SDL_strtod(EndPos, &EndPos);
//
//                             *RR_PUSH_SLICE(&ScratchColors, Scratch.Arena) =
//                             NewColor;
//                         }
//                     }
//                     break;
//                     case 't':
//                     {
//                         CurrentIndex++;
//                         Rr_Vec2 NewTexCoord;
//                         NewTexCoord.X = (float)SDL_strtod(Asset->Data +
//                         CurrentIndex, &EndPos); NewTexCoord.Y =
//                         (float)SDL_strtod(EndPos, &EndPos);
//
//                         *RR_PUSH_SLICE(&ScratchTexCoords, Scratch.Arena) =
//                         NewTexCoord;
//                     }
//                     break;
//                     case 'n':
//                     {
//                         CurrentIndex++;
//                         Rr_Vec3 NewNormal;
//                         NewNormal.X = (float)SDL_strtod(Asset->Data +
//                         CurrentIndex, &EndPos); NewNormal.Y =
//                         (float)SDL_strtod(EndPos, &EndPos); NewNormal.Z =
//                         (float)SDL_strtod(EndPos, &EndPos);
//
//                         *RR_PUSH_SLICE(&ScratchNormals, Scratch.Arena) =
//                         NewNormal;
//                     }
//                     break;
//                 }
//             }
//             break;
//             case 'f':
//             {
//                 CurrentIndex++;
//                 Rr_IntVec3 OBJIndices[3] = { 0 };
//                 OBJIndices[0].X = (int32_t)SDL_strtoul(Asset->Data +
//                 CurrentIndex, &EndPos, 10); OBJIndices[0].Y =
//                 (int32_t)SDL_strtoul(EndPos + 1, &EndPos, 0); OBJIndices[0].Z
//                 = (int32_t)SDL_strtoul(EndPos + 1, &EndPos, 0);
//                 OBJIndices[1].X = (int32_t)SDL_strtoul(EndPos + 1, &EndPos,
//                 0); OBJIndices[1].Y = (int32_t)SDL_strtoul(EndPos + 1,
//                 &EndPos, 0); OBJIndices[1].Z = (int32_t)SDL_strtoul(EndPos +
//                 1, &EndPos, 0); OBJIndices[2].X = (int32_t)SDL_strtoul(EndPos
//                 + 1, &EndPos, 0); OBJIndices[2].Y =
//                 (int32_t)SDL_strtoul(EndPos + 1, &EndPos, 0); OBJIndices[2].Z
//                 = (int32_t)SDL_strtoul(EndPos + 1, &EndPos, 0); for(size_t
//                 Index = 0; Index < 3; Index++)
//                 {
//                     OBJIndices[Index].X--;
//                     OBJIndices[Index].Y--;
//                     OBJIndices[Index].Z--;
//
//                     size_t ExistingOBJIndex = SIZE_MAX;
//                     for(size_t I = 0; I < ScratchIndices.Count; I++)
//                     {
//                         if(Rr_EqIV3(OBJIndices[Index],
//                         ScratchIndices.Data[I]))
//                         {
//                             ExistingOBJIndex = I;
//                             break;
//                         }
//                     }
//                     if(ExistingOBJIndex == SIZE_MAX)
//                     {
//                         Rr_Vec2 *TexCoord =
//                         &ScratchTexCoords.Data[OBJIndices[Index].Y];
//                         Rr_Vertex NewVertex = { 0 };
//                         NewVertex.Position =
//                         ScratchPositions.Data[OBJIndices[Index].X];
//                         // NewVertex.Color =
//                         // ScratchColors.Data[OBJIndices[Index].X];
//                         NewVertex.Normal =
//                         ScratchNormals.Data[OBJIndices[Index].Z];
//                         NewVertex.TexCoordX = (*TexCoord).X;
//                         // NewVertex.TexCoordY = (*TexCoord).Y;
//                         NewVertex.TexCoordY = 1.0f - (*TexCoord).Y;
//                         *RR_PUSH_SLICE(&RawMesh.VerticesSlice, Arena) =
//                         NewVertex;
//
//                         *RR_PUSH_SLICE(&ScratchIndices, Scratch.Arena) =
//                         OBJIndices[Index];
//
//                         /* Add freshly added vertex index */
//                         *RR_PUSH_SLICE(&RawMesh.IndicesSlice, Arena) =
//                             (Rr_MeshIndexType){ RawMesh.VerticesSlice.Count -
//                             1 };
//                     }
//                     else
//                     {
//                         *RR_PUSH_SLICE(&RawMesh.IndicesSlice, Arena) =
//                         ExistingOBJIndex;
//                     }
//                 }
//             }
//             break;
//             default:
//             {
//             }
//             break;
//         }
//
//         CurrentIndex += strcspn(Asset->Data + CurrentIndex, "\n") + 1;
//     }
//
//     Rr_CalculateTangents(RawMesh.IndicesSlice.Count,
//     RawMesh.IndicesSlice.Data, RawMesh.VerticesSlice.Data);
//
//     Rr_DestroyScratch(Scratch);
//
//     return RawMesh;
// }
//
// Rr_Primitive *Rr_CreatePrimitive(Rr_App *App, Rr_UploadContext
// *UploadContext, Rr_RawMesh *RawMesh)
// {
//     Rr_Primitive *Primitive =
//     RR_GET_FREE_LIST_ITEM(&App->Renderer.Primitives, App->PermanentArena);
//
//     Primitive->IndexCount = RawMesh->IndicesSlice.Count;
//
//     size_t VertexBufferSize = sizeof(Rr_Vertex) *
//     RawMesh->VerticesSlice.Count; size_t IndexBufferSize =
//     sizeof(Rr_MeshIndexType) * Primitive->IndexCount;
//
//     Primitive->VertexBuffer =
//         Rr_CreateBuffer(App, VertexBufferSize,
//         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO, false);
//
//     Primitive->IndexBuffer =
//         Rr_CreateBuffer(App, IndexBufferSize,
//         VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO, false);
//
//     Rr_UploadBuffer(
//         App,
//         UploadContext,
//         Primitive->VertexBuffer,
//         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
//         0,
//         VK_PIPELINE_STAGE_VERTEX_INPUT_BIT |
//         VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
//         VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
//         RR_MAKE_DATA(RawMesh->VerticesSlice.Data, VertexBufferSize));
//
//     Rr_UploadBuffer(
//         App,
//         UploadContext,
//         Primitive->IndexBuffer,
//         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
//         0,
//         VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
//         VK_ACCESS_INDEX_READ_BIT,
//         RR_MAKE_DATA(RawMesh->IndicesSlice.Data, IndexBufferSize));
//
//     return Primitive;
// }
//
// void Rr_DestroyPrimitive(Rr_App *App, Rr_Primitive *Primitive)
// {
//     if(Primitive == NULL)
//     {
//         return;
//     }
//
//     Rr_DestroyBuffer(App, Primitive->IndexBuffer);
//     Rr_DestroyBuffer(App, Primitive->VertexBuffer);
//
//     RR_RETURN_FREE_LIST_ITEM(&App->Renderer.Primitives, Primitive);
// }
//
// Rr_StaticMesh *Rr_CreateStaticMesh(
//     Rr_App *App,
//     Rr_UploadContext *UploadContext,
//     Rr_RawMesh *RawMeshes,
//     size_t RawMeshCount)
// {
//     Rr_StaticMesh *StaticMesh =
//     RR_GET_FREE_LIST_ITEM(&App->Renderer.StaticMeshes, App->PermanentArena);
//
//     for(size_t Index = 0; Index < RawMeshCount; ++Index)
//     {
//         StaticMesh->Primitives[Index] = Rr_CreatePrimitive(App,
//         UploadContext, RawMeshes + Index);
//     }
//     StaticMesh->PrimitiveCount = RawMeshCount;
//
//     return StaticMesh;
// }
//
// void Rr_DestroyStaticMesh(Rr_App *App, Rr_StaticMesh *StaticMesh)
// {
//     if(StaticMesh == NULL)
//     {
//         return;
//     }
//
//     for(size_t Index = 0; Index < StaticMesh->PrimitiveCount; ++Index)
//     {
//         Rr_DestroyPrimitive(App, StaticMesh->Primitives[Index]);
//     }
//
//     RR_RETURN_FREE_LIST_ITEM(&App->Renderer.StaticMeshes, StaticMesh);
// }
//
// Rr_StaticMesh *Rr_CreateStaticMeshGLTF(
//     Rr_App *App,
//     Rr_UploadContext *UploadContext,
//     Rr_AssetRef AssetRef,
//     Rr_GLTFLoader *Loader,
//     size_t MeshIndex,
//     Rr_Arena *Arena)
// {
//     Rr_Scratch Scratch = Rr_GetScratch(Arena);
//
//     Rr_Asset Asset = Rr_LoadAsset(AssetRef);
//
//     cgltf_options Options = { .memory =
//     Rr_GetCGLTFMemoryOptions(Scratch.Arena) }; cgltf_data *Data = NULL;
//     cgltf_mesh *Mesh = Rr_ParseGLTFMesh(&Asset, MeshIndex, &Options, &Data);
//     cgltf_load_buffers(&Options, Data, NULL);
//
//     Rr_RawMesh *RawMeshes = RR_ALLOC_STRUCT_COUNT(Scratch.Arena, Rr_RawMesh,
//     Mesh->primitives_count);
//
//     // Rr_Material *Materials[RR_MESH_MAX_PRIMITIVES] = { 0 };
//     //
//     // for(size_t Index = 0; Index < Mesh->primitives_count; ++Index)
//     // {
//     //     cgltf_primitive *Primitive = Mesh->primitives + Index;
//     //
//     //     if(Loader != NULL && Primitive->material != NULL)
//     //     {
//     //         Rr_Image *Textures[RR_MAX_TEXTURES_PER_MATERIAL] = { 0 };
//     //
//     //         cgltf_material *CGLTFMaterial = Primitive->material;
//     //         if(CGLTFMaterial->has_pbr_metallic_roughness)
//     //         {
//     //             cgltf_texture *BaseColorTexture =
//     CGLTFMaterial->pbr_metallic_roughness.base_color_texture.texture;
//     //             if(BaseColorTexture)
//     //             {
//     //                 if(strcmp(BaseColorTexture->image->mime_type,
//     "image/png") == 0)
//     //                 {
//     //                     char *PNGData = (char
//     *)BaseColorTexture->image->buffer_view->buffer->data +
//     // BaseColorTexture->image->buffer_view->offset;
//     //                     size_t PNGSize =
//     BaseColorTexture->image->buffer_view->size;
//     //
//     //                     Textures[Loader->BaseTexture] =
//     //                         Rr_CreateColorImageFromPNGMemory(App,
//     UploadContext, PNGData, PNGSize, false);
//     //                 }
//     //             }
//     //         }
//     //         if(CGLTFMaterial->normal_texture.texture != NULL)
//     //         {
//     //             cgltf_texture *NormalTexture =
//     CGLTFMaterial->normal_texture.texture;
//     //             if(strcmp(NormalTexture->image->mime_type, "image/png") ==
//     0)
//     //             {
//     //                 char *PNGData = (char
//     *)NormalTexture->image->buffer_view->buffer->data +
//     // NormalTexture->image->buffer_view->offset;
//     //                 size_t PNGSize =
//     NormalTexture->image->buffer_view->size;
//     //
//     //                 Textures[Loader->NormalTexture] =
//     //                     Rr_CreateColorImageFromPNGMemory(App,
//     UploadContext, PNGData, PNGSize, false);
//     //             }
//     //         }
//     //
//     //         Materials[Index] = Rr_CreateMaterial(App,
//     Loader->GenericPipeline, Textures,
//     //         RR_MAX_TEXTURES_PER_MATERIAL); Materials[Index]->bOwning =
//     true;
//     //     }
//     //
//     //     Rr_RawMesh *RawMesh = RawMeshes + Index;
//     //     *RawMesh = Rr_CreateRawMeshFromGLTFPrimitive(Primitive,
//     Scratch.Arena);
//     // }
//
//     Rr_StaticMesh *StaticMesh = Rr_CreateStaticMesh(App, UploadContext,
//     RawMeshes, Mesh->primitives_count);
//
//     cgltf_free(Data);
//
//     Rr_DestroyScratch(Scratch);
//
//     return StaticMesh;
// }
//
// Rr_StaticMesh *Rr_CreateStaticMeshOBJ(
//     Rr_App *App,
//     Rr_UploadContext *UploadContext,
//     Rr_AssetRef AssetRef,
//     Rr_Arena *Arena)
// {
//     Rr_Scratch Scratch = Rr_GetScratch(Arena);
//
//     Rr_Asset Asset = Rr_LoadAsset(AssetRef);
//
//     Rr_RawMesh RawMesh = Rr_CreateRawMeshFromOBJ(&Asset, Scratch.Arena);
//
//     Rr_StaticMesh *StaticMesh = Rr_CreateStaticMesh(App, UploadContext,
//     &RawMesh, 1);
//
//     Rr_DestroyScratch(Scratch);
//
//     return StaticMesh;
// }
//
// void Rr_GetStaticMeshSizeOBJ(Rr_AssetRef AssetRef, Rr_Arena *Arena,
// Rr_LoadSize *OutLoadSize)
// {
//     Rr_Scratch Scratch = Rr_GetScratch(Arena);
//
//     Rr_Asset Asset = Rr_LoadAsset(AssetRef);
//
//     Rr_RawMesh RawMesh = Rr_CreateRawMeshFromOBJ(&Asset, Scratch.Arena);
//
//     size_t VertexBufferSize = sizeof(Rr_Vertex) *
//     RawMesh.VerticesSlice.Count; size_t IndexBufferSize =
//     sizeof(Rr_MeshIndexType) * RawMesh.IndicesSlice.Count;
//
//     OutLoadSize->StagingBufferSize += VertexBufferSize + IndexBufferSize;
//     OutLoadSize->BufferCount += 2;
//
//     Rr_DestroyScratch(Scratch);
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
    Rr_App *App,
    struct Rr_UploadContext *UploadContext,
    Rr_AssetRef AssetRef,
    Rr_GLTFContext *GLTFContext,
    Rr_Arena *Arena)
{
    Rr_Scratch Scratch = Rr_GetScratch(Arena);

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
    size_t StagingDataOffset = 0;
    size_t GeometryDataSize = 0;
    size_t ImageDataSize = 0;

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

            StagingDataSize +=
                Rr_GetStagingSizeForVertexCount(GLTFContext, VertexCount);

            size_t IndexSize = cgltf_calc_size(
                Primitive->indices->type,
                Primitive->indices->component_type);
            StagingDataSize += Primitive->indices->count * IndexSize;
        }
    }

    GeometryDataSize = StagingDataSize;

    /* Calculate how much memory is needed for images. */

    for(size_t ImageIndex = 0; ImageIndex < Data->images_count; ++ImageIndex)
    {
        cgltf_image *Image = Data->images + ImageIndex;

        if(strcmp(Image->mime_type, "image/png") == 0 ||
           strcmp(Image->mime_type, "image/jpeg") == 0)
        {
            char *Data = (char *)Image->buffer_view->buffer->data +
                         Image->buffer_view->offset;
            size_t Size = Image->buffer_view->size;

            int Width = 0;
            int Height = 0;
            int Components = 0;
            stbi_info_from_memory(
                (stbi_uc *)Data,
                Size,
                &Width,
                &Height,
                &Components);

            StagingDataSize += Width * Height * Components;
        }
    }

    ImageDataSize = StagingDataSize - GeometryDataSize;
    RR_UNUSED(ImageDataSize);

    char *StagingData = RR_ALLOC(Scratch.Arena, StagingDataSize);

    /* Preallocate materials. */

    GLTFAsset->Materials = RR_ALLOC_TYPE_COUNT(
        GLTFContext->Arena,
        Rr_GLTFMaterial,
        Data->materials_count);

    GLTFAsset->Images =
        RR_ALLOC_TYPE_COUNT(GLTFContext->Arena, Rr_Image *, Data->images_count);

    /* Process meshes. */

    GLTFAsset->MeshCount = Data->meshes_count;
    GLTFAsset->Meshes = RR_ALLOC_TYPE_COUNT(
        GLTFContext->Arena,
        Rr_GLTFMesh,
        GLTFAsset->MeshCount);
    for(size_t MeshIndex = 0; MeshIndex < Data->meshes_count; ++MeshIndex)
    {
        cgltf_mesh *Mesh = Data->meshes + MeshIndex;

        Rr_GLTFMesh *GLTFMesh = GLTFAsset->Meshes + MeshIndex;
        GLTFMesh->PrimitiveCount = Mesh->primitives_count;
        GLTFMesh->Primitives = RR_ALLOC_TYPE_COUNT(
            GLTFContext->Arena,
            Rr_GLTFPrimitive,
            GLTFMesh->PrimitiveCount);

        RR_ALLOC_COPY(
            GLTFContext->Arena,
            GLTFMesh->Name,
            Mesh->name,
            strlen(Mesh->name));

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
            GLTFPrimitive->VertexBufferOffsets = RR_ALLOC_TYPE_COUNT(
                GLTFContext->Arena,
                size_t,
                GLTFContext->VertexInputBindingCount);

            GLTFPrimitive->Material =
                GLTFAsset->Materials +
                cgltf_material_index(Data, Primitive->material);

            size_t VertexCount = Primitive->attributes->data->count;

            GLTFPrimitive->VertexCount = VertexCount;

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
                        (char *)StagingData + StagingDataOffset + BindingOffset;

                    GLTFPrimitive->VertexBufferOffsets[Info.Binding] =
                        StagingDataOffset + BindingOffset;

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

            StagingDataOffset += Rr_GetFlatBindingOffset(
                GLTFContext,
                GLTFContext->VertexInputBindingCount,
                VertexCount);

            GLTFPrimitive->IndexCount = Primitive->indices->count;
            GLTFPrimitive->IndexBufferOffset = StagingDataOffset;
            GLTFPrimitive->IndexType = Rr_CGLTFComponentTypeToIndexType(
                Primitive->indices->component_type);
            size_t IndexSize =
                cgltf_component_size(Primitive->indices->component_type);
            for(size_t IndexIndex = 0; IndexIndex < GLTFPrimitive->IndexCount;
                ++IndexIndex)
            {
                void *Dst = (char *)StagingData + StagingDataOffset +
                            (IndexSize * IndexIndex);
                void *Src =
                    Rr_GetCGLTFAccessorValueAt(Primitive->indices, IndexIndex);
                memcpy(Dst, Src, IndexSize);
            }

            StagingDataOffset += GLTFPrimitive->IndexCount * IndexSize;
        }
    }

    assert(StagingDataOffset == GeometryDataSize);

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
            GLTFMaterial->TextureTypes[CurrentTextureIndex] =
                RR_GLTF_TEXTURE_TYPE_COLOR;
            if(GLTFAsset->Images[CurrentTextureIndex] == NULL)
            {
                int32_t ImageDataSize =
                    Material->pbr_metallic_roughness.base_color_texture.texture
                        ->image->buffer_view->size;
                char *ImageData =
                    (char *)Material->pbr_metallic_roughness.base_color_texture
                        .texture->image->buffer_view->buffer->data +
                    Material->pbr_metallic_roughness.base_color_texture.texture
                        ->image->buffer_view->offset;

                GLTFAsset->Images[CurrentTextureIndex] =
                    Rr_CreateImageRGBA8FromPNG(
                        App,
                        UploadContext,
                        ImageDataSize,
                        ImageData,
                        false);

                *RR_PUSH_SLICE(&GLTFContext->Images, GLTFContext->Arena) =
                    GLTFAsset->Images[CurrentTextureIndex];
            }
            CurrentTextureIndex++;
        }
    }

    /* At this point StagingData is populated with vertices, indices
     * and images. We create a mapped buffer and upload it to GPU. */

    Rr_Buffer *StagingBuffer = Rr_CreateStagingBuffer(App, StagingDataSize);
    *RR_PUSH_SLICE(&UploadContext->StagingBuffers, UploadContext->Arena) =
        StagingBuffer;
    memcpy(
        StagingBuffer->AllocatedBuffers[0].AllocationInfo.pMappedData,
        StagingData,
        StagingDataSize);

    GLTFAsset->Buffer = Rr_CreateBuffer(
        App,
        GeometryDataSize,
        RR_BUFFER_USAGE_INDEX_BUFFER_BIT | RR_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        false);
    *RR_PUSH_SLICE(&GLTFContext->Buffers, GLTFContext->Arena) =
        GLTFAsset->Buffer;

    Rr_UploadStagingBuffer(
        App,
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
        GeometryDataSize);

    // cgltf_scene *Scene = Data->scenes + SceneIndex;
    //
    // size_t VertexBufferSize = 0;
    // size_t IndexBufferSize = 0;
    //
    // bool *AddedMeshes = RR_ALLOC_TYPE_COUNT(Scratch.Arena, bool,
    // Data->meshes_count); bool *AddedImages =
    // RR_ALLOC_TYPE_COUNT(Scratch.Arena, bool, Data->images_count);
    //
    // for(size_t NodeIndex = 0; NodeIndex < Scene->nodes_count; ++NodeIndex)
    // {
    //     cgltf_node *Node = Scene->nodes[NodeIndex];
    //     cgltf_mesh *Mesh = Node->mesh;
    //     size_t MeshIndex = cgltf_mesh_index(Data, Mesh);
    //     if(AddedMeshes[MeshIndex] == true)
    //     {
    //         continue;
    //     }
    //     AddedMeshes[MeshIndex] = true;
    //     for(size_t PrimitiveIndex = 0; PrimitiveIndex <
    //     Mesh->primitives_count; ++PrimitiveIndex)
    //     {
    //         cgltf_primitive *Primitive = Mesh->primitives + PrimitiveIndex;
    //         for(size_t AttributeIndex = 0; AttributeIndex <
    //         Primitive->attributes_count; ++AttributeIndex)
    //         {
    //             cgltf_attribute *Attribute = Primitive->attributes +
    //             AttributeIndex;
    //             // Attribute->data->buffer_view->buffer
    //         }
    //     }
    // }

    // for(size_t Index = 0; Index < Mesh->primitives_count; ++Index)
    // {
    //     cgltf_primitive *Primitive = Mesh->primitives + Index;
    //
    //     VertexBufferSize += sizeof(Rr_Vertex) *
    //     Primitive->attributes->data->count; IndexBufferSize +=
    //     sizeof(Rr_MeshIndexType) * Primitive->indices->count;
    //
    //     if(Loader != NULL && Primitive->material != NULL)
    //     {
    //         cgltf_material *CGLTFMaterial = Primitive->material;
    //         if(CGLTFMaterial->has_pbr_metallic_roughness)
    //         {
    //             cgltf_texture *BaseColorTexture =
    //             CGLTFMaterial->pbr_metallic_roughness.base_color_texture.texture;
    //             if(BaseColorTexture)
    //             {
    //                 if(strcmp(BaseColorTexture->image->mime_type,
    //                 "image/png") == 0)
    //                 {
    //                     char *PNGData = (char
    //                     *)BaseColorTexture->image->buffer_view->buffer->data
    //                     +
    //                                     BaseColorTexture->image->buffer_view->offset;
    //                     size_t PNGSize =
    //                     BaseColorTexture->image->buffer_view->size;
    //
    //                     Rr_GetImageSizePNGMemory(PNGData, PNGSize,
    //                     Scratch.Arena, OutLoadSize);
    //                 }
    //             }
    //         }
    //         if(CGLTFMaterial->normal_texture.texture != NULL)
    //         {
    //             cgltf_texture *NormalTexture =
    //             CGLTFMaterial->normal_texture.texture;
    //             if(strcmp(NormalTexture->image->mime_type, "image/png") == 0)
    //             {
    //                 char *PNGData = (char
    //                 *)NormalTexture->image->buffer_view->buffer->data +
    //                                 NormalTexture->image->buffer_view->offset;
    //                 size_t PNGSize = NormalTexture->image->buffer_view->size;
    //
    //                 Rr_GetImageSizePNGMemory(PNGData, PNGSize, Scratch.Arena,
    //                 OutLoadSize);
    //             }
    //         }
    //     }
    // }
    //
    cgltf_free(Data);

    Rr_DestroyScratch(Scratch);

    return GLTFAsset;
}
