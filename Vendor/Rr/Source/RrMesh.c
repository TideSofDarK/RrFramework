#include "RrMesh.h"

#include "RrRenderer.h"
#include "RrVulkan.h"
#include "RrTypes.h"
#include "RrBuffer.h"

void Rr_UploadMesh(
    SRr* const Rr,
    SRrMeshBuffers* const MeshBuffers,
    MeshIndexType const* const Indices,
    size_t IndexCount,
    SRrVertex const* const Vertices,
    size_t VertexCount)
{
    size_t VertexBufferSize = sizeof(SRrVertex) * VertexCount;
    size_t IndexBufferSize = sizeof(MeshIndexType) * IndexCount;

    AllocatedBuffer_Init(
        &MeshBuffers->VertexBuffer,
        Rr->Allocator,
        VertexBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_AUTO,
        false);

    VkBufferDeviceAddressInfo DeviceAddressInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = MeshBuffers->VertexBuffer.Handle
    };
    MeshBuffers->VertexBufferAddress = vkGetBufferDeviceAddress(Rr->Device, &DeviceAddressInfo);

    AllocatedBuffer_Init(
        &MeshBuffers->IndexBuffer,
        Rr->Allocator,
        IndexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO, false);

    SAllocatedBuffer StagingBuffer = { 0 };
    AllocatedBuffer_Init(
        &StagingBuffer,
        Rr->Allocator,
        VertexBufferSize + IndexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO, true);

    void* StagingData = StagingBuffer.AllocationInfo.pMappedData;

    memcpy((char*)StagingData, (void*)Vertices, VertexBufferSize);
    memcpy((char*)StagingData + VertexBufferSize, (void*)Indices, IndexBufferSize);

    Rr_BeginImmediate(Rr);

    VkBufferCopy VertexCopy = {
        .dstOffset = 0,
        .srcOffset = 0,
        .size = VertexBufferSize
    };

    vkCmdCopyBuffer(Rr->ImmediateMode.CommandBuffer, StagingBuffer.Handle, MeshBuffers->VertexBuffer.Handle, 1, &VertexCopy);

    VkBufferCopy IndexCopy = {
        .dstOffset = 0,
        .srcOffset = VertexBufferSize,
        .size = IndexBufferSize
    };

    vkCmdCopyBuffer(Rr->ImmediateMode.CommandBuffer, StagingBuffer.Handle, MeshBuffers->IndexBuffer.Handle, 1, &IndexCopy);

    Rr_EndImmediate(Rr);

    AllocatedBuffer_Cleanup(&StagingBuffer, Rr->Allocator);
}

void Rr_CleanupMesh(SRr* const Rr, SRrMeshBuffers* const Mesh)
{
    AllocatedBuffer_Cleanup(&Mesh->IndexBuffer, Rr->Allocator);
    AllocatedBuffer_Cleanup(&Mesh->VertexBuffer, Rr->Allocator);
}
