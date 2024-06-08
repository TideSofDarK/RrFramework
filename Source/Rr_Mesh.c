#include "Rr_Mesh.h"

#include "Rr_UploadContext.h"
#include "Rr_Image.h"
#include "Rr_App.h"
#include "Rr_Material.h"

#include <SDL3/SDL.h>

#include <stb/stb_image.h>

#include <cgltf/cgltf.h>

static void* Rr_CGLTFArenaAlloc(void* Arena, cgltf_size Size)
{
    return Rr_ArenaAllocOne((Rr_Arena*)Arena, Size);
}

static void Rr_CGLTFArenaFree(void* Arena, void* Ptr)
{
    /* no-op */
}

static cgltf_memory_options Rr_GetCGLTFMemoryOptions(Rr_Arena* Arena)
{
    return (cgltf_memory_options){
        .alloc_func = Rr_CGLTFArenaAlloc,
        .free_func = Rr_CGLTFArenaFree,
        .user_data = Arena
    };
}

static cgltf_mesh* Rr_ParseGLTFMesh(Rr_Asset* Asset, usize MeshIndex, cgltf_options* Options, cgltf_data** OutData)
{
    cgltf_data* Data = NULL;
    cgltf_result Result = cgltf_parse(Options, Asset->Data, Asset->Length, &Data);
    if (Result != cgltf_result_success)
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "[glTF] Error loading glTF asset!");
        abort();
    }

    cgltf_mesh* Mesh = Data->meshes + MeshIndex;

    if (MeshIndex + 1 > Data->meshes_count || Mesh->primitives_count < 1)
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "[glTF] Mesh contains no geometry!");
        abort();
    }

    if (Data->meshes->primitives_count > RR_MESH_MAX_PRIMITIVES)
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "[glTF] Exceeding max mesh primitives count!");
        abort();
    }

    *OutData = Data;

    return Mesh;
}

static Rr_RawMesh Rr_CreateRawMeshFromGLTFPrimitive(cgltf_primitive* Primitive, Rr_Arena* Arena)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    usize VertexCount = Primitive->attributes->data->count;
    usize IndexCount = Primitive->indices->count;

    Rr_RawMesh RawMesh = { 0 };
    Rr_SliceReserve(&RawMesh.VerticesSlice, VertexCount, Arena);
    Rr_SliceReserve(&RawMesh.IndicesSlice, IndexCount, Arena);

    byte* IndexData = (byte*)Primitive->indices->buffer_view->buffer->data + Primitive->indices->buffer_view->offset;
    if (Primitive->indices->component_type == cgltf_component_type_r_16u)
    {
        u16* Indices = (u16*)IndexData;
        for (usize Index = 0; Index < IndexCount; ++Index)
        {
            u32 Converted = *(Indices + Index);
            *Rr_SlicePush(&RawMesh.IndicesSlice, NULL) = Converted;
        }
    }
    else
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Unsupported index type!");
        abort();
    }

    for (usize VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
    {
        Rr_Vertex NewVertex = { 0 };

        for (usize AttributeIndex = 0; AttributeIndex < Primitive->attributes_count; ++AttributeIndex)
        {
            cgltf_attribute* Attribute = Primitive->attributes + AttributeIndex;
            cgltf_accessor* Accessor = Attribute->data;
            byte* AttributeData = (byte*)Accessor->buffer_view->buffer->data + Accessor->buffer_view->offset;
            switch (Attribute->type)
            {
                case cgltf_attribute_type_position:
                {
                    Rr_Vec3* Position = (Rr_Vec3*)(AttributeData + Accessor->stride * VertexIndex);
                    NewVertex.Position = *Position;
                }
                break;
                case cgltf_attribute_type_normal:
                {
                    Rr_Vec3* Normal = (Rr_Vec3*)(AttributeData + Accessor->stride * VertexIndex);
                    NewVertex.Normal = *Normal;
                }
                break;
                case cgltf_attribute_type_tangent:
                {
                    Rr_Vec3* Tangent = (Rr_Vec3*)(AttributeData + Accessor->stride * VertexIndex);
                    NewVertex.Tangent.RGB = *Tangent;
                }
                break;
                case cgltf_attribute_type_texcoord:
                {
                    Rr_Vec2* TexCoord = (Rr_Vec2*)(AttributeData + Accessor->stride * VertexIndex);
                    NewVertex.TexCoordX = TexCoord->X;
                    NewVertex.TexCoordY = TexCoord->Y;
                }
                break;
                default:
                    break;
            }
        }

        *Rr_SlicePush(&RawMesh.VerticesSlice, NULL) = NewVertex;
    }

    Rr_DestroyArenaScratch(Scratch);

    return RawMesh;
}

