#include "Rr_TransferNode.h"

#include "Rr_App.h"
#include "Rr_Graph.h"
#include "Rr_Log.h"

#include <assert.h>
#include <string.h>

Rr_GraphNode *Rr_AddTransferNode(Rr_App *App, const char *Name, Rr_Buffer *DstBuffer, Rr_Data Data, size_t DstOffset)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_GraphNode *GraphNode = Rr_AddGraphNode(Frame, RR_GRAPH_NODE_TYPE_TRANSFER, Name);

    Rr_AllocatedBuffer *StagingBuffer = Rr_GetCurrentAllocatedBuffer(App, Frame->StagingBuffer.Buffer);

    size_t SrcOffset = Frame->StagingBuffer.Offset;

    if(SrcOffset + Data.Size > StagingBuffer->AllocationInfo.size)
    {
        Rr_LogAbort("Transfer: source buffer overflow!");
    }

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

    return GraphNode;
}

bool Rr_BatchTransferNode(Rr_App *App, Rr_GraphBatch *Batch, Rr_TransferNode *Node)
{
    return Rr_BatchBuffer(
               App,
               Batch,
               Node->SrcBuffer->Handle,
               Node->Size,
               Node->SrcOffset,
               VK_PIPELINE_STAGE_TRANSFER_BIT,
               VK_ACCESS_TRANSFER_READ_BIT) &&
           Rr_BatchBuffer(
               App,
               Batch,
               Node->DstBuffer->Handle,
               Node->Size,
               Node->DstOffset,
               VK_PIPELINE_STAGE_TRANSFER_BIT,
               VK_ACCESS_TRANSFER_WRITE_BIT);
}

void Rr_ExecuteTransferNode(Rr_App *App, Rr_TransferNode *Node, Rr_Arena *Arena)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    VkCommandBuffer CommandBuffer = Frame->CommandBuffer.Handle;

    VkBufferCopy Copy = { .size = Node->Size, .srcOffset = Node->SrcOffset, .dstOffset = Node->DstOffset };
    vkCmdCopyBuffer(CommandBuffer, Node->SrcBuffer->Handle, Node->DstBuffer->Handle, 1, &Copy);
}
