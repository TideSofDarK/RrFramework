#include "Rr_Mesh.h"

#include "Rr_App.h"
#include "Rr_Image.h"
#include "Rr_Log.h"
#include "Rr_UploadContext.h"

#include <Rr/Rr_Defines.h>

#include <SDL3/SDL.h>

#include <stb/stb_image.h>

#include <cgltf/cgltf.h>

static void *Rr_CGLTFArenaAlloc(void *Arena, cgltf_size Size)
{
    return RR_ALLOC((Rr_Arena *)Arena, Size);
}

static void Rr_CGLTFArenaFree(void *Arena, void *Ptr)
{ /* no-op */
}

static cgltf_memory_options Rr_GetCGLTFMemoryOptions(Rr_Arena *Arena)
{
    return (
        cgltf_memory_options){ .alloc_func = Rr_CGLTFArenaAlloc, .free_func = Rr_CGLTFArenaFree, .user_data = Arena };
}

static cgltf_mesh *Rr_ParseGLTFMesh(Rr_Asset *Asset, size_t MeshIndex, cgltf_options *Options, cgltf_data **OutData)
{
    cgltf_data *Data = NULL;
    cgltf_result Result = cgltf_parse(Options, Asset->Data, Asset->Length, &Data);
    if(Result != cgltf_result_success)
    {
        Rr_LogAbort("Error loading glTF asset!");
    }

    cgltf_mesh *Mesh = Data->meshes + MeshIndex;

    if(MeshIndex + 1 > Data->meshes_count || Mesh->primitives_count < 1)
    {
        Rr_LogAbort("Mesh contains no geometry!");
    }

    if(Data->meshes->primitives_count > RR_MESH_MAX_PRIMITIVES)
    {
        Rr_LogAbort("Exceeding max mesh primitives count!");
    }

    *OutData = Data;

    return Mesh;
}

static Rr_RawMesh Rr_CreateRawMeshFromGLTFPrimitive(cgltf_primitive *Primitive, Rr_Arena *Arena)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    size_t VertexCount = Primitive->attributes->data->count;
    size_t IndexCount = Primitive->indices->count;

    Rr_RawMesh RawMesh = { 0 };
    RR_SLICE_RESERVE(&RawMesh.VerticesSlice, VertexCount, Arena);
    RR_SLICE_RESERVE(&RawMesh.IndicesSlice, IndexCount, Arena);

    char *IndexData = (char *)Primitive->indices->buffer_view->buffer->data + Primitive->indices->buffer_view->offset;
    if(Primitive->indices->component_type == cgltf_component_type_r_16u)
    {
        uint16_t *Indices = (uint16_t *)IndexData;
        for(size_t Index = 0; Index < IndexCount; ++Index)
        {
            uint32_t Converted = *(Indices + Index);
            *RR_SLICE_PUSH(&RawMesh.IndicesSlice, NULL) = Converted;
        }
    }
    else
    {
        Rr_LogAbort("Unsupported index type!");
    }

    for(size_t VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
    {
        Rr_Vertex NewVertex = { 0 };

        for(size_t AttributeIndex = 0; AttributeIndex < Primitive->attributes_count; ++AttributeIndex)
        {
            cgltf_attribute *Attribute = Primitive->attributes + AttributeIndex;
            cgltf_accessor *Accessor = Attribute->data;
            char *AttributeData = (char *)Accessor->buffer_view->buffer->data + Accessor->buffer_view->offset;
            switch(Attribute->type)
            {
                case cgltf_attribute_type_position:
                {
                    Rr_Vec3 *Position = (Rr_Vec3 *)(AttributeData + Accessor->stride * VertexIndex);
                    NewVertex.Position = *Position;
                }
                break;
                case cgltf_attribute_type_normal:
                {
                    Rr_Vec3 *Normal = (Rr_Vec3 *)(AttributeData + Accessor->stride * VertexIndex);
                    NewVertex.Normal = *Normal;
                }
                break;
                case cgltf_attribute_type_tangent:
                {
                    Rr_Vec3 *Tangent = (Rr_Vec3 *)(AttributeData + Accessor->stride * VertexIndex);
                    NewVertex.Tangent = *Tangent;
                }
                break;
                case cgltf_attribute_type_texcoord:
                {
                    Rr_Vec2 *TexCoord = (Rr_Vec2 *)(AttributeData + Accessor->stride * VertexIndex);
                    NewVertex.TexCoordX = TexCoord->X;
                    NewVertex.TexCoordY = TexCoord->Y;
                }
                break;
                default:
                    break;
            }
        }

        *RR_SLICE_PUSH(&RawMesh.VerticesSlice, NULL) = NewVertex;
    }

    Rr_DestroyArenaScratch(Scratch);

    return RawMesh;
}

