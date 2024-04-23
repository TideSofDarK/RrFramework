#include "Rr_Mesh.h"

#include <cglm/vec3.h>
#include <cglm/vec4.h>
#include <cglm/ivec3.h>

#include <SDL_stdinc.h>

#include "Rr_Array.h"
#include "Rr_Renderer.h"
#include "Rr_Vulkan.h"
#include "Rr_Buffer.h"

void Rr_UploadMesh(
    Rr_Renderer* const Renderer,
    Rr_MeshBuffers* const MeshBuffers,
    Rr_MeshIndexType const* const Indices,
    size_t IndexCount,
    Rr_Vertex const* const Vertices,
    size_t VertexCount)
{
    size_t VertexBufferSize = sizeof(Rr_Vertex) * VertexCount;
    size_t IndexBufferSize = sizeof(Rr_MeshIndexType) * IndexCount;

    MeshBuffers->VertexBuffer = Rr_CreateBuffer(
        Renderer->Allocator,
        VertexBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_AUTO,
        false);

    VkBufferDeviceAddressInfo DeviceAddressInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = MeshBuffers->VertexBuffer.Handle
    };
    MeshBuffers->VertexBufferAddress = vkGetBufferDeviceAddress(Renderer->Device, &DeviceAddressInfo);

    MeshBuffers->IndexBuffer = Rr_CreateBuffer(
        Renderer->Allocator,
        IndexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO, false);

    Rr_Buffer StagingBuffer = Rr_CreateBuffer(
        Renderer->Allocator,
        VertexBufferSize + IndexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO, true);

    void* StagingData = StagingBuffer.AllocationInfo.pMappedData;

    SDL_memcpy((char*)StagingData, (void*)Vertices, VertexBufferSize);
    SDL_memcpy((char*)StagingData + VertexBufferSize, (void*)Indices, IndexBufferSize);

    Rr_BeginImmediate(Renderer);

    VkBufferCopy VertexCopy = {
        .dstOffset = 0,
        .srcOffset = 0,
        .size = VertexBufferSize
    };

    vkCmdCopyBuffer(Renderer->ImmediateMode.CommandBuffer, StagingBuffer.Handle, MeshBuffers->VertexBuffer.Handle, 1, &VertexCopy);

    VkBufferCopy IndexCopy = {
        .dstOffset = 0,
        .srcOffset = VertexBufferSize,
        .size = IndexBufferSize
    };

    vkCmdCopyBuffer(Renderer->ImmediateMode.CommandBuffer, StagingBuffer.Handle, MeshBuffers->IndexBuffer.Handle, 1, &IndexCopy);

    Rr_EndImmediate(Renderer);

    Rr_DestroyBuffer(&StagingBuffer, Renderer->Allocator);
}

Rr_MeshBuffers Rr_CreateMesh_FromOBJ(Rr_Renderer* const Renderer, Rr_Asset* Asset)
{
    Rr_MeshBuffers MeshBuffers;
    Rr_RawMesh RawMesh;
    Rr_ParseOBJ(&RawMesh, Asset);
    MeshBuffers.IndexCount = Rr_ArrayCount(RawMesh.Indices);
    Rr_UploadMesh(Renderer, &MeshBuffers, RawMesh.Indices, MeshBuffers.IndexCount, RawMesh.Vertices, Rr_ArrayCount(RawMesh.Vertices));
    Rr_DestroyRawMesh(&RawMesh);
    return MeshBuffers;
}

