#include "Rr_Mesh.h"

#include "Rr_Array.h"
#include "Rr_Vulkan.h"
#include "Rr_Buffer.h"
#include "Rr_Memory.h"
#include "Rr_Types.h"
#include "Rr_Material.h"

#include <SDL_stdinc.h>

#include <cgltf/cgltf.h>

static cgltf_mesh* ParseGLTFMesh(const Rr_Asset* Asset, size_t MeshIndex, cgltf_data** OutData)
{
    cgltf_options Options = { 0 };
    cgltf_data* Data = NULL;
    cgltf_result Result = cgltf_parse(&Options, Asset->Data, Asset->Length, &Data);
    if (Result != cgltf_result_success)
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Error loading glTF asset!");
        abort();
    }

    cgltf_mesh* Mesh = Data->meshes + MeshIndex;

    if (MeshIndex + 1 > Data->meshes_count || Mesh->primitives_count < 1)
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Mesh contains no geometry!");
        abort();
    }

    if (Data->meshes->primitives_count > RR_MESH_MAX_PRIMITIVES)
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Exceeding max mesh primitives count!");
        abort();
    }

    *OutData = Data;

    return Mesh;
}

static void GLTFPrimitiveToRawMesh(cgltf_primitive* Primitive, Rr_RawMesh* RawMesh)
{
    size_t VertexCount = Primitive->attributes->data->count;
    size_t IndexCount = Primitive->indices->count;

    Rr_ArrayInit(RawMesh->Vertices, Rr_Vertex, VertexCount);
    Rr_ArrayInit(RawMesh->Indices, Rr_MeshIndexType, IndexCount);

    void* IndexData = Primitive->indices->buffer_view->buffer->data + Primitive->indices->buffer_view->offset;
    if (Primitive->indices->component_type == cgltf_component_type_r_16u)
    {
        u16* Indices = (u16*)IndexData;
        for (size_t Index = 0; Index < IndexCount; ++Index)
        {
            u32 Converted = *(Indices + Index);
            Rr_ArrayPush(RawMesh->Indices, &Converted);
        }
    }
    else
    {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Unsupported index type!");
        abort();
    }

    for (size_t VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
    {
        Rr_Vertex NewVertex;

        for (size_t AttributeIndex = 0; AttributeIndex < Primitive->attributes_count; ++AttributeIndex)
        {
            cgltf_attribute* Attribute = Primitive->attributes + AttributeIndex;
            cgltf_accessor* Accessor = Attribute->data;
            void* AttributeData = Accessor->buffer_view->buffer->data + Accessor->buffer_view->offset;
            switch (Attribute->type)
            {
                case cgltf_attribute_type_position:
                {
                    Rr_Vec3* Position = AttributeData + Accessor->stride * VertexIndex;
                    NewVertex.Position = *Position;
                }
                break;
                case cgltf_attribute_type_normal:
                {
                    Rr_Vec3* Normal = AttributeData + Accessor->stride * VertexIndex;
                    NewVertex.Normal = *Normal;
                }
                break;
                case cgltf_attribute_type_tangent:
                {
                    Rr_Vec3* Tangent = AttributeData + Accessor->stride * VertexIndex;
                    NewVertex.Color.RGB = *Tangent;
                }
                break;
                case cgltf_attribute_type_texcoord:
                {
                    Rr_Vec2* TexCoord = AttributeData + Accessor->stride * VertexIndex;
                    NewVertex.TexCoordX = TexCoord->X;
                    NewVertex.TexCoordY = TexCoord->Y;
                }
                break;
                default:
                    break;
            }
        }

        Rr_ArrayPush(RawMesh->Vertices, &NewVertex);
    }
}

static void InitAndUploadMeshBuffers(
    Rr_Renderer* Renderer,
    Rr_UploadContext* UploadContext,
    Rr_RawMesh* RawMesh,
    Rr_MeshBuffers* MeshBuffers)
{
    const size_t VertexBufferSize = sizeof(Rr_Vertex) * Rr_ArrayCount(RawMesh->Vertices);
    const size_t IndexBufferSize = sizeof(Rr_MeshIndexType) * Rr_ArrayCount(RawMesh->Indices);

    MeshBuffers->VertexBuffer = Rr_CreateBuffer(
        Renderer,
        VertexBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO,
        false);

    MeshBuffers->IndexBuffer = Rr_CreateBuffer(
        Renderer,
        IndexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO,
        false);

    Rr_UploadBuffer(
        Renderer,
        UploadContext,
        MeshBuffers->VertexBuffer->Handle,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        0,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
        RawMesh->Vertices,
        VertexBufferSize);

    Rr_UploadBuffer(
        Renderer,
        UploadContext,
        MeshBuffers->IndexBuffer->Handle,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        0,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        VK_ACCESS_INDEX_READ_BIT,
        RawMesh->Indices,
        IndexBufferSize);
}

void Rr_DestroyMeshBuffers(Rr_App* App, Rr_MeshBuffers* MeshBuffers)
{
    if (MeshBuffers == NULL)
    {
        return;
    }

    Rr_Renderer* Renderer = &App->Renderer;

    Rr_DestroyBuffer(Renderer, MeshBuffers->IndexBuffer);
    Rr_DestroyBuffer(Renderer, MeshBuffers->VertexBuffer);
}

Rr_StaticMesh* Rr_CreateStaticMesh(
    Rr_App* App,
    Rr_UploadContext* UploadContext,
    Rr_RawMesh* RawMeshes,
    size_t RawMeshCount,
    Rr_Material** Material,
    size_t MaterialCount)
{
    Rr_Renderer* Renderer = &App->Renderer;
    Rr_StaticMesh* StaticMesh = Rr_CreateObject(Renderer);

    for (size_t Index = 0; Index < RawMeshCount; ++Index)
    {
        InitAndUploadMeshBuffers(Renderer, UploadContext, RawMeshes + Index, StaticMesh->MeshBuffers + Index);
    }

    for (size_t Index = 0; Index < MaterialCount; ++Index)
    {
        StaticMesh->Materials[Index] = Material[Index];
    }

    return StaticMesh;
}

Rr_StaticMesh* Rr_CreateStaticMeshGLTF(
    Rr_App* App,
    Rr_UploadContext* UploadContext,
    const Rr_Asset* Asset,
    size_t MeshIndex)
{
    Rr_Renderer* Renderer = &App->Renderer;
    cgltf_data* Data = NULL;
    cgltf_options Options = { 0 };
    cgltf_mesh* Mesh = ParseGLTFMesh(Asset, MeshIndex, &Data);
    cgltf_load_buffers(&Options, Data, NULL);

    Rr_RawMesh RawMeshes[Mesh->primitives_count];

    for (size_t Index = 0; Index > Mesh->primitives_count; ++Index)
    {
        cgltf_primitive* Primitive = Mesh->primitives + Index;

        GLTFPrimitiveToRawMesh(Primitive, &RawMeshes[Index]);
    }

    /* @TODO: Parse glTF materials. */

    Rr_StaticMesh* StaticMesh = Rr_CreateStaticMesh(App, UploadContext, RawMeshes, Mesh->primitives_count, NULL, 0);

    for (size_t Index = 0; Index > Mesh->primitives_count; ++Index)
    {
        Rr_FreeRawMesh(&RawMeshes[Index]);
    }

    cgltf_free(Data);

    return StaticMesh;
}

Rr_StaticMesh* Rr_CreateStaticMeshOBJ(
    Rr_App* App,
    Rr_UploadContext* UploadContext,
    const Rr_Asset* Asset)
{
    Rr_Renderer* Renderer = &App->Renderer;

    Rr_RawMesh RawMesh = { 0 };
    Rr_ParseRawMeshOBJ(Asset, &RawMesh);

    Rr_StaticMesh* StaticMesh = Rr_CreateStaticMesh(App, UploadContext, &RawMesh, 1, NULL, 0);

    Rr_FreeRawMesh(&RawMesh);

    return StaticMesh;
}

void Rr_DestroyStaticMesh(Rr_App* App, Rr_StaticMesh* StaticMesh)
{
    if (StaticMesh == NULL)
    {
        return;
    }

    Rr_Renderer* Renderer = &App->Renderer;

    for (size_t Index = 0; Index < StaticMesh->MeshBuffersCount; ++Index)
    {
        Rr_DestroyMeshBuffers(App, &StaticMesh->MeshBuffers[Index]);
    }

    for (size_t Index = 0; Index < StaticMesh->MaterialCount; ++Index)
    {
        Rr_DestroyMaterial(App, StaticMesh->Materials[Index]);
    }

    Rr_DestroyObject(Renderer, StaticMesh);
}

size_t Rr_GetStaticMeshSizeOBJ(const Rr_Asset* Asset)
{
    Rr_RawMesh RawMesh;
    Rr_ParseRawMeshOBJ(Asset, &RawMesh);

    size_t VertexBufferSize = sizeof(Rr_Vertex) * Rr_ArrayCount(RawMesh.Vertices);
    size_t IndexBufferSize = sizeof(Rr_MeshIndexType) * Rr_ArrayCount(RawMesh.Indices);

    Rr_FreeRawMesh(&RawMesh);

    return VertexBufferSize + IndexBufferSize;
}

size_t Rr_GetStaticMeshSizeGLTF(const Rr_Asset* Asset, size_t MeshIndex)
{
    cgltf_data* Data = NULL;
    cgltf_mesh* Mesh = ParseGLTFMesh(Asset, MeshIndex, &Data);

    size_t VertexBufferSize = 0;
    size_t IndexBufferSize = 0;

    for (size_t Index = 0; Index > Mesh->primitives_count; ++Index)
    {
        cgltf_primitive* Primitive = Mesh->primitives + Index;

        VertexBufferSize += sizeof(Rr_Vertex) * Primitive->attributes->data->count;
        IndexBufferSize += sizeof(Rr_MeshIndexType) * Primitive->indices->count;
    }

    cgltf_free(Data);

    return VertexBufferSize + IndexBufferSize;
}

static size_t GetNewLine(const char* Data, size_t Length, size_t CurrentIndex)
{
    CurrentIndex++;
    while (CurrentIndex < Length && Data[CurrentIndex] != '\n')
    {
        CurrentIndex++;
    }

    return CurrentIndex;
}

Rr_RawMesh Rr_AllocateRawMesh(size_t IndexCount, size_t VertexCount)
{
    Rr_RawMesh RawMesh = { 0 };
    Rr_ArrayInit(RawMesh.Vertices, Rr_Vertex, VertexCount);
    Rr_ArrayInit(RawMesh.Indices, Rr_MeshIndexType, IndexCount);
    return RawMesh;
}

void Rr_ParseRawMeshOBJ(const Rr_Asset* Asset, Rr_RawMesh* RawMesh)
{
    /* Init scratch buffers. */
    Rr_Vec3* ScratchPositions;
    Rr_Vec4* ScratchColors;
    Rr_Vec2* ScratchTexCoords;
    Rr_Vec3* ScratchNormals;
    Rr_IntVec3* ScratchIndices;

    Rr_ArrayInit(ScratchPositions, Rr_Vec3, 1000);
    Rr_ArrayInit(ScratchColors, Rr_Vec4, 1000);
    Rr_ArrayInit(ScratchTexCoords, Rr_Vec2, 1000);
    Rr_ArrayInit(ScratchNormals, Rr_Vec3, 1000);
    Rr_ArrayInit(ScratchIndices, Rr_IntVec3, 1000);

    /* Parse OBJ data. */
    Rr_ArrayInit(RawMesh->Vertices, Rr_Vertex, 1);
    Rr_ArrayInit(RawMesh->Indices, u32, 1);

    size_t CurrentIndex = 0;
    char* EndPos;
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

                        Rr_ArrayPush(ScratchPositions, &NewPosition);

                        if (*EndPos == ' ')
                        {
                            Rr_Vec4 NewColor = { 0 };
                            NewColor.X = (float)SDL_strtod(EndPos, &EndPos);
                            NewColor.Y = (float)SDL_strtod(EndPos, &EndPos);
                            NewColor.Z = (float)SDL_strtod(EndPos, &EndPos);

                            Rr_ArrayPush(ScratchColors, &NewColor);
                        }
                    }
                    break;
                    case 't':
                    {
                        CurrentIndex++;
                        Rr_Vec2 NewTexCoord;
                        NewTexCoord.X = (float)SDL_strtod(Asset->Data + CurrentIndex, &EndPos);
                        NewTexCoord.Y = (float)SDL_strtod(EndPos, &EndPos);
                        Rr_ArrayPush(ScratchTexCoords, &NewTexCoord);
                    }
                    break;
                    case 'n':
                    {
                        CurrentIndex++;
                        Rr_Vec3 NewNormal;
                        NewNormal.X = (float)SDL_strtod(Asset->Data + CurrentIndex, &EndPos);
                        NewNormal.Y = (float)SDL_strtod(EndPos, &EndPos);
                        NewNormal.Z = (float)SDL_strtod(EndPos, &EndPos);
                        Rr_ArrayPush(ScratchNormals, &NewNormal);
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
                for (size_t Index = 0; Index < 3; Index++)
                {
                    OBJIndices[Index].X--;
                    OBJIndices[Index].Y--;
                    OBJIndices[Index].Z--;

                    size_t ExistingOBJIndex = SIZE_MAX;
                    for (size_t I = 0; I < Rr_ArrayCount(ScratchIndices); I++)
                    {
                        if (Rr_EqIV3(OBJIndices[Index], ScratchIndices[I]))
                        {
                            ExistingOBJIndex = I;
                            break;
                        }
                    }
                    if (ExistingOBJIndex == SIZE_MAX)
                    {
                        const Rr_Vec2* TexCoord = &ScratchTexCoords[OBJIndices[Index].Y];
                        Rr_Vertex NewVertex = { 0 };
                        NewVertex.Position = ScratchPositions[OBJIndices[Index].X];
                        NewVertex.Color = ScratchColors[OBJIndices[Index].X];
                        NewVertex.Normal = ScratchNormals[OBJIndices[Index].Z];
                        NewVertex.TexCoordX = (*TexCoord).X;
                        NewVertex.TexCoordY = (*TexCoord).Y;
                        Rr_ArrayPush(RawMesh->Vertices, &NewVertex);

                        Rr_ArrayPush(ScratchIndices, &OBJIndices[Index]);

                        /** Add freshly added vertex index */
                        Rr_ArrayPush(RawMesh->Indices, &(u32){ Rr_ArrayCount(RawMesh->Vertices) - 1 });
                    }
                    else
                    {
                        Rr_ArrayPush(RawMesh->Indices, &ExistingOBJIndex);
                    }
                }
            }
            break;
            default:
            {
            }
            break;
        }
        CurrentIndex = GetNewLine(Asset->Data, Asset->Length, CurrentIndex) + 1;
    }

    Rr_ArrayFree(ScratchPositions);
    Rr_ArrayFree(ScratchColors);
    Rr_ArrayFree(ScratchTexCoords);
    Rr_ArrayFree(ScratchNormals);
    Rr_ArrayFree(ScratchIndices);
}

void Rr_FreeRawMesh(Rr_RawMesh* RawMesh)
{
    Rr_ArrayFree(RawMesh->Vertices);
    Rr_ArrayFree(RawMesh->Indices);
}