static void Rr_CalculateTangents(size_t IndexCount, const Rr_MeshIndexType *Indices, Rr_Vertex *OutVertices)
{
    for(uint32_t Index = 3; Index < IndexCount; Index += 3)
    {
        uint32_t V0Index = Indices[Index - 3];
        uint32_t V1Index = Indices[Index - 2];
        uint32_t V2Index = Indices[Index - 1];

        Rr_Vertex *Vertex0 = &OutVertices[V0Index];
        Rr_Vertex *Vertex1 = &OutVertices[V1Index];
        Rr_Vertex *Vertex2 = &OutVertices[V2Index];

        Rr_Vec3 Tangent = Rr_V3(0.0f, 0.0f, 0.0f);
        Rr_Vec3 Edge0 = Rr_SubV3(Vertex1->Position, Vertex0->Position);
        Rr_Vec3 Edge1 = Rr_SubV3(Vertex2->Position, Vertex0->Position);
        Rr_Vec2 DeltaUV0 = { Vertex1->TexCoordX - Vertex0->TexCoordX, Vertex1->TexCoordY - Vertex0->TexCoordY };
        Rr_Vec2 DeltaUV1 = { Vertex2->TexCoordX - Vertex0->TexCoordX, Vertex2->TexCoordY - Vertex0->TexCoordY };

        float Denominator = DeltaUV0.X * DeltaUV1.Y - DeltaUV1.X * DeltaUV0.Y;
        if(fabsf(Denominator) <= 0.0000001f)
        {
            Tangent = Rr_SubV3(Vertex1->Position, Vertex2->Position);
        }
        else
        {
            float F = 1.0f / Denominator;

            Tangent.X = F * (DeltaUV1.Y * Edge0.X - DeltaUV0.Y * Edge1.X);
            Tangent.Y = F * (DeltaUV1.Y * Edge0.Y - DeltaUV0.Y * Edge1.Y);
            Tangent.Z = F * (DeltaUV1.Y * Edge0.Z - DeltaUV0.Y * Edge1.Z);
        }

        Vertex0->Tangent = Rr_AddV3(Vertex0->Tangent, Tangent);
        Vertex1->Tangent = Rr_AddV3(Vertex1->Tangent, Tangent);
        Vertex2->Tangent = Rr_AddV3(Vertex2->Tangent, Tangent);
    }
}