void Rr_DestroyMesh(Rr_Renderer* const Renderer, Rr_MeshBuffers* const Mesh)
{
    Rr_DestroyBuffer(&Mesh->IndexBuffer, Renderer->Allocator);
    Rr_DestroyBuffer(&Mesh->VertexBuffer, Renderer->Allocator);
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

void Rr_ParseOBJ(Rr_RawMesh* RawMesh, Rr_Asset* Asset)
{
    /* Init scratch buffers. */
    vec3* ScratchPositions;
    vec4* ScratchColors;
    vec2* ScratchTexCoords;
    vec3* ScratchNormals;
    ivec3* ScratchIndices;

    Rr_ArrayInit(ScratchPositions, vec3, 1000);
    Rr_ArrayInit(ScratchColors, vec4, 1000);
    Rr_ArrayInit(ScratchTexCoords, vec2, 1000);
    Rr_ArrayInit(ScratchNormals, vec3, 1000);
    Rr_ArrayInit(ScratchIndices, ivec3, 1000);

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
                        vec3 NewPosition;
                        NewPosition[0] = (float)SDL_strtod(Asset->Data + CurrentIndex, &EndPos);
                        NewPosition[1] = (float)SDL_strtod(EndPos, &EndPos);
                        NewPosition[2] = (float)SDL_strtod(EndPos, &EndPos);

                        Rr_ArrayPush(ScratchPositions, &NewPosition);

                        if (*EndPos == ' ')
                        {
                            vec4 NewColor = { 0 };
                            NewColor[0] = (float)SDL_strtod(EndPos, &EndPos);
                            NewColor[1] = (float)SDL_strtod(EndPos, &EndPos);
                            NewColor[2] = (float)SDL_strtod(EndPos, &EndPos);

                            Rr_ArrayPush(ScratchColors, &NewColor);
                        }
                    }
                    break;
                    case 't':
                    {
                        CurrentIndex++;
                        vec2 NewTexCoord;
                        NewTexCoord[0] = (float)SDL_strtod(Asset->Data + CurrentIndex, &EndPos);
                        NewTexCoord[1] = (float)SDL_strtod(EndPos, &EndPos);
                        Rr_ArrayPush(ScratchTexCoords, &NewTexCoord);
                    }
                    break;
                    case 'n':
                    {
                        CurrentIndex++;
                        vec3 NewNormal;
                        NewNormal[0] = (float)SDL_strtod(Asset->Data + CurrentIndex, &EndPos);
                        NewNormal[1] = (float)SDL_strtod(EndPos, &EndPos);
                        NewNormal[2] = (float)SDL_strtod(EndPos, &EndPos);
                        Rr_ArrayPush(ScratchNormals, &NewNormal);
                    }
                    break;
                }
            }
            break;
            case 'f':
            {
                CurrentIndex++;
                ivec3 OBJIndices[3] = { 0 };
                OBJIndices[0][0] = (i32)SDL_strtoul(Asset->Data + CurrentIndex, &EndPos, 10);
                OBJIndices[0][1] = (i32)SDL_strtoul(EndPos + 1, &EndPos, 0);
                OBJIndices[0][2] = (i32)SDL_strtoul(EndPos + 1, &EndPos, 0);
                OBJIndices[1][0] = (i32)SDL_strtoul(EndPos + 1, &EndPos, 0);
                OBJIndices[1][1] = (i32)SDL_strtoul(EndPos + 1, &EndPos, 0);
                OBJIndices[1][2] = (i32)SDL_strtoul(EndPos + 1, &EndPos, 0);
                OBJIndices[2][0] = (i32)SDL_strtoul(EndPos + 1, &EndPos, 0);
                OBJIndices[2][1] = (i32)SDL_strtoul(EndPos + 1, &EndPos, 0);
                OBJIndices[2][2] = (i32)SDL_strtoul(EndPos + 1, &EndPos, 0);
                for (size_t Index = 0; Index < 3; Index++)
                {
                    glm_ivec3_subs(OBJIndices[Index], 1, OBJIndices[Index]);

                    size_t ExistingOBJIndex = SIZE_MAX;
                    for (size_t I = 0; I < Rr_ArrayCount(ScratchIndices); I++)
                    {
                        if (glm_ivec3_eqv(OBJIndices[Index], ((ivec3*)ScratchIndices)[I]))
                        {
                            ExistingOBJIndex = I;
                            break;
                        }
                    }
                    if (ExistingOBJIndex == SIZE_MAX)
                    {
                        vec3* Position = &ScratchPositions[OBJIndices[Index][0]];
                        vec4* Color = &ScratchColors[OBJIndices[Index][0]];
                        vec2* TexCoord = &ScratchTexCoords[OBJIndices[Index][1]];
                        vec3* Normal = &ScratchNormals[OBJIndices[Index][2]];
                        Rr_Vertex NewVertex = { 0 };
                        glm_vec3_copy(*Position, NewVertex.Position);
                        glm_vec4_copy(*Color, NewVertex.Color);
                        glm_vec3_copy(*Normal, NewVertex.Normal);
                        NewVertex.TexCoordX = (*TexCoord)[0];
                        NewVertex.TexCoordY = (*TexCoord)[1];
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

void Rr_DestroyRawMesh(Rr_RawMesh* RawMesh)
{
    Rr_ArrayFree(RawMesh->Vertices);
    Rr_ArrayFree(RawMesh->Indices);
}
