#include "Rr_Mesh.h"

#include "Rr_Array.h"
#include "Rr_Vulkan.h"
#include "Rr_Buffer.h"
#include "Rr_Memory.h"
#include "Rr_Types.h"

#include <SDL_stdinc.h>

#include <cgltf/cgltf.h>

Rr_StaticMesh* Rr_CreateStaticMeshFromOBJ(
    Rr_App* App,
    Rr_UploadContext* UploadContext,
    Rr_Asset* Asset)
{
    Rr_Renderer* Renderer = &App->Renderer;
    Rr_StaticMesh* StaticMesh = Rr_CreateObject(Renderer);
    Rr_RawMesh* RawMesh = Rr_CreateRawMeshOBJ(Asset);
    StaticMesh->IndexCount = Rr_ArrayCount(RawMesh->Indices);

    const size_t VertexBufferSize = sizeof(Rr_Vertex) * Rr_ArrayCount(RawMesh->Vertices);
    const size_t IndexBufferSize = sizeof(Rr_MeshIndexType) * StaticMesh->IndexCount;

    StaticMesh->VertexBuffer = Rr_CreateBuffer(
        Renderer,
        VertexBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO,
        false);

    StaticMesh->IndexBuffer = Rr_CreateBuffer(
        Renderer,
        IndexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO,
        false);

    Rr_UploadBuffer(
        Renderer,
        UploadContext,
        StaticMesh->VertexBuffer->Handle,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        0,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
        RawMesh->Vertices,
        VertexBufferSize);

    Rr_UploadBuffer(
        Renderer,
        UploadContext,
        StaticMesh->IndexBuffer->Handle,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        0,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        VK_ACCESS_INDEX_READ_BIT,
        RawMesh->Indices,
        IndexBufferSize);

    Rr_DestroyRawMesh(RawMesh);

    return StaticMesh;
}

void Rr_DestroyStaticMesh(Rr_App* App, Rr_StaticMesh* Mesh)
{
    if (Mesh == NULL)
    {
        return;
    }

    Rr_Renderer* Renderer = &App->Renderer;

    Rr_DestroyBuffer(Renderer, Mesh->IndexBuffer);
    Rr_DestroyBuffer(Renderer, Mesh->VertexBuffer);

    Rr_DestroyObject(Renderer, Mesh);
}

size_t Rr_GetStaticMeshSizeOBJ(Rr_Asset* Asset)
{
    Rr_RawMesh* RawMesh = Rr_CreateRawMeshOBJ(Asset);

    size_t VertexBufferSize = sizeof(Rr_Vertex) * Rr_ArrayCount(RawMesh->Vertices);
    size_t IndexBufferSize = sizeof(Rr_MeshIndexType) * Rr_ArrayCount(RawMesh->Indices);
    size_t TotalSize = VertexBufferSize + IndexBufferSize;

    Rr_DestroyRawMesh(RawMesh);

    return TotalSize;
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

Rr_RawMesh* Rr_CreateRawMeshOBJ(Rr_Asset* Asset)
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
    Rr_RawMesh* RawMesh = Rr_Calloc(1, sizeof(Rr_RawMesh));
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

    return RawMesh;
}

Rr_RawMesh* Rr_CreateRawMeshGLTF(const Rr_Asset* Asset)
{
    Rr_RawMesh* RawMesh = Rr_Calloc(1, sizeof(Rr_RawMesh));

    cgltf_options Options = { 0 };
    cgltf_data* Data = NULL;
    cgltf_result Result = cgltf_parse(&Options, Asset->Data, Asset->Length, &Data);
    if (Result == cgltf_result_success)
    {
        cgltf_free(Data);
    }

    return RawMesh;
}

void Rr_DestroyRawMesh(Rr_RawMesh* RawMesh)
{
    Rr_ArrayFree(RawMesh->Vertices);
    Rr_ArrayFree(RawMesh->Indices);

    Rr_Free(RawMesh);
}