static Rr_RawMesh Rr_CreateRawMeshFromOBJ(Rr_Asset *Asset, Rr_Arena *Arena)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    RR_SLICE_TYPE(Rr_Vec3) ScratchPositions = { 0 };
    RR_SLICE_TYPE(Rr_Vec4) ScratchColors = { 0 };
    RR_SLICE_TYPE(Rr_Vec2) ScratchTexCoords = { 0 };
    RR_SLICE_TYPE(Rr_Vec3) ScratchNormals = { 0 };
    RR_SLICE_TYPE(Rr_IntVec3) ScratchIndices = { 0 };

    Rr_RawMesh RawMesh = { 0 };

    size_t CurrentIndex = 0;
    char *EndPos;
    while(CurrentIndex < Asset->Length)
    {
        switch(Asset->Data[CurrentIndex])
        {
            case 'v':
            {
                CurrentIndex++;
                switch(Asset->Data[CurrentIndex])
                {
                    case ' ':
                    {
                        Rr_Vec3 NewPosition;
                        NewPosition.X = (float)SDL_strtod(Asset->Data + CurrentIndex, &EndPos);
                        NewPosition.Y = (float)SDL_strtod(EndPos, &EndPos);
                        NewPosition.Z = (float)SDL_strtod(EndPos, &EndPos);

                        *RR_SLICE_PUSH(&ScratchPositions, Scratch.Arena) = NewPosition;

                        if(*EndPos == ' ')
                        {
                            Rr_Vec4 NewColor = { 0 };
                            NewColor.X = (float)SDL_strtod(EndPos, &EndPos);
                            NewColor.Y = (float)SDL_strtod(EndPos, &EndPos);
                            NewColor.Z = (float)SDL_strtod(EndPos, &EndPos);

                            *RR_SLICE_PUSH(&ScratchColors, Scratch.Arena) = NewColor;
                        }
                    }
                    break;
                    case 't':
                    {
                        CurrentIndex++;
                        Rr_Vec2 NewTexCoord;
                        NewTexCoord.X = (float)SDL_strtod(Asset->Data + CurrentIndex, &EndPos);
                        NewTexCoord.Y = (float)SDL_strtod(EndPos, &EndPos);

                        *RR_SLICE_PUSH(&ScratchTexCoords, Scratch.Arena) = NewTexCoord;
                    }
                    break;
                    case 'n':
                    {
                        CurrentIndex++;
                        Rr_Vec3 NewNormal;
                        NewNormal.X = (float)SDL_strtod(Asset->Data + CurrentIndex, &EndPos);
                        NewNormal.Y = (float)SDL_strtod(EndPos, &EndPos);
                        NewNormal.Z = (float)SDL_strtod(EndPos, &EndPos);

                        *RR_SLICE_PUSH(&ScratchNormals, Scratch.Arena) = NewNormal;
                    }
                    break;
                }
            }
            break;
            case 'f':
            {
                CurrentIndex++;
                Rr_IntVec3 OBJIndices[3] = { 0 };
                OBJIndices[0].X = (int32_t)SDL_strtoul(Asset->Data + CurrentIndex, &EndPos, 10);
                OBJIndices[0].Y = (int32_t)SDL_strtoul(EndPos + 1, &EndPos, 0);
                OBJIndices[0].Z = (int32_t)SDL_strtoul(EndPos + 1, &EndPos, 0);
                OBJIndices[1].X = (int32_t)SDL_strtoul(EndPos + 1, &EndPos, 0);
                OBJIndices[1].Y = (int32_t)SDL_strtoul(EndPos + 1, &EndPos, 0);
                OBJIndices[1].Z = (int32_t)SDL_strtoul(EndPos + 1, &EndPos, 0);
                OBJIndices[2].X = (int32_t)SDL_strtoul(EndPos + 1, &EndPos, 0);
                OBJIndices[2].Y = (int32_t)SDL_strtoul(EndPos + 1, &EndPos, 0);
                OBJIndices[2].Z = (int32_t)SDL_strtoul(EndPos + 1, &EndPos, 0);
                for(size_t Index = 0; Index < 3; Index++)
                {
                    OBJIndices[Index].X--;
                    OBJIndices[Index].Y--;
                    OBJIndices[Index].Z--;

                    size_t ExistingOBJIndex = SIZE_MAX;
                    for(size_t I = 0; I < ScratchIndices.Count; I++)
                    {
                        if(Rr_EqIV3(OBJIndices[Index], ScratchIndices.Data[I]))
                        {
                            ExistingOBJIndex = I;
                            break;
                        }
                    }
                    if(ExistingOBJIndex == SIZE_MAX)
                    {
                        Rr_Vec2 *TexCoord = &ScratchTexCoords.Data[OBJIndices[Index].Y];
                        Rr_Vertex NewVertex = { 0 };
                        NewVertex.Position = ScratchPositions.Data[OBJIndices[Index].X];
                        // NewVertex.Color =
                        // ScratchColors.Data[OBJIndices[Index].X];
                        NewVertex.Normal = ScratchNormals.Data[OBJIndices[Index].Z];
                        NewVertex.TexCoordX = (*TexCoord).X;
                        // NewVertex.TexCoordY = (*TexCoord).Y;
                        NewVertex.TexCoordY = 1.0f - (*TexCoord).Y;
                        *RR_SLICE_PUSH(&RawMesh.VerticesSlice, Arena) = NewVertex;

                        *RR_SLICE_PUSH(&ScratchIndices, Scratch.Arena) = OBJIndices[Index];

                        /* Add freshly added vertex index */
                        *RR_SLICE_PUSH(&RawMesh.IndicesSlice, Arena) =
                            (Rr_MeshIndexType){ RawMesh.VerticesSlice.Count - 1 };
                    }
                    else
                    {
                        *RR_SLICE_PUSH(&RawMesh.IndicesSlice, Arena) = ExistingOBJIndex;
                    }
                }
            }
            break;
            default:
            {
            }
            break;
        }

        CurrentIndex += strcspn(Asset->Data + CurrentIndex, "\n") + 1;
    }

    Rr_CalculateTangents(RawMesh.IndicesSlice.Count, RawMesh.IndicesSlice.Data, RawMesh.VerticesSlice.Data);

    Rr_DestroyArenaScratch(Scratch);

    return RawMesh;
}