static Rr_RawMesh Rr_CreateRawMeshFromOBJ(
    Rr_Asset* Asset,
    Rr_Arena* Arena)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    Rr_SliceType(Rr_Vec3) ScratchPositions = { 0 };
    Rr_SliceType(Rr_Vec4) ScratchColors = { 0 };
    Rr_SliceType(Rr_Vec2) ScratchTexCoords = { 0 };
    Rr_SliceType(Rr_Vec3) ScratchNormals = { 0 };
    Rr_SliceType(Rr_IntVec3) ScratchIndices = { 0 };

    Rr_RawMesh RawMesh = { 0 };

    usize CurrentIndex = 0;
    byte* EndPos;
    while (CurrentIndex < Asset->Length)
    {
        switch (Asset->Data[CurrentIndex])
        {
            case 'v':
            {
                CurrentIndex++;
                switch (Asset->Data[CurrentIndex])
                {
                    case ' ':
                    {
                        Rr_Vec3 NewPosition;
                        NewPosition.X = (float)SDL_strtod(Asset->Data + CurrentIndex, &EndPos);
                        NewPosition.Y = (float)SDL_strtod(EndPos, &EndPos);
                        NewPosition.Z = (float)SDL_strtod(EndPos, &EndPos);

                        *Rr_SlicePush(&ScratchPositions, Scratch.Arena) = NewPosition;

                        if (*EndPos == ' ')
                        {
                            Rr_Vec4 NewColor = { 0 };
                            NewColor.X = (float)SDL_strtod(EndPos, &EndPos);
                            NewColor.Y = (float)SDL_strtod(EndPos, &EndPos);
                            NewColor.Z = (float)SDL_strtod(EndPos, &EndPos);

                            *Rr_SlicePush(&ScratchColors, Scratch.Arena) = NewColor;
                        }
                    }
                    break;
                    case 't':
                    {
                        CurrentIndex++;
                        Rr_Vec2 NewTexCoord;
                        NewTexCoord.X = (float)SDL_strtod(Asset->Data + CurrentIndex, &EndPos);
                        NewTexCoord.Y = (float)SDL_strtod(EndPos, &EndPos);

                        *Rr_SlicePush(&ScratchTexCoords, Scratch.Arena) = NewTexCoord;
                    }
                    break;
                    case 'n':
                    {
                        CurrentIndex++;
                        Rr_Vec3 NewNormal;
                        NewNormal.X = (float)SDL_strtod(Asset->Data + CurrentIndex, &EndPos);
                        NewNormal.Y = (float)SDL_strtod(EndPos, &EndPos);
                        NewNormal.Z = (float)SDL_strtod(EndPos, &EndPos);

                        *Rr_SlicePush(&ScratchNormals, Scratch.Arena) = NewNormal;
                    }
                    break;
                }
            }
            break;
            case 'f':
            {
                CurrentIndex++;
                Rr_IntVec3 OBJIndices[3] = { 0 };
                OBJIndices[0].X = (i32)SDL_strtoul(Asset->Data + CurrentIndex, &EndPos, 10);
                OBJIndices[0].Y = (i32)SDL_strtoul(EndPos + 1, &EndPos, 0);
                OBJIndices[0].Z = (i32)SDL_strtoul(EndPos + 1, &EndPos, 0);
                OBJIndices[1].X = (i32)SDL_strtoul(EndPos + 1, &EndPos, 0);
                OBJIndices[1].Y = (i32)SDL_strtoul(EndPos + 1, &EndPos, 0);
                OBJIndices[1].Z = (i32)SDL_strtoul(EndPos + 1, &EndPos, 0);
                OBJIndices[2].X = (i32)SDL_strtoul(EndPos + 1, &EndPos, 0);
                OBJIndices[2].Y = (i32)SDL_strtoul(EndPos + 1, &EndPos, 0);
                OBJIndices[2].Z = (i32)SDL_strtoul(EndPos + 1, &EndPos, 0);
                for (usize Index = 0; Index < 3; Index++)
                {
                    OBJIndices[Index].X--;
                    OBJIndices[Index].Y--;
                    OBJIndices[Index].Z--;

                    usize ExistingOBJIndex = SIZE_MAX;
                    for (usize I = 0; I < Rr_SliceLength(&ScratchIndices); I++)
                    {
                        if (Rr_EqIV3(OBJIndices[Index], ScratchIndices.Data[I]))
                        {
                            ExistingOBJIndex = I;
                            break;
                        }
                    }
                    if (ExistingOBJIndex == SIZE_MAX)
                    {
                        Rr_Vec2* TexCoord = &ScratchTexCoords.Data[OBJIndices[Index].Y];
                        Rr_Vertex NewVertex = { 0 };
                        NewVertex.Position = ScratchPositions.Data[OBJIndices[Index].X];
                        // NewVertex.Color = ScratchColors.Data[OBJIndices[Index].X];
                        NewVertex.Normal = ScratchNormals.Data[OBJIndices[Index].Z];
                        NewVertex.TexCoordX = (*TexCoord).X;
                        NewVertex.TexCoordY = 1.0f - (*TexCoord).Y;
                        *Rr_SlicePush(&RawMesh.VerticesSlice, Arena) = NewVertex;

                        *Rr_SlicePush(&ScratchIndices, Scratch.Arena) = OBJIndices[Index];

                        /* Add freshly added vertex index */
                        *Rr_SlicePush(&RawMesh.IndicesSlice, Arena) = (Rr_MeshIndexType){ Rr_SliceLength(&RawMesh.VerticesSlice) - 1 };
                    }
                    else
                    {
                        *Rr_SlicePush(&RawMesh.IndicesSlice, Arena) = ExistingOBJIndex;
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

    Rr_DestroyArenaScratch(Scratch);

    return RawMesh;
}

Rr_Primitive* Rr_CreatePrimitive(
    Rr_App* App,
    Rr_UploadContext* UploadContext,
    Rr_RawMesh* RawMesh)
{
    Rr_Renderer* Renderer = &App->Renderer;

    Rr_Primitive* Primitive = Rr_CreateObject(&App->Renderer.ObjectStorage);

    Primitive->IndexCount = Rr_SliceLength(&RawMesh->IndicesSlice);

    usize VertexBufferSize = sizeof(Rr_Vertex) * Rr_SliceLength(&RawMesh->VerticesSlice);
    usize IndexBufferSize = sizeof(Rr_MeshIndexType) * Primitive->IndexCount;

    Primitive->VertexBuffer = Rr_CreateBuffer(
        App,
        VertexBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO,
        false);

    Primitive->IndexBuffer = Rr_CreateBuffer(
        App,
        IndexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO,
        false);

    Rr_UploadBuffer(
        App,
        UploadContext,
        Primitive->VertexBuffer->Handle,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        0,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
        RawMesh->VerticesSlice.Data,
        VertexBufferSize);

    Rr_UploadBuffer(
        App,
        UploadContext,
        Primitive->IndexBuffer->Handle,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        0,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        VK_ACCESS_INDEX_READ_BIT,
        RawMesh->IndicesSlice.Data,
        IndexBufferSize);

    return Primitive;
}

void Rr_DestroyPrimitive(Rr_App* App, Rr_Primitive* Primitive)
{
    if (Primitive == NULL)
    {
        return;
    }

    Rr_Renderer* Renderer = &App->Renderer;

    Rr_DestroyBuffer(App, Primitive->IndexBuffer);
    Rr_DestroyBuffer(App, Primitive->VertexBuffer);

    Rr_DestroyObject(&Renderer->ObjectStorage, Primitive);
}

Rr_StaticMesh* Rr_CreateStaticMesh(
    Rr_App* App,
    Rr_UploadContext* UploadContext,
    Rr_RawMesh* RawMeshes,
    usize RawMeshCount,
    Rr_Material** Materials,
    usize MaterialCount)
{
    Rr_Renderer* Renderer = &App->Renderer;
    Rr_StaticMesh* StaticMesh = Rr_CreateObject(&Renderer->ObjectStorage);

    for (usize Index = 0; Index < RawMeshCount; ++Index)
    {
        StaticMesh->Primitives[Index] = Rr_CreatePrimitive(App, UploadContext, RawMeshes + Index);
    }
    StaticMesh->PrimitiveCount = RawMeshCount;

    if (Materials != NULL)
    {
        for (usize Index = 0; Index < MaterialCount; ++Index)
        {
            StaticMesh->Materials[Index] = Materials[Index];
        }
    }
    StaticMesh->MaterialCount = MaterialCount;

    return StaticMesh;
}

void Rr_DestroyStaticMesh(Rr_App* App, Rr_StaticMesh* StaticMesh)
{
    if (StaticMesh == NULL)
    {
        return;
    }

    Rr_Renderer* Renderer = &App->Renderer;

    for (usize Index = 0; Index < StaticMesh->PrimitiveCount; ++Index)
    {
        Rr_DestroyPrimitive(App, StaticMesh->Primitives[Index]);
    }

    for (usize Index = 0; Index < StaticMesh->MaterialCount; ++Index)
    {
        Rr_DestroyMaterial(App, StaticMesh->Materials[Index]);
    }

    Rr_DestroyObject(&Renderer->ObjectStorage, StaticMesh);
}

Rr_StaticMesh* Rr_CreateStaticMeshGLTF(
    Rr_App* App,
    Rr_UploadContext* UploadContext,
    Rr_AssetRef AssetRef,
    Rr_GLTFLoader* Loader,
    usize MeshIndex,
    Rr_Arena* Arena)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    Rr_Asset Asset = Rr_LoadAsset(AssetRef);

    cgltf_options Options = { .memory = Rr_GetCGLTFMemoryOptions(Scratch.Arena) };
    cgltf_data* Data = NULL;
    cgltf_mesh* Mesh = Rr_ParseGLTFMesh(&Asset, MeshIndex, &Options, &Data);
    cgltf_load_buffers(&Options, Data, NULL);

    Rr_RawMesh* RawMeshes = Rr_StackAlloc(Rr_RawMesh, Mesh->primitives_count);

    Rr_Material* Materials[RR_MESH_MAX_PRIMITIVES] = { 0 };

    for (usize Index = 0; Index < Mesh->primitives_count; ++Index)
    {
        cgltf_primitive* Primitive = Mesh->primitives + Index;

        if (Loader != NULL && Primitive->material != NULL)
        {
            Rr_Image* Textures[RR_MAX_TEXTURES_PER_MATERIAL] = { 0 };

            cgltf_material* CGLTFMaterial = Primitive->material;
            if (CGLTFMaterial->has_pbr_metallic_roughness)
            {
                cgltf_texture* BaseColorTexture = CGLTFMaterial->pbr_metallic_roughness.base_color_texture.texture;
                if (BaseColorTexture)
                {
                    if (SDL_strcmp(BaseColorTexture->image->mime_type, "image/png") == 0)
                    {
                        byte* PNGData = (byte*)BaseColorTexture->image->buffer_view->buffer->data + BaseColorTexture->image->buffer_view->offset;
                        usize PNGSize = BaseColorTexture->image->buffer_view->size;

                        Textures[Loader->BaseTexture] = Rr_CreateColorImageFromPNGMemory(
                            App,
                            UploadContext,
                            PNGData,
                            PNGSize,
                            false);
                    }
                }
            }
            if (CGLTFMaterial->normal_texture.texture != NULL)
            {
                cgltf_texture* NormalTexture = CGLTFMaterial->normal_texture.texture;
                if (SDL_strcmp(NormalTexture->image->mime_type, "image/png") == 0)
                {
                    byte* PNGData = (byte*)NormalTexture->image->buffer_view->buffer->data + NormalTexture->image->buffer_view->offset;
                    usize PNGSize = NormalTexture->image->buffer_view->size;

                    Textures[Loader->NormalTexture] = Rr_CreateColorImageFromPNGMemory(
                        App,
                        UploadContext,
                        PNGData,
                        PNGSize,
                        false);
                }
            }

            Materials[Index] = Rr_CreateMaterial(
                App,
                Loader->GenericPipeline,
                Textures,
                RR_MAX_TEXTURES_PER_MATERIAL);
            Materials[Index]->bOwning = true;
        }

        Rr_RawMesh* RawMesh = RawMeshes + Index;
        *RawMesh = Rr_CreateRawMeshFromGLTFPrimitive(Primitive, Scratch.Arena);
    }

    Rr_StaticMesh* StaticMesh = Rr_CreateStaticMesh(App, UploadContext, RawMeshes, Mesh->primitives_count, Materials, RR_MESH_MAX_PRIMITIVES);

    cgltf_free(Data);

    Rr_DestroyArenaScratch(Scratch);

    Rr_StackFree(RawMeshes);

    return StaticMesh;
}

Rr_StaticMesh* Rr_CreateStaticMeshOBJ(
    Rr_App* App,
    Rr_UploadContext* UploadContext,
    Rr_AssetRef AssetRef,
    Rr_Arena* Arena)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    Rr_Asset Asset = Rr_LoadAsset(AssetRef);

    Rr_RawMesh RawMesh = Rr_CreateRawMeshFromOBJ(&Asset, Scratch.Arena);

    Rr_StaticMesh* StaticMesh = Rr_CreateStaticMesh(App, UploadContext, &RawMesh, 1, NULL, 0);

    Rr_DestroyArenaScratch(Scratch);

    return StaticMesh;
}

void Rr_GetStaticMeshSizeOBJ(
    Rr_AssetRef AssetRef,
    Rr_Arena* Arena,
    Rr_LoadSize* OutLoadSize)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    Rr_Asset Asset = Rr_LoadAsset(AssetRef);

    Rr_RawMesh RawMesh = Rr_CreateRawMeshFromOBJ(&Asset, Scratch.Arena);

    usize VertexBufferSize = sizeof(Rr_Vertex) * Rr_SliceLength(&RawMesh.VerticesSlice);
    usize IndexBufferSize = sizeof(Rr_MeshIndexType) * Rr_SliceLength(&RawMesh.IndicesSlice);

    OutLoadSize->StagingBufferSize += VertexBufferSize + IndexBufferSize;
    OutLoadSize->BufferCount += 2;

    Rr_DestroyArenaScratch(Scratch);
}

void Rr_GetStaticMeshSizeGLTF(
    Rr_AssetRef AssetRef,
    Rr_GLTFLoader* Loader,
    usize MeshIndex,
    Rr_Arena* Arena,
    Rr_LoadSize* OutLoadSize)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    Rr_Asset Asset = Rr_LoadAsset(AssetRef);

    cgltf_options Options = { .memory = Rr_GetCGLTFMemoryOptions(Scratch.Arena) };
    cgltf_data* Data = NULL;
    cgltf_mesh* Mesh = Rr_ParseGLTFMesh(&Asset, MeshIndex, &Options, &Data);
    cgltf_load_buffers(&Options, Data, NULL);

    usize VertexBufferSize = 0;
    usize IndexBufferSize = 0;

    for (usize Index = 0; Index < Mesh->primitives_count; ++Index)
    {
        cgltf_primitive* Primitive = Mesh->primitives + Index;

        VertexBufferSize += sizeof(Rr_Vertex) * Primitive->attributes->data->count;
        IndexBufferSize += sizeof(Rr_MeshIndexType) * Primitive->indices->count;

        if (Loader != NULL && Primitive->material != NULL)
        {
            cgltf_material* CGLTFMaterial = Primitive->material;
            if (CGLTFMaterial->has_pbr_metallic_roughness)
            {
                cgltf_texture* BaseColorTexture = CGLTFMaterial->pbr_metallic_roughness.base_color_texture.texture;
                if (BaseColorTexture)
                {
                    if (SDL_strcmp(BaseColorTexture->image->mime_type, "image/png") == 0)
                    {
                        byte* PNGData = (byte*)BaseColorTexture->image->buffer_view->buffer->data + BaseColorTexture->image->buffer_view->offset;
                        usize PNGSize = BaseColorTexture->image->buffer_view->size;

                        Rr_GetImageSizePNGMemory(PNGData, PNGSize, Scratch.Arena, OutLoadSize);
                    }
                }
            }
            if (CGLTFMaterial->normal_texture.texture != NULL)
            {
                cgltf_texture* NormalTexture = CGLTFMaterial->normal_texture.texture;
                if (SDL_strcmp(NormalTexture->image->mime_type, "image/png") == 0)
                {
                    byte* PNGData = (byte*)NormalTexture->image->buffer_view->buffer->data + NormalTexture->image->buffer_view->offset;
                    usize PNGSize = NormalTexture->image->buffer_view->size;

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
