#include "Rr_TransferNode.h"

#include "Rr_App.h"
#include "Rr_Graph.h"
#include "Rr_Log.h"

#include <assert.h>
#include <string.h>

Rr_GraphNode *Rr_AddTransferNode(
    Rr_App *App,
    const char *Name,
    Rr_Buffer *DstBuffer,
    Rr_Data Data,
    size_t *InOutOffset,
    Rr_GraphNode **Dependencies,
    size_t DependencyCount)
{
    assert(DstBuffer != NULL);
    assert(Data.Pointer != NULL);
    assert(Data.Size != 0);
    assert(InOutOffset != NULL);

    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_GraphNode *GraphNode = Rr_AddGraphNode(Frame, RR_GRAPH_NODE_TYPE_TRANSFER, Name, Dependencies, DependencyCount);

    Rr_AllocatedBuffer *StagingBuffer = Rr_GetCurrentAllocatedBuffer(App, Frame->StagingBuffer.Buffer);
    size_t Alignment = 0;
    if((DstBuffer->Usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0)
    {
        Alignment = RR_MAX(Rr_GetUniformAlignment(&App->Renderer), Alignment);
    }
    if((DstBuffer->Usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) != 0)
    {
        Alignment = RR_MAX(Rr_GetStorageAlignment(&App->Renderer), Alignment);
    }

    size_t SrcOffset = Frame->StagingBuffer.Offset;

    if(SrcOffset + Data.Size > StagingBuffer->AllocationInfo.size)
    {
        Rr_LogAbort("Transfer: source buffer overflow!");
    }

    size_t DstOffset = RR_ALIGN_POW2(*InOutOffset, Alignment);

    if(DstOffset + Data.Size > DstBuffer->AllocatedBuffers[0].AllocationInfo.size)
    {
        Rr_LogAbort("Transfer: destination buffer overflow!");
    }

    memcpy((char *)StagingBuffer->AllocationInfo.pMappedData + Frame->StagingBuffer.Offset, Data.Pointer, Data.Size);

    Frame->StagingBuffer.Offset += Data.Size;

    Rr_TransferNode *TransferNode = &GraphNode->Union.TransferNode;
    TransferNode->SrcBuffer = StagingBuffer;
    TransferNode->SrcOffset = SrcOffset;
    TransferNode->DstBuffer = Rr_GetCurrentAllocatedBuffer(App, DstBuffer);
    TransferNode->DstOffset = DstOffset;
    TransferNode->Size = Data.Size;

    *InOutOffset = DstOffset;

    return GraphNode;
}

bool Rr_BatchTransferNode(Rr_App *App, Rr_GraphBatch *Batch, Rr_TransferNode *Node)
{
    if(Rr_BatchBufferPossible(&Batch->LocalSync, Node->SrcBuffer->Handle) != true ||
       Rr_BatchBufferPossible(&Batch->LocalSync, Node->DstBuffer->Handle) != true)
    {
        return false;
    }

    Rr_BatchBuffer(App, Batch, Node->SrcBuffer->Handle, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    Rr_BatchBuffer(App, Batch, Node->DstBuffer->Handle, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);

    return true;
}

void Rr_ExecuteTransferNode(Rr_App *App, Rr_TransferNode *Node, Rr_Arena *Arena)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    VkCommandBuffer CommandBuffer = Frame->CommandBuffer.Handle;

    VkBufferCopy Copy = { .size = Node->Size, .srcOffset = Node->SrcOffset, .dstOffset = Node->DstOffset };
    vkCmdCopyBuffer(CommandBuffer, Node->SrcBuffer->Handle, Node->DstBuffer->Handle, 1, &Copy);
}