Rr_Primitive *Rr_CreatePrimitive(Rr_App *App, Rr_UploadContext *UploadContext, Rr_RawMesh *RawMesh)
{
    Rr_Primitive *Primitive = Rr_CreateObject(App);

    Primitive->IndexCount = RawMesh->IndicesSlice.Count;

    size_t VertexBufferSize = sizeof(Rr_Vertex) * RawMesh->VerticesSlice.Count;
    size_t IndexBufferSize = sizeof(Rr_MeshIndexType) * Primitive->IndexCount;

    Primitive->VertexBuffer =
        Rr_CreateBuffer(App, VertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO, false);

    Primitive->IndexBuffer =
        Rr_CreateBuffer(App, IndexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO, false);

    Rr_UploadBuffer(
        App,
        UploadContext,
        Primitive->VertexBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        0,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
        RR_MAKE_DATA(RawMesh->VerticesSlice.Data, VertexBufferSize));

    Rr_UploadBuffer(
        App,
        UploadContext,
        Primitive->IndexBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        0,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        VK_ACCESS_INDEX_READ_BIT,
        RR_MAKE_DATA(RawMesh->IndicesSlice.Data, IndexBufferSize));

    return Primitive;
}

void Rr_DestroyPrimitive(Rr_App *App, Rr_Primitive *Primitive)
{
    if(Primitive == NULL)
    {
        return;
    }

    Rr_DestroyBuffer(App, Primitive->IndexBuffer);
    Rr_DestroyBuffer(App, Primitive->VertexBuffer);

    Rr_DestroyObject(App, Primitive);
}

Rr_StaticMesh *Rr_CreateStaticMesh(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    Rr_RawMesh *RawMeshes,
    size_t RawMeshCount)
{
    Rr_StaticMesh *StaticMesh = Rr_CreateObject(App);

    for(size_t Index = 0; Index < RawMeshCount; ++Index)
    {
        StaticMesh->Primitives[Index] = Rr_CreatePrimitive(App, UploadContext, RawMeshes + Index);
    }
    StaticMesh->PrimitiveCount = RawMeshCount;

    return StaticMesh;
}

void Rr_DestroyStaticMesh(Rr_App *App, Rr_StaticMesh *StaticMesh)
{
    if(StaticMesh == NULL)
    {
        return;
    }

    for(size_t Index = 0; Index < StaticMesh->PrimitiveCount; ++Index)
    {
        Rr_DestroyPrimitive(App, StaticMesh->Primitives[Index]);
    }

    Rr_DestroyObject(App, StaticMesh);
}

Rr_StaticMesh *Rr_CreateStaticMeshGLTF(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    Rr_AssetRef AssetRef,
    Rr_GLTFLoader *Loader,
    size_t MeshIndex,
    Rr_Arena *Arena)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    Rr_Asset Asset = Rr_LoadAsset(AssetRef);

    cgltf_options Options = { .memory = Rr_GetCGLTFMemoryOptions(Scratch.Arena) };
    cgltf_data *Data = NULL;
    cgltf_mesh *Mesh = Rr_ParseGLTFMesh(&Asset, MeshIndex, &Options, &Data);
    cgltf_load_buffers(&Options, Data, NULL);

    Rr_RawMesh *RawMeshes = RR_ALLOC_STRUCT_COUNT(Scratch.Arena, Rr_RawMesh, Mesh->primitives_count);

    // Rr_Material *Materials[RR_MESH_MAX_PRIMITIVES] = { 0 };
    //
    // for(size_t Index = 0; Index < Mesh->primitives_count; ++Index)
    // {
    //     cgltf_primitive *Primitive = Mesh->primitives + Index;
    //
    //     if(Loader != NULL && Primitive->material != NULL)
    //     {
    //         Rr_Image *Textures[RR_MAX_TEXTURES_PER_MATERIAL] = { 0 };
    //
    //         cgltf_material *CGLTFMaterial = Primitive->material;
    //         if(CGLTFMaterial->has_pbr_metallic_roughness)
    //         {
    //             cgltf_texture *BaseColorTexture = CGLTFMaterial->pbr_metallic_roughness.base_color_texture.texture;
    //             if(BaseColorTexture)
    //             {
    //                 if(strcmp(BaseColorTexture->image->mime_type, "image/png") == 0)
    //                 {
    //                     char *PNGData = (char *)BaseColorTexture->image->buffer_view->buffer->data +
    //                                     BaseColorTexture->image->buffer_view->offset;
    //                     size_t PNGSize = BaseColorTexture->image->buffer_view->size;
    //
    //                     Textures[Loader->BaseTexture] =
    //                         Rr_CreateColorImageFromPNGMemory(App, UploadContext, PNGData, PNGSize, false);
    //                 }
    //             }
    //         }
    //         if(CGLTFMaterial->normal_texture.texture != NULL)
    //         {
    //             cgltf_texture *NormalTexture = CGLTFMaterial->normal_texture.texture;
    //             if(strcmp(NormalTexture->image->mime_type, "image/png") == 0)
    //             {
    //                 char *PNGData = (char *)NormalTexture->image->buffer_view->buffer->data +
    //                                 NormalTexture->image->buffer_view->offset;
    //                 size_t PNGSize = NormalTexture->image->buffer_view->size;
    //
    //                 Textures[Loader->NormalTexture] =
    //                     Rr_CreateColorImageFromPNGMemory(App, UploadContext, PNGData, PNGSize, false);
    //             }
    //         }
    //
    //         Materials[Index] = Rr_CreateMaterial(App, Loader->GenericPipeline, Textures,
    //         RR_MAX_TEXTURES_PER_MATERIAL); Materials[Index]->bOwning = true;
    //     }
    //
    //     Rr_RawMesh *RawMesh = RawMeshes + Index;
    //     *RawMesh = Rr_CreateRawMeshFromGLTFPrimitive(Primitive, Scratch.Arena);
    // }

    Rr_StaticMesh *StaticMesh = Rr_CreateStaticMesh(App, UploadContext, RawMeshes, Mesh->primitives_count);

    cgltf_free(Data);

    Rr_DestroyArenaScratch(Scratch);

    return StaticMesh;
}

Rr_StaticMesh *Rr_CreateStaticMeshOBJ(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    Rr_AssetRef AssetRef,
    Rr_Arena *Arena)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    Rr_Asset Asset = Rr_LoadAsset(AssetRef);

    Rr_RawMesh RawMesh = Rr_CreateRawMeshFromOBJ(&Asset, Scratch.Arena);

    Rr_StaticMesh *StaticMesh = Rr_CreateStaticMesh(App, UploadContext, &RawMesh, 1);

    Rr_DestroyArenaScratch(Scratch);

    return StaticMesh;
}

void Rr_GetStaticMeshSizeOBJ(Rr_AssetRef AssetRef, Rr_Arena *Arena, Rr_LoadSize *OutLoadSize)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    Rr_Asset Asset = Rr_LoadAsset(AssetRef);

    Rr_RawMesh RawMesh = Rr_CreateRawMeshFromOBJ(&Asset, Scratch.Arena);

    size_t VertexBufferSize = sizeof(Rr_Vertex) * RawMesh.VerticesSlice.Count;
    size_t IndexBufferSize = sizeof(Rr_MeshIndexType) * RawMesh.IndicesSlice.Count;

    OutLoadSize->StagingBufferSize += VertexBufferSize + IndexBufferSize;
    OutLoadSize->BufferCount += 2;

    Rr_DestroyArenaScratch(Scratch);
}

void Rr_GetStaticMeshSizeGLTF(
    Rr_AssetRef AssetRef,
    Rr_GLTFLoader *Loader,
    size_t MeshIndex,
    Rr_Arena *Arena,
    Rr_LoadSize *OutLoadSize)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    Rr_Asset Asset = Rr_LoadAsset(AssetRef);

    cgltf_options Options = { .memory = Rr_GetCGLTFMemoryOptions(Scratch.Arena) };
    cgltf_data *Data = NULL;
    cgltf_mesh *Mesh = Rr_ParseGLTFMesh(&Asset, MeshIndex, &Options, &Data);
    cgltf_load_buffers(&Options, Data, NULL);

    size_t VertexBufferSize = 0;
    size_t IndexBufferSize = 0;

    for(size_t Index = 0; Index < Mesh->primitives_count; ++Index)
    {
        cgltf_primitive *Primitive = Mesh->primitives + Index;

        VertexBufferSize += sizeof(Rr_Vertex) * Primitive->attributes->data->count;
        IndexBufferSize += sizeof(Rr_MeshIndexType) * Primitive->indices->count;

        if(Loader != NULL && Primitive->material != NULL)
        {
            cgltf_material *CGLTFMaterial = Primitive->material;
            if(CGLTFMaterial->has_pbr_metallic_roughness)
            {
                cgltf_texture *BaseColorTexture = CGLTFMaterial->pbr_metallic_roughness.base_color_texture.texture;
                if(BaseColorTexture)
                {
                    if(strcmp(BaseColorTexture->image->mime_type, "image/png") == 0)
                    {
                        char *PNGData = (char *)BaseColorTexture->image->buffer_view->buffer->data +
                                        BaseColorTexture->image->buffer_view->offset;
                        size_t PNGSize = BaseColorTexture->image->buffer_view->size;

                        Rr_GetImageSizePNGMemory(PNGData, PNGSize, Scratch.Arena, OutLoadSize);
                    }
                }
            }
            if(CGLTFMaterial->normal_texture.texture != NULL)
            {
                cgltf_texture *NormalTexture = CGLTFMaterial->normal_texture.texture;
                if(strcmp(NormalTexture->image->mime_type, "image/png") == 0)
                {
                    char *PNGData = (char *)NormalTexture->image->buffer_view->buffer->data +
                                    NormalTexture->image->buffer_view->offset;
                    size_t PNGSize = NormalTexture->image->buffer_view->size;

                    Rr_GetImageSizePNGMemory(PNGData, PNGSize, Scratch.Arena, OutLoadSize);
                }
            }
        }
    }

    cgltf_free(Data);

    OutLoadSize->StagingBufferSize += VertexBufferSize + IndexBufferSize;
    OutLoadSize->BufferCount += 2;

    Rr_DestroyArenaScratch(Scratch);
}
