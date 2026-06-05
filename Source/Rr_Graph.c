/*
 * Copyright (C) 2024-2026 Alexandr Semenov <tidesmain@gmail.com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include "Rr_Graph.h"

#define RR_LOG_MACRO_CATEGORY RR_LOG_CATEGORY_GRAPH
#include "Rr_App.h"
#include "Rr_Buffer.h"
#include "Rr_Descriptor.h"
#include "Rr_Image.h"
#include "Rr_LogMacro.h"
#include "Rr_Renderer.h"

#include <assert.h>

Rr_Graph *Rr_GetGraph(void)
{
    return Rr_GetCurrentFrame()->Graph;
}

Rr_Graph *Rr_BeginGraph(Rr_QueueType QueueType)
{
    assert(Rr_HasQueue(QueueType));

    Rr_ThreadContext *ThreadContext = Rr_GetThreadContext();
    assert(ThreadContext && "Call Rr_InitThreadContext first!");
    assert(
        !ThreadContext->Graph &&
        "Graph recording is already on; did you forget to call Rr_EndGraph()?");

    size_t ArenaPosition = ThreadContext->Arena->Position;

    ThreadContext->Graph = RR_ALLOC_TYPE(Rr_Graph, ThreadContext->Arena);
    ThreadContext->Graph->QueueType = QueueType;
    ThreadContext->Graph->DescriptorPoolList = Rr_AcquireDescriptorPoolList();
    ThreadContext->Graph->ArenaPosition = ArenaPosition;
    ThreadContext->Graph->Arena = ThreadContext->Arena;

    return ThreadContext->Graph;
}

void Rr_EndGraph(Rr_Graph *Graph)
{
    assert(Graph);
    assert(
        !Graph->Primary &&
        "Rr_EndGraph() can only be called on non-primary graphs!");

    if (Graph->Nodes.Count == 0)
    {
        goto Cleanup;
    }

    Rr_CommandPools *CommandPools = Rr_AcquireCommandPools();

    Rr_Device *Device = &gRenderer->Device;

    Rr_Queue *Queue = Rr_GetQueue(Graph->QueueType);
    uint32_t QueueFamilyIndex = Queue->FamilyIndex;

    VkCommandPool CommandPool;
    switch (Graph->QueueType)
    {
        case RR_QUEUE_TYPE_MAIN:
        {
            CommandPool = CommandPools->Graphics;
        }
        break;
        case RR_QUEUE_TYPE_DEDICATED_TRANSFER:
        {
            CommandPool = CommandPools->Transfer;
        }
        break;
        case RR_QUEUE_TYPE_ASYNC_COMPUTE:
        {
            CommandPool = CommandPools->Compute;
        }
        break;
        default:
            RR_LOG_ABORT("Invalid queue type!");
    }

    VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
    Device->AllocateCommandBuffers(
        Device->Handle,
        &(VkCommandBufferAllocateInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = CommandPool,
            .commandBufferCount = 1,
        },
        &CommandBuffer);
    VkCommandBufferBeginInfo CommandBufferBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    Device->BeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo);

    Rr_ExecuteGraph(Graph, QueueFamilyIndex, CommandBuffer, VK_NULL_HANDLE);

    Device->EndCommandBuffer(CommandBuffer);

    VkFence Fence = Rr_AcquireVulkanFence();

    Rr_LockSpinlock(&Queue->Lock);

    Device->QueueSubmit(
        Queue->Handle,
        1,
        &(VkSubmitInfo){
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &CommandBuffer,
        },
        Fence);

    Rr_UnlockSpinlock(&Queue->Lock);

    Device->WaitForFences(Device->Handle, 1, &Fence, VK_TRUE, UINT64_MAX);

    Rr_ReleaseVulkanFence(Fence);

    Device->ResetCommandPool(
        Device->Handle,
        CommandPool,
        VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);

Cleanup:

    Rr_ReleaseGraphResources(Graph);

    /* TODO: Check whether semaphores are needed here considering that we always
     * wait on a fence. */

    Graph->Arena->Position = Graph->ArenaPosition;
    Rr_GetThreadContext()->Graph = NULL;
}

static Rr_AllocatedBuffer *Rr_GetGraphBuffer(
    Rr_Graph *Graph,
    Rr_GraphBuffer Handle)
{
    return Graph->BufferResources.Data[Handle.Values.Index].Allocated;
}

static Rr_GraphResource *Rr_GetGraphBufferResource(
    Rr_Graph *Graph,
    Rr_GraphBuffer Handle)
{
    return &Graph->BufferResources.Data[Handle.Values.Index];
}

static Rr_AllocatedImage *Rr_GetGraphImage(
    Rr_Graph *Graph,
    Rr_GraphImage Handle)
{
    return Graph->ImageResources.Data[Handle.Values.Index].Allocated;
}

static Rr_GraphResource *Rr_GetGraphImageResource(
    Rr_Graph *Graph,
    Rr_GraphImage Handle)
{
    return &Graph->ImageResources.Data[Handle.Values.Index];
}

static inline Rr_DescriptorsState Rr_MakeDescriptorsState(
    Rr_Graph *Graph,
    VkCommandBuffer CommandBuffer)
{
    Rr_DescriptorsState DescriptorsState = { 0 };
    DescriptorsState.Device = &gRenderer->Device;
    DescriptorsState.CommandBuffer = CommandBuffer;
    DescriptorsState.EmptyDescriptorSet = gRenderer->EmptyDescriptorSet;
    DescriptorsState.DescriptorPoolList = Graph->DescriptorPoolList;
    return DescriptorsState;
}

static inline Rr_GraphNode *Rr_AddGraphNode(
    Rr_Graph *Graph,
    Rr_GraphNodeType Type)
{
    Rr_GraphNode *GraphNode = RR_ALLOC(sizeof(Rr_GraphNode), Graph->Arena);
    GraphNode->Type = Type;
    if (Graph->NextNodeName)
    {
        GraphNode->Name = Graph->NextNodeName;
        Graph->NextNodeName = NULL;
    }
    GraphNode->OriginalIndex = (uint32_t)Graph->Nodes.Count;
    GraphNode->Graph = Graph;

    RR_RESERVE_ARRAY(&GraphNode->BufferDeps, 2, Graph->Arena);
    RR_RESERVE_ARRAY(&GraphNode->ImageDeps, 2, Graph->Arena);

    *RR_PUSH_INTO_ARRAY(&Graph->Nodes, Graph->Arena) = GraphNode;

#ifdef RR_USE_GPU_DEBUG_UTILS
    GraphNode->DebugLabelCount = Graph->DebugLabelNames.Count;
    if (GraphNode->DebugLabelCount)
    {
        GraphNode->DebugLabelStates = RR_ALLOC_COPY(
            Graph->DebugLabelStates.Data,
            sizeof(bool) * Graph->DebugLabelNames.Count,
            Graph->Arena);
    }
#endif

    return GraphNode;
}

static inline bool Rr_AddNodeDependency(
    Rr_GraphNode *Node,
    Rr_NodeDependencyArray *Deps,
    Rr_HashTrie **WriteToNode,
    Rr_GraphHandle *Handle,
    Rr_SyncState *State,
    bool AllowMultipleWrites,
    bool AllowReadWrite)
{
    for (size_t Index = 0; Index < Deps->Count; ++Index)
    {
        Rr_NodeDependency *Dependency = Deps->Data + Index;

        if (Dependency->Handle.Values.Index == Handle->Values.Index)
        {
            bool AlreadyWriting =
                Dependency->State.AccessMask & RR_VULKAN_WRITES;
            bool WantToWrite = State->AccessMask & RR_VULKAN_WRITES;
            if (AlreadyWriting && WantToWrite && !AllowMultipleWrites)
            {
                RR_LOG_ERROR(
                    "Node \"%s\": already writing to the versioned "
                    "resource!",
                    Node->Name);

                return false;
            }
            if (!AlreadyWriting && WantToWrite && !AllowReadWrite)
            {
                RR_LOG_ERROR(
                    "Node \"%s\": trying to read and write a versioned "
                    "resource at the "
                    "same time!",
                    Node->Name);

                return false;
            }

            /* Multiple reads might be from different stages. */

            Dependency->State.StageMask |= State->StageMask;
            Dependency->State.AccessMask |= State->AccessMask;

            return true;
        }
    }

    Rr_Arena *Arena = Node->Graph->Arena;

    Rr_GraphHandle CurrentHandle = *Handle;

    Rr_GraphNode **NodeInMap =
        RR_FIND_IN_HASH_TRIE(WriteToNode, Handle->Hash, Arena);

    /* Treat any image read as a write for now due to layout transitions. */

    if (State->Layout != VK_IMAGE_LAYOUT_UNDEFINED ||
        (State->AccessMask & RR_VULKAN_WRITES))
    {
        if (*NodeInMap == NULL)
        {
            Handle->Values.Generation++;

            *NodeInMap = Node;
        }
        else
        {
            RR_LOG_ERROR(
                "Node \"%s\": another node already writes to the versioned "
                "resource!",
                Node->Name);

            return false;
        }
    }

    *RR_PUSH_INTO_ARRAY(Deps, Arena) = (Rr_NodeDependency){
        .State = *State,
        .Handle = CurrentHandle,
    };

    return true;
}

static inline bool Rr_AddBufferDependency(
    Rr_GraphNode *Node,
    Rr_GraphHandle *Handle,
    Rr_SyncState *State)
{
    return Rr_AddNodeDependency(
        Node,
        &Node->BufferDeps,
        &Node->Graph->BufferWriteToNode,
        Handle,
        State,
        false,
        false);
}

static inline bool Rr_AddStorageBufferDependency(
    Rr_GraphNode *Node,
    Rr_GraphHandle *Handle,
    Rr_SyncState *State)
{
    return Rr_AddNodeDependency(
        Node,
        &Node->BufferDeps,
        &Node->Graph->BufferWriteToNode,
        Handle,
        State,
        true,
        true);
}

static inline bool Rr_AddImageDependency(
    Rr_GraphNode *Node,
    Rr_GraphHandle *Handle,
    Rr_SyncState *State)
{
    if (Handle == Node->Graph->SwapchainImageHandle)
    {
        Node->UsesLateCommandBuffer = true;
    }

    return Rr_AddNodeDependency(
        Node,
        &Node->ImageDeps,
        &Node->Graph->ImageWriteToNode,
        Handle,
        State,
        false,
        false);
}

static inline bool Rr_AddStorageImageDependency(
    Rr_GraphNode *Node,
    Rr_GraphHandle *Handle,
    Rr_SyncState *State)
{
    if (Handle == Node->Graph->SwapchainImageHandle)
    {
        Node->UsesLateCommandBuffer = true;
    }

    return Rr_AddNodeDependency(
        Node,
        &Node->ImageDeps,
        &Node->Graph->ImageWriteToNode,
        Handle,
        State,
        true,
        true);
}

static inline void Rr_ProcessDependencies(
    Rr_Graph *Graph,
    Rr_IndexArray *AdjacencyList,
    uint32_t Index,
    Rr_GraphNode *Node,
    Rr_NodeDependencyArray *Deps,
    Rr_HashTrie **WriteToNode,
    Rr_Arena *Arena)
{
    for (size_t DepIndex = 0; DepIndex < Deps->Count; ++DepIndex)
    {
        Rr_NodeDependency *Dependency = &Deps->Data[DepIndex];

        /* Artifical "read-before-write" dependency. */

        Rr_GraphNode *Writer = RR_FIND_IN_HASH_TRIE_DEREF(
            WriteToNode,
            Dependency->Handle.Hash,
            Arena);
        if (Writer != NULL && Writer != Node)
        {
            *RR_PUSH_INTO_ARRAY(&AdjacencyList[Writer->OriginalIndex], Arena) =
                Node->OriginalIndex;
        }

        /* If generation is greater than zero
         * it means we should lookup another node that
         * produces that state of the resource and add
         * that node as a dependency. */

        if (Dependency->Handle.Values.Generation > 0)
        {
            Rr_GraphHandle Handle = Dependency->Handle;
            Handle.Values.Generation--;
            Rr_GraphNode *Producer =
                RR_FIND_IN_HASH_TRIE_DEREF(WriteToNode, Handle.Hash, Arena);
            if (Producer != NULL)
            {
                *RR_PUSH_INTO_ARRAY(&AdjacencyList[Index], Arena) =
                    Producer->OriginalIndex;
            }
            else
            {
                RR_LOG_ABORT("Failed to find resource producer!");
            }
        }
    }
}

static void Rr_CreateGraphAdjacencyList(
    Rr_Graph *Graph,
    Rr_IndexArray *AdjacencyList,
    Rr_Arena *Arena)
{
    for (uint32_t Index = 0; Index < Graph->Nodes.Count; ++Index)
    {
        Rr_GraphNode *Node = Graph->Nodes.Data[Index];

        Rr_ProcessDependencies(
            Graph,
            AdjacencyList,
            Index,
            Node,
            &Node->BufferDeps,
            &Graph->BufferWriteToNode,
            Arena);

        Rr_ProcessDependencies(
            Graph,
            AdjacencyList,
            Index,
            Node,
            &Node->ImageDeps,
            &Graph->ImageWriteToNode,
            Arena);
    }
}

static void Rr_SortGraph(
    size_t CurrentNodeIndex,
    Rr_IndexArray *AdjacencyList,
    int *State,
    Rr_GraphNode **Nodes,
    Rr_NodeArray *Out)
{
    static const int VisitedBit = 1;
    static const int OnStackBit = 2;

    if (State[CurrentNodeIndex] & VisitedBit)
    {
        if (State[CurrentNodeIndex] & OnStackBit)
        {
            RR_LOG_ABORT(
                "Cyclic graph detected on node \"%s\"!",
                Nodes[CurrentNodeIndex]->Name);
        }

        return;
    }

    State[CurrentNodeIndex] |= VisitedBit;
    State[CurrentNodeIndex] |= OnStackBit;

    Rr_IndexArray *Dependencies = &AdjacencyList[CurrentNodeIndex];
    for (size_t Index = 0; Index < Dependencies->Count; ++Index)
    {
        Rr_SortGraph(
            Dependencies->Data[Index],
            AdjacencyList,
            State,
            Nodes,
            Out);
    }

    *RR_PUSH_INTO_ARRAY(Out, NULL) = Nodes[CurrentNodeIndex];

    State[CurrentNodeIndex] &= ~OnStackBit;
}

static void Rr_PrintAdjacencyList(
    Rr_GraphNode **Nodes,
    Rr_IndexArray *AdjacencyList,
    size_t Count)
{
    for (size_t Index = 0; Index < Count; ++Index)
    {
        Rr_IndexArray *Deps = AdjacencyList + Index;

        RR_LOG_INFO(
            "%s Node \"%s\"",
            Nodes[Index]->UsesLateCommandBuffer ? "late" : "early",
            Nodes[Index]->Name);
        for (size_t DepIndex = 0; DepIndex < Deps->Count; ++DepIndex)
        {
            RR_LOG_INFO(
                " depends on node \"%s\"",
                Nodes[Deps->Data[DepIndex]]->Name);
        }
    }
}

static void Rr_ProcessGraphNodes(
    Rr_Graph *Graph,
    Rr_NodeArray *SortedNodes,
    Rr_Arena *Arena)
{
    Rr_Scratch Scratch = Rr_GetScratch(Arena);

    /* Adjacency list maps a node to a set of nodes that must
     * be executed before it. */

    Rr_IndexArray *AdjacencyList =
        RR_ALLOC_TYPE_COUNT(Rr_IndexArray, Graph->Nodes.Count, Scratch.Arena);
    Rr_CreateGraphAdjacencyList(Graph, AdjacencyList, Scratch.Arena);

    /* Rr_PrintAdjacencyList(Graph->Nodes.Data, AdjacencyList,
     * Graph->Nodes.Count); */

    /* Topological sort. */

    int *SortState = RR_ALLOC(sizeof(int) * Graph->Nodes.Count, Scratch.Arena);
    for (size_t Index = 0; Index < Graph->Nodes.Count; ++Index)
    {
        Rr_GraphNode *Node = Graph->Nodes.Data[Index];
        if (Node != NULL)
        {
            Rr_SortGraph(
                Node->OriginalIndex,
                AdjacencyList,
                SortState,
                Graph->Nodes.Data,
                SortedNodes);
        }
    }

    /* Split nodes between early and late command buffers. */
    /* TODO: Probably shouldn't require its own pass? */
    /* TODO: Some early nodes still get batched for late execution. */

    for (size_t Index = 0; Index < SortedNodes->Count; ++Index)
    {
        Rr_GraphNode *Node = SortedNodes->Data[Index];
        if (Node->UsesLateCommandBuffer)
        {
            continue;
        }
        Rr_IndexArray *Dependencies = &AdjacencyList[Node->OriginalIndex];
        for (size_t DependencyIndex = 0; DependencyIndex < Dependencies->Count;
             ++DependencyIndex)
        {
            Rr_GraphNode *Dependency =
                Graph->Nodes.Data[Dependencies->Data[DependencyIndex]];
            if (Dependency != NULL)
            {
                Node->UsesLateCommandBuffer |=
                    Dependency->UsesLateCommandBuffer;
            }
        }
    }

    /* Rr_PrintAdjacencyList(Graph->Nodes.Data, AdjacencyList,
     * Graph->Nodes.Count); */

    /* Longest path search will determine dependency level for each node.
     * Nodes within same dependency level are meant to be batched together. */

    Rr_GraphNode **Reversed =
        RR_ALLOC_TYPE_COUNT(Rr_GraphNode *, SortedNodes->Count, Scratch.Arena);
    for (size_t Index = 0; Index < SortedNodes->Count; ++Index)
    {
        Reversed[Index] = SortedNodes->Data[SortedNodes->Count - 1 - Index];
    }

    for (size_t Index = 0; Index < SortedNodes->Count; ++Index)
    {
        Rr_GraphNode *Node = Reversed[Index];
        size_t OriginalIndex = Node->OriginalIndex;
        for (size_t DepIndex = 0; DepIndex < AdjacencyList[OriginalIndex].Count;
             ++DepIndex)
        {
            size_t DepNodeIndex = AdjacencyList[OriginalIndex].Data[DepIndex];
            Graph->Nodes.Data[DepNodeIndex]->DependencyLevel = RR_MAX(
                Graph->Nodes.Data[DepNodeIndex]->DependencyLevel,
                Graph->Nodes.Data[OriginalIndex]->DependencyLevel + 1);
        }
    }

    Rr_DestroyScratch(Scratch);
}

static void Rr_ExecuteTransferNode(
    Rr_Graph *Graph,
    Rr_TransferNode *Node,
    VkCommandBuffer CommandBuffer)
{
    Rr_Device *Device = &gRenderer->Device;

    for (size_t Index = 0; Index < Node->Transfers.Count; ++Index)
    {
        Rr_Transfer *Transfer = Node->Transfers.Data + Index;

        VkBuffer SrcBuffer =
            Rr_GetGraphBuffer(Graph, Transfer->SrcBuffer)->Handle;
        VkBuffer DstBuffer =
            Rr_GetGraphBuffer(Graph, Transfer->DstBuffer)->Handle;

        VkBufferCopy Copy = { .size = Transfer->Size,
                              .srcOffset = Transfer->SrcOffset,
                              .dstOffset = Transfer->DstOffset };

        Device->CmdCopyBuffer(CommandBuffer, SrcBuffer, DstBuffer, 1, &Copy);
    }
}

static inline void Rr_ExecuteGenerateMipmaps(
    Rr_Graph *Graph,
    Rr_GraphImage ImageHandle,
    VkCommandBuffer CommandBuffer)
{
    Rr_Device *Device = &gRenderer->Device;

    Rr_GraphResource *ImageResource =
        Rr_GetGraphImageResource(Graph, ImageHandle);
    Rr_AllocatedImage *Image = Rr_GetGraphImage(Graph, ImageHandle);
    Rr_Image *Container = Image->Container;
    Rr_SyncState *State = &ImageResource->SyncState;
    VkImageAspectFlags Aspect = Container->AspectFlags;

    VkImageMemoryBarrier Level0ReadBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = State->AccessMask,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout = State->Layout,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = Image->Handle,
        .subresourceRange =
            (VkImageSubresourceRange){
                .aspectMask = Aspect,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
    };
    Device->CmdPipelineBarrier(
        CommandBuffer,
        State->StageMask,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0,
        NULL,
        0,
        NULL,
        1,
        &Level0ReadBarrier);

    int32_t SrcWidth = (int32_t)Container->Extent.width;
    int32_t SrcHeight = (int32_t)Container->Extent.height;
    int32_t SrcDepth = (int32_t)Container->Extent.depth;
    int32_t DstWidth = (int32_t)Container->Extent.width / 2;
    int32_t DstHeight = (int32_t)Container->Extent.height / 2;
    int32_t DstDepth = (int32_t)Container->Extent.depth / 2;
    for (uint32_t SrcLevel = 0; SrcLevel < Container->LevelCount - 1;
         ++SrcLevel)
    {
        DstWidth = RR_MAX(1, DstWidth);
        DstHeight = RR_MAX(1, DstHeight);
        DstDepth = RR_MAX(1, DstDepth);

        VkImageBlit Blit = {
            .srcSubresource =
                (VkImageSubresourceLayers){
                    .aspectMask = Aspect,
                    .mipLevel = SrcLevel,
                    .baseArrayLayer = 0,
                    .layerCount = Container->LayerCount,
                },
            .srcOffsets[1] = { SrcWidth, SrcHeight, SrcDepth },
            .dstSubresource =
                (VkImageSubresourceLayers){
                    .aspectMask = Aspect,
                    .mipLevel = SrcLevel + 1,
                    .baseArrayLayer = 0,
                    .layerCount = Container->LayerCount,
                },
            .dstOffsets[1] = { DstWidth, DstHeight, DstDepth },
        };
        Device->CmdBlitImage(
            CommandBuffer,
            Image->Handle,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            Image->Handle,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &Blit,
            VK_FILTER_LINEAR);
        VkImageMemoryBarrier ReadBarrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = Image->Handle,
            .subresourceRange =
                (VkImageSubresourceRange){
                    .aspectMask = Aspect,
                    .baseMipLevel = SrcLevel + 1,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = Container->LayerCount,
                },
        };
        Device->CmdPipelineBarrier(
            CommandBuffer,
            State->StageMask,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            NULL,
            0,
            NULL,
            1,
            &ReadBarrier);

        SrcWidth = DstWidth;
        SrcHeight = DstHeight;
        SrcDepth = DstDepth;
        DstWidth /= 2;
        DstHeight /= 2;
        DstDepth /= 2;
    }

    State->StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    State->AccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    State->Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
}

static inline bool Rr_ClampBlitRect(Rr_IntVec4 *Rect, VkExtent3D *Extent)
{
    Rect->X = RR_CLAMP(0, Rect->X, (int)Extent->width);
    Rect->Y = RR_CLAMP(0, Rect->Y, (int)Extent->height);
    Rect->Width = RR_CLAMP(0, Rect->Width, (int)Extent->width - Rect->X);
    Rect->Height = RR_CLAMP(0, Rect->Height, (int)Extent->height - Rect->Y);

    return Rect->Width > 0 && Rect->Height > 0;
}

static void Rr_ExecuteBlitNode(
    Rr_Graph *Graph,
    Rr_BlitNode *Node,
    VkCommandBuffer CommandBuffer)
{
    Rr_Device *Device = &gRenderer->Device;

    Rr_AllocatedImage *SrcImage = Rr_GetGraphImage(Graph, Node->SrcImageHandle);
    Rr_AllocatedImage *DstImage = Rr_GetGraphImage(Graph, Node->DstImageHandle);

    if (Rr_ClampBlitRect(&Node->SrcRect, &SrcImage->Container->Extent) &&
        Rr_ClampBlitRect(&Node->DstRect, &DstImage->Container->Extent))
    {
        VkImageBlit ImageBlit = {
            .srcSubresource = {
                .aspectMask = Node->AspectMask,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .srcOffsets = {
                { Node->SrcRect.X, Node->SrcRect.Y, 0, },
                { Node->SrcRect.X + Node->SrcRect.Width, Node->SrcRect.Y + Node->SrcRect.Height, 1, },
            },
            .dstSubresource = {
                .aspectMask = Node->AspectMask,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .dstOffsets = {
                { Node->DstRect.X, Node->DstRect.Y, 0, },
                { Node->DstRect.X + Node->DstRect.Width, Node->DstRect.Y + Node->DstRect.Height, 1, },
            },
        };

        Device->CmdBlitImage(
            CommandBuffer,
            SrcImage->Handle,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            DstImage->Handle,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &ImageBlit,
            VK_FILTER_LINEAR);
    }
}

static inline VkFormat Rr_GetImageFormatForBinding(
    Rr_DescriptorsState *State,
    uint32_t Set,
    uint32_t Binding)
{
    Rr_PipelineLayout *Layout = State->Layout;
    assert(Layout);
    Rr_DescriptorSetLayout *SetLayout = Layout->Key.DescriptorSetLayouts[Set];
    assert(SetLayout);
    return (VkFormat)SetLayout->Key.Bindings[Binding].ImageFormat;
}

static inline void Rr_ExecuteGenericEncodedCommands(
    Rr_Graph *Graph,
    Rr_NodeFunction *Function,
    Rr_DescriptorsState *DescriptorsState,
    VkCommandBuffer CommandBuffer)
{
    switch (Function->Type)
    {
        case RR_NODE_FUNCTION_TYPE_BIND_SAMPLER:
        {
            Rr_BindSamplerArgs *Args = Function->Args;
            Rr_WriteSamplerDescriptor(
                DescriptorsState,
                Args->Set,
                Args->Binding,
                Args->ArrayIndex,
                Args->Sampler->Handle);
        }
        break;
        case RR_NODE_FUNCTION_TYPE_BIND_SAMPLED_IMAGE:
        {
            Rr_BindSampledImageArgs *Args = Function->Args;
            Rr_AllocatedImage *AllocatedImage =
                Rr_GetGraphImage(Graph, Args->ImageHandle);
            VkImageView ImageView = Rr_GetVulkanImageView(
                AllocatedImage,
                &(Rr_ImageViewKey){
                    .SubresourceRange = Args->SubresourceRange,
                    .Type = Args->ViewType,
                    .Format = Rr_GetImageFormatForBinding(
                        DescriptorsState,
                        Args->Set,
                        Args->Binding),
                });
            Rr_WriteImageDescriptor(
                DescriptorsState,
                Args->Set,
                Args->Binding,
                Args->ArrayIndex,
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                ImageView,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                NULL);
        }
        break;
        case RR_NODE_FUNCTION_TYPE_BIND_COMBINED_IMAGE_SAMPLER:
        {
            Rr_BindCombinedImageSamplerArgs *Args = Function->Args;
            Rr_AllocatedImage *AllocatedImage =
                Rr_GetGraphImage(Graph, Args->ImageHandle);
            VkImageView ImageView = Rr_GetVulkanImageView(
                AllocatedImage,
                &(Rr_ImageViewKey){
                    .SubresourceRange = Args->SubresourceRange,
                    .Type = Args->ViewType,
                    .Format = Rr_GetImageFormatForBinding(
                        DescriptorsState,
                        Args->Set,
                        Args->Binding),
                });
            Rr_WriteImageDescriptor(
                DescriptorsState,
                Args->Set,
                Args->Binding,
                Args->ArrayIndex,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                ImageView,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                Args->Sampler->Handle);
        }
        break;
        case RR_NODE_FUNCTION_TYPE_BIND_UNIFORM_BUFFER:
        {
            Rr_BindUniformBufferArgs *Args = Function->Args;
            Rr_WriteBufferDescriptor(
                DescriptorsState,
                Args->Set,
                Args->Binding,
                Args->ArrayIndex,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                Rr_GetGraphBuffer(Graph, Args->BufferHandle)->Handle,
                Args->Size,
                Args->Offset);
        }
        break;
        case RR_NODE_FUNCTION_TYPE_BIND_STORAGE_BUFFER:
        {
            Rr_BindStorageBufferArgs *Args = Function->Args;
            Rr_WriteBufferDescriptor(
                DescriptorsState,
                Args->Set,
                Args->Binding,
                Args->ArrayIndex,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                Rr_GetGraphBuffer(Graph, Args->BufferHandle)->Handle,
                Args->Size,
                Args->Offset);
        }
        break;
        case RR_NODE_FUNCTION_TYPE_BIND_STORAGE_IMAGE:
        {
            Rr_BindStorageImageArgs *Args = Function->Args;
            Rr_AllocatedImage *AllocatedImage =
                Rr_GetGraphImage(Graph, Args->ImageHandle);
            VkImageView ImageView = Rr_GetVulkanImageView(
                AllocatedImage,
                &(Rr_ImageViewKey){
                    .SubresourceRange = Args->SubresourceRange,
                    .Type = Args->ViewType,
                });
            Rr_WriteImageDescriptor(
                DescriptorsState,
                Args->Set,
                Args->Binding,
                Args->ArrayIndex,
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                ImageView,
                VK_IMAGE_LAYOUT_GENERAL,
                NULL);
        }
        break;
        case RR_NODE_FUNCTION_TYPE_DEBUG_LABEL:
        {
            const char *LabelName = *(const char **)Function->Args;
            if (LabelName == NULL)
            {
                Rr_EndVulkanCommandBufferLabel(CommandBuffer);
            }
            else
            {
                Rr_BeginVulkanCommandBufferLabel(CommandBuffer, LabelName);
            }
        }
        break;
        default:
            break;
    }
}

static void Rr_ExecuteComputeNode(
    Rr_Graph *Graph,
    Rr_ComputeNode *Node,
    VkCommandBuffer CommandBuffer)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Device *Device = &gRenderer->Device;

    Rr_DescriptorsState DescriptorsState =
        Rr_MakeDescriptorsState(Graph, CommandBuffer);

    for (Rr_NodeFunction *Function = Node->Encoded.EncodedFirst;
         Function != NULL;
         Function = Function->Next)
    {
        switch (Function->Type)
        {
            case RR_NODE_FUNCTION_TYPE_BIND_COMPUTE_PIPELINE:
            {
                Rr_ComputePipeline *ComputePipeline =
                    *(Rr_ComputePipeline **)Function->Args;
                Device->CmdBindPipeline(
                    CommandBuffer,
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    ComputePipeline->Handle);
                Rr_InvalidateDescriptorsState(
                    &DescriptorsState,
                    ComputePipeline->Layout);
            }
            break;
            case RR_NODE_FUNCTION_TYPE_DISPATCH:
            {
                Rr_ApplyDescriptorsState(
                    &DescriptorsState,
                    VK_PIPELINE_BIND_POINT_COMPUTE);
                Rr_DispatchArgs *Args = Function->Args;
                Device->CmdDispatch(
                    CommandBuffer,
                    Args->GroupCountX,
                    Args->GroupCountY,
                    Args->GroupCountZ);
            }
            break;
            case RR_NODE_FUNCTION_TYPE_COMPUTE_BARRIER:
            {
                VkMemoryBarrier MemoryBarrier = {
                    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                    .srcAccessMask =
                        VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                    .dstAccessMask =
                        VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                };
                Device->CmdPipelineBarrier(
                    CommandBuffer,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    0,
                    1,
                    &MemoryBarrier,
                    0,
                    NULL,
                    0,
                    NULL);
            }
            break;
            default:
            {
                Rr_ExecuteGenericEncodedCommands(
                    Graph,
                    Function,
                    &DescriptorsState,
                    CommandBuffer);
            }
            break;
        }
    }

    Rr_DestroyScratch(Scratch);
}

static void Rr_ExecuteGraphicsNode(
    Rr_Graph *Graph,
    Rr_GraphicsNode *Node,
    VkCommandBuffer CommandBuffer)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Device *Device = &gRenderer->Device;

    /* TODO: See if it's possible to skip minimum viewport calculation. */
    Rr_IntVec4 Viewport = {
        .Width = INT32_MAX,
        .Height = INT32_MAX,
    };

    uint32_t AttachmentCount =
        Node->ColorTargetCount * 2 + (Node->DepthTarget ? 1 : 0);

    Rr_RenderPassKey RenderPassKey = {
        .ColorAttachmentCount = (uint8_t)Node->ColorTargetCount,
        .DepthStencil = Node->DepthTarget != NULL,
    };

    VkImageView *ImageViews =
        RR_ALLOC_TYPE_COUNT(VkImageView, AttachmentCount, Scratch.Arena);

    VkClearValue *ClearValues =
        RR_ALLOC_TYPE_COUNT(VkClearValue, AttachmentCount, Scratch.Arena);

    uint32_t ResolveAttachmentIndex = Node->ColorTargetCount;

    for (uint32_t Index = 0; Index < Node->ColorTargetCount; ++Index)
    {
        Rr_ColorTarget *ColorTarget = &Node->ColorTargets[Index];

        memcpy(&ClearValues[Index], &ColorTarget->Clear, sizeof(VkClearValue));

        Rr_AllocatedImage *ColorImage =
            Rr_GetCurrentAllocatedImage(Node->ColorTargets[Index].Image);

        RenderPassKey.Attachments[Index].Samples =
            ColorImage->Container->SampleCount;
        RenderPassKey.Attachments[Index].Format = ColorImage->Container->Format;
        RenderPassKey.Attachments[Index].LoadOp =
            Rr_ToVulkanLoadOp(ColorTarget->LoadOp);
        RenderPassKey.Attachments[Index].StoreOp =
            Rr_ToVulkanStoreOp(ColorTarget->StoreOp);

        ImageViews[Index] = Rr_GetVulkanImageView(
            ColorImage,
            &(Rr_ImageViewKey){
                .SubresourceRange =
                    (VkImageSubresourceRange){
                        .aspectMask = ColorImage->Container->AspectFlags,
                        .baseArrayLayer = ColorTarget->ImageLayerIndex,
                        .layerCount = 1,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                    },
                .Type = VK_IMAGE_VIEW_TYPE_2D,
            });

        Viewport.Width = RR_MIN(
            Viewport.Width,
            (int32_t)ColorImage->Container->Extent.width);
        Viewport.Height = RR_MIN(
            Viewport.Height,
            (int32_t)ColorImage->Container->Extent.height);

        if (ColorTarget->ResolveImage == NULL)
        {
            continue;
        }

        RenderPassKey.ResolveMask |= (uint8_t)(1 << Index);
        RenderPassKey.ResolveAttachmentCount++;

        memcpy(
            &ClearValues[ResolveAttachmentIndex],
            &ColorTarget->Clear,
            sizeof(VkClearValue));

        Rr_AllocatedImage *ResolveImage =
            Rr_GetCurrentAllocatedImage(Node->ColorTargets[Index].ResolveImage);

        RenderPassKey.Attachments[ResolveAttachmentIndex].Samples = 1;
        RenderPassKey.Attachments[ResolveAttachmentIndex].Format =
            ResolveImage->Container->Format;
        RenderPassKey.Attachments[ResolveAttachmentIndex].LoadOp =
            Rr_ToVulkanLoadOp(ColorTarget->ResolveLoadOp);
        RenderPassKey.Attachments[ResolveAttachmentIndex].StoreOp =
            Rr_ToVulkanStoreOp(ColorTarget->ResolveStoreOp);

        ImageViews[ResolveAttachmentIndex] = Rr_GetVulkanImageView(
            ResolveImage,
            &(Rr_ImageViewKey){
                .SubresourceRange =
                    (VkImageSubresourceRange){
                        .aspectMask = ResolveImage->Container->AspectFlags,
                        .baseArrayLayer = ColorTarget->ResolveImageLayerIndex,
                        .layerCount = 1,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                    },
                .Type = VK_IMAGE_VIEW_TYPE_2D,
            });

        ResolveAttachmentIndex++;
    }
    if (Node->DepthTarget != NULL)
    {
        Rr_DepthTarget *DepthTarget = Node->DepthTarget;

        memcpy(
            &ClearValues[ResolveAttachmentIndex],
            &DepthTarget->Clear,
            sizeof(VkClearValue));

        Rr_AllocatedImage *DepthImage =
            Rr_GetCurrentAllocatedImage(Node->DepthTarget->Image);

        RenderPassKey.Attachments[ResolveAttachmentIndex].Samples =
            DepthImage->Container->SampleCount;
        RenderPassKey.Attachments[ResolveAttachmentIndex].Format =
            DepthImage->Container->Format;
        RenderPassKey.Attachments[ResolveAttachmentIndex].LoadOp =
            Rr_ToVulkanLoadOp(DepthTarget->LoadOp);
        RenderPassKey.Attachments[ResolveAttachmentIndex].StoreOp =
            Rr_ToVulkanStoreOp(DepthTarget->StoreOp);

        ImageViews[ResolveAttachmentIndex] = Rr_GetVulkanImageView(
            DepthImage,
            &(Rr_ImageViewKey){
                .SubresourceRange =
                    (VkImageSubresourceRange){
                        .aspectMask = DepthImage->Container->AspectFlags,
                        .baseArrayLayer = Node->DepthTarget->ImageLayerIndex,
                        .layerCount = 1,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                    },
                .Type = VK_IMAGE_VIEW_TYPE_2D,
            });

        Viewport.Width = RR_MIN(
            Viewport.Width,
            (int32_t)DepthImage->Container->Extent.width);
        Viewport.Height = RR_MIN(
            Viewport.Height,
            (int32_t)DepthImage->Container->Extent.height);
    }

    /* Begin render pass. */

    VkRenderPass RenderPass = Rr_GetRenderPass(&RenderPassKey);

    Rr_FramebufferKey FramebufferKey = {
        .Extent =
            (VkExtent3D){
                .width = (uint32_t)Viewport.Width,
                .height = (uint32_t)Viewport.Height,
                .depth = 1,
            },
        .ColorAttachmentCount = RenderPassKey.ColorAttachmentCount,
        .ResolveAttachmentCount = RenderPassKey.ResolveAttachmentCount,
        .DepthStencil = RenderPassKey.DepthStencil,
        .RenderPass = RenderPass,
    };
    memcpy(
        FramebufferKey.ImageViews,
        ImageViews,
        AttachmentCount * sizeof(VkImageView));

    VkFramebuffer Framebuffer = Rr_GetFramebuffer(&FramebufferKey);

    VkRenderPassBeginInfo RenderPassBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = NULL,
        .framebuffer = Framebuffer,
        .renderArea =
            (VkRect2D){
                {
                    Viewport.X,
                    Viewport.Y,
                },
                {
                    (uint32_t)Viewport.Width,
                    (uint32_t)Viewport.Height,
                },
            },
        .renderPass = RenderPass,
        .clearValueCount = (uint32_t)AttachmentCount,
        .pClearValues = ClearValues,
    };
    Device->CmdBeginRenderPass(
        CommandBuffer,
        &RenderPassBeginInfo,
        VK_SUBPASS_CONTENTS_INLINE);

    /* Set dynamic states. */

    Device->CmdSetViewport(
        CommandBuffer,
        0,
        1,
        &(VkViewport){
            .x = (float)Viewport.X,
            .y = (float)Viewport.Y,
            .width = (float)Viewport.Width,
            .height = (float)Viewport.Height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        });

    Device->CmdSetScissor(
        CommandBuffer,
        0,
        1,
        &(VkRect2D){
            .offset.x = Viewport.X,
            .offset.y = Viewport.Y,
            .extent.width = (uint32_t)Viewport.Width,
            .extent.height = (uint32_t)Viewport.Height,
        });

    Rr_DescriptorsState DescriptorsState =
        Rr_MakeDescriptorsState(Graph, CommandBuffer);

    for (Rr_NodeFunction *Function = Node->Encoded.EncodedFirst;
         Function != NULL;
         Function = Function->Next)
    {
        switch (Function->Type)
        {
            case RR_NODE_FUNCTION_TYPE_DRAW:
            {
                Rr_ApplyDescriptorsState(
                    &DescriptorsState,
                    VK_PIPELINE_BIND_POINT_GRAPHICS);
                Rr_DrawArgs *Args = (Rr_DrawArgs *)Function->Args;
                Device->CmdDraw(
                    CommandBuffer,
                    Args->VertexCount,
                    Args->InstanceCount,
                    Args->FirstVertex,
                    Args->FirstInstance);
            }
            break;
            case RR_NODE_FUNCTION_TYPE_DRAW_INDIRECT:
            {
                Rr_ApplyDescriptorsState(
                    &DescriptorsState,
                    VK_PIPELINE_BIND_POINT_GRAPHICS);
                Rr_DrawIndirectArgs *Args =
                    (Rr_DrawIndirectArgs *)Function->Args;
                Device->CmdDrawIndirect(
                    CommandBuffer,
                    Rr_GetGraphBuffer(Graph, Args->BufferHandle)->Handle,
                    Args->Offset,
                    Args->Count,
                    Args->Stride);
            }
            break;
            case RR_NODE_FUNCTION_TYPE_DRAW_INDEXED:
            {
                Rr_ApplyDescriptorsState(
                    &DescriptorsState,
                    VK_PIPELINE_BIND_POINT_GRAPHICS);
                Rr_DrawIndexedArgs *Args = (Rr_DrawIndexedArgs *)Function->Args;
                Device->CmdDrawIndexed(
                    CommandBuffer,
                    Args->IndexCount,
                    Args->InstanceCount,
                    Args->FirstIndex,
                    Args->VertexOffset,
                    Args->FirstInstance);
            }
            break;
            case RR_NODE_FUNCTION_TYPE_DRAW_INDEXED_INDIRECT:
            {
                Rr_ApplyDescriptorsState(
                    &DescriptorsState,
                    VK_PIPELINE_BIND_POINT_GRAPHICS);
                Rr_DrawIndirectArgs *Args =
                    (Rr_DrawIndirectArgs *)Function->Args;
                VkBuffer BufferHandle =
                    Rr_GetGraphBuffer(Graph, Args->BufferHandle)->Handle;
                if (gRenderer->PhysicalDevice.Features.multiDrawIndirect)
                {
                    Device->CmdDrawIndexedIndirect(
                        CommandBuffer,
                        BufferHandle,
                        Args->Offset,
                        Args->Count,
                        Args->Stride);
                }
                else
                {
                    for (uint32_t Index = 0; Index < Args->Count; ++Index)
                    {
                        Device->CmdDrawIndexedIndirect(
                            CommandBuffer,
                            BufferHandle,
                            Args->Offset + (Index * Args->Stride),
                            1,
                            0);
                    }
                }
            }
            break;
            case RR_NODE_FUNCTION_TYPE_BIND_INDEX_BUFFER:
            {
                Rr_BindIndexBufferArgs *Args = Function->Args;
                Device->CmdBindIndexBuffer(
                    CommandBuffer,
                    Rr_GetGraphBuffer(Graph, Args->BufferHandle)->Handle,
                    Args->Offset,
                    Args->Type);
            }
            break;
            case RR_NODE_FUNCTION_TYPE_BIND_VERTEX_BUFFER:
            {
                Rr_BindBufferArgs *Args = Function->Args;
                Device->CmdBindVertexBuffers(
                    CommandBuffer,
                    Args->Slot,
                    1,
                    &Rr_GetGraphBuffer(Graph, Args->BufferHandle)->Handle,
                    &(VkDeviceSize){ Args->Offset });
            }
            break;
            case RR_NODE_FUNCTION_TYPE_BIND_GRAPHICS_PIPELINE:
            {
                Rr_GraphicsPipeline *GraphicsPipeline =
                    *(Rr_GraphicsPipeline **)Function->Args;
                Device->CmdBindPipeline(
                    CommandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    GraphicsPipeline->Handle);
                Rr_InvalidateDescriptorsState(
                    &DescriptorsState,
                    GraphicsPipeline->Layout);
            }
            break;
            case RR_NODE_FUNCTION_TYPE_SET_VIEWPORT:
            {
                Rr_Rect *ViewportRect = Function->Args;
                Device->CmdSetViewport(
                    CommandBuffer,
                    0,
                    1,
                    &(VkViewport){
                        .x = ViewportRect->Offset.X,
                        .y = ViewportRect->Offset.Y,
                        .width = ViewportRect->Extent.Width,
                        .height = ViewportRect->Extent.Height,
                        .minDepth = 0.0f,
                        .maxDepth = 1.0f,
                    });
            }
            break;
            case RR_NODE_FUNCTION_TYPE_SET_SCISSOR:
            {
                Rr_IntRect *ScissorRect = Function->Args;

                Device->CmdSetScissor(
                    CommandBuffer,
                    0,
                    1,
                    &(VkRect2D){
                        .offset = { ScissorRect->Offset.X,
                                    ScissorRect->Offset.Y },
                        .extent = { (uint32_t)ScissorRect->Extent.Width,
                                    (uint32_t)ScissorRect->Extent.Height },
                    });
            }
            break;
            default:
            {
                Rr_ExecuteGenericEncodedCommands(
                    Graph,
                    Function,
                    &DescriptorsState,
                    CommandBuffer);
            }
            break;
        }
    }

    Device->CmdEndRenderPass(CommandBuffer);

    Rr_DestroyScratch(Scratch);
}

static void Rr_ExecuteClearColorImageNode(
    Rr_Graph *Graph,
    Rr_ClearColorImageNode *Node,
    VkCommandBuffer CommandBuffer)
{
    Rr_Device *Device = &gRenderer->Device;

    Rr_AllocatedImage *ColorImage = Rr_GetGraphImage(Graph, Node->ColorImage);

    Device->CmdClearColorImage(
        CommandBuffer,
        ColorImage->Handle,
        VK_IMAGE_LAYOUT_GENERAL,
        (void *)&Node->ColorClear,
        1,
        &(VkImageSubresourceRange){
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        });
}

static void Rr_ExecuteResolveImageNode(
    Rr_Graph *Graph,
    Rr_ResolveImageNode *Node,
    VkCommandBuffer CommandBuffer)
{
    Rr_Device *Device = &gRenderer->Device;

    VkExtent3D MinExtent = { INT32_MAX, INT32_MAX, INT32_MAX };
    MinExtent.width =
        RR_MIN(Node->SrcImage->Extent.width, Node->DstImage->Extent.width);
    MinExtent.height =
        RR_MIN(Node->SrcImage->Extent.height, Node->DstImage->Extent.height);
    MinExtent.depth =
        RR_MIN(Node->SrcImage->Extent.depth, Node->DstImage->Extent.depth);

    Rr_AllocatedImage *SrcAllocatedcImage =
        Rr_GetCurrentAllocatedImage(Node->SrcImage);
    Rr_AllocatedImage *DstAllocatedtImage =
        Rr_GetCurrentAllocatedImage(Node->DstImage);

    VkImageResolve Region = {
        .srcSubresource =
            (VkImageSubresourceLayers){
                .aspectMask = Node->AspectFlags,
                .baseArrayLayer = Node->SrcImageLayerIndex,
                .layerCount = 1,
            },
        .srcOffset = { 0 },
        .dstSubresource =
            (VkImageSubresourceLayers){
                .aspectMask = Node->AspectFlags,
                .baseArrayLayer = Node->DstImageLayerIndex,
                .layerCount = 1,
            },
        .dstOffset = { 0 },
        .extent = MinExtent,
    };

    Device->CmdResolveImage(
        CommandBuffer,
        SrcAllocatedcImage->Handle,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        DstAllocatedtImage->Handle,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &Region);
}

static void Rr_ExecuteCopyBufferToImageNode(
    Rr_Graph *Graph,
    Rr_CopyBufferToImageNode *Node,
    VkCommandBuffer CommandBuffer)
{
    Rr_Device *Device = &gRenderer->Device;

    Rr_AllocatedImage *AllocatedImage = Rr_GetGraphImage(Graph, Node->Image);
    Rr_Image *Image = AllocatedImage->Container;

    VkBuffer BufferHandle = Rr_GetGraphBuffer(Graph, Node->Buffer)->Handle;

    VkBufferImageCopy BufferImageCopy = {
        .bufferOffset = Node->BufferOffset,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource =
            (VkImageSubresourceLayers){
                .aspectMask = Image->AspectFlags,
                .mipLevel = Node->MipLevel,
                .baseArrayLayer = Node->BaseLayer,
                .layerCount = Node->LayerCount,
            },
        .imageExtent = Node->Extent,
    };

    Device->CmdCopyBufferToImage(
        CommandBuffer,
        BufferHandle,
        AllocatedImage->Handle,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &BufferImageCopy);
}

static void Rr_ExecuteCopyImageToBufferNode(
    Rr_Graph *Graph,
    Rr_CopyImageToBufferNode *Node,
    VkCommandBuffer CommandBuffer)
{
    Rr_Device *Device = &gRenderer->Device;

    VkImage ImageHandle = Rr_GetGraphImage(Graph, Node->Image)->Handle;
    VkBuffer BufferHandle = Rr_GetGraphBuffer(Graph, Node->Buffer)->Handle;

    Device->CmdCopyImageToBuffer(
        CommandBuffer,
        ImageHandle,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        BufferHandle,
        Node->BufferImageCopyCount,
        Node->BufferImageCopies);
}

static void Rr_ExecuteCopyImageNode(
    Rr_Graph *Graph,
    Rr_CopyImageNode *Node,
    VkCommandBuffer CommandBuffer)
{
    Rr_Device *Device = &gRenderer->Device;

    Rr_AllocatedImage *SrcAllocatedImage =
        Rr_GetGraphImage(Graph, Node->SrcImage);
    Rr_Image *SrcImage = SrcAllocatedImage->Container;

    Rr_AllocatedImage *DstAllocatedImage =
        Rr_GetGraphImage(Graph, Node->DstImage);
    Rr_Image *DstImage = DstAllocatedImage->Container;

    assert(SrcImage->AspectFlags == DstImage->AspectFlags);

    VkImageCopy ImageCopy = {
        .srcSubresource =
            (VkImageSubresourceLayers){
                .aspectMask = SrcImage->AspectFlags,
                .mipLevel = Node->MipLevel,
                .baseArrayLayer = Node->BaseLayer,
                .layerCount = Node->LayerCount,
            },
        .srcOffset = { Node->SrcOffset.X,
                       Node->SrcOffset.Y,
                       Node->SrcOffset.Z },
        .dstSubresource =
            (VkImageSubresourceLayers){
                .aspectMask = DstImage->AspectFlags,
                .mipLevel = Node->MipLevel,
                .baseArrayLayer = Node->BaseLayer,
                .layerCount = Node->LayerCount,
            },
        .dstOffset = { Node->DstOffset.X,
                       Node->DstOffset.Y,
                       Node->DstOffset.Z },
        .extent = {
            (uint32_t)Node->Extent.Width,
            (uint32_t)Node->Extent.Height,
            (uint32_t)Node->Extent.Depth,
        },
    };

    Device->CmdCopyImage(
        CommandBuffer,
        SrcAllocatedImage->Handle,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        DstAllocatedImage->Handle,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &ImageCopy);
}

static void Rr_ExecuteGraphNode(
    Rr_Graph *Graph,
    Rr_GraphNode *Node,
    VkCommandBuffer CommandBuffer)
{
#ifdef RR_USE_GPU_DEBUG_UTILS
    bool SetVulkanMarker = Node->Name;

    if (SetVulkanMarker)
    {
        Rr_BeginVulkanCommandBufferLabel(CommandBuffer, Node->Name);
    }
#endif

    switch (Node->Type)
    {
        case RR_GRAPH_NODE_TYPE_COMPUTE:
        {
            Rr_ExecuteComputeNode(Graph, &Node->Union.Compute, CommandBuffer);
        }
        break;
        case RR_GRAPH_NODE_TYPE_GRAPHICS:
        {
            Rr_ExecuteGraphicsNode(Graph, &Node->Union.Graphics, CommandBuffer);
        }
        break;
        case RR_GRAPH_NODE_TYPE_CLEAR_COLOR_IMAGE:
        {
            Rr_ExecuteClearColorImageNode(
                Graph,
                &Node->Union.ClearColorImage,
                CommandBuffer);
        }
        break;
        case RR_GRAPH_NODE_TYPE_RESOLVE_IMAGE:
        {
            Rr_ExecuteResolveImageNode(
                Graph,
                &Node->Union.ResolveImage,
                CommandBuffer);
        }
        break;
        case RR_GRAPH_NODE_TYPE_COPY_BUFFER_TO_IMAGE:
        {
            Rr_ExecuteCopyBufferToImageNode(
                Graph,
                &Node->Union.CopyBufferToImage,
                CommandBuffer);
        }
        break;
        case RR_GRAPH_NODE_TYPE_COPY_IMAGE_TO_BUFFER:
        {
            Rr_ExecuteCopyImageToBufferNode(
                Graph,
                &Node->Union.CopyImageToBuffer,
                CommandBuffer);
        }
        break;
        case RR_GRAPH_NODE_TYPE_COPY_IMAGE:
        {
            Rr_ExecuteCopyImageNode(
                Graph,
                &Node->Union.CopyImage,
                CommandBuffer);
        }
        break;
        case RR_GRAPH_NODE_TYPE_BLIT:
        {
            Rr_ExecuteBlitNode(Graph, &Node->Union.Blit, CommandBuffer);
        }
        break;
        case RR_GRAPH_NODE_TYPE_TRANSFER:
        {
            Rr_ExecuteTransferNode(Graph, &Node->Union.Transfer, CommandBuffer);
        }
        break;
        case RR_GRAPH_NODE_TYPE_GENERATE_MIPMAPS:
        {
            Rr_ExecuteGenerateMipmaps(
                Graph,
                Node->Union.GenerateMipmaps,
                CommandBuffer);
        }
        break;
        default:
        {
            RR_LOG_ABORT("Unsupported node type!");
        }
        break;
    }

#ifdef RR_USE_GPU_DEBUG_UTILS
    if (SetVulkanMarker)
    {
        Rr_EndVulkanCommandBufferLabel(CommandBuffer);
    }
#endif
}

static void Rr_ApplyBarrierBatch(
    Rr_BarrierBatch *Batch,
    VkCommandBuffer CommandBuffer)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Device *Device = &gRenderer->Device;

    uint32_t MaxPossibleBarriers = (uint32_t)Batch->BufferBarriers.Count +
                                   (uint32_t)Batch->ImageBarriers.Count;

    if (MaxPossibleBarriers == 0)
    {
        return;
    }

    VkPipelineStageFlags SrcStageMaskEarly = 0;
    VkPipelineStageFlags DstStageMaskEarly = 0;
    RR_ARRAY(VkBufferMemoryBarrier) BufferBarriersEarly = { 0 };
    RR_RESERVE_ARRAY(
        &BufferBarriersEarly,
        Batch->BufferBarriers.Count,
        Scratch.Arena);
    RR_ARRAY(VkImageMemoryBarrier) ImageBarriersEarly = { 0 };
    RR_RESERVE_ARRAY(
        &ImageBarriersEarly,
        Batch->ImageBarriers.Count,
        Scratch.Arena);

    /* TODO: Get rid of RR_ARRAY here? */

    VkPipelineStageFlags SrcStageMask = 0;
    VkPipelineStageFlags DstStageMask = 0;
    RR_ARRAY(VkBufferMemoryBarrier) BufferBarriers = { 0 };
    RR_RESERVE_ARRAY(
        &BufferBarriers,
        Batch->BufferBarriers.Count,
        Scratch.Arena);
    RR_ARRAY(VkImageMemoryBarrier) ImageBarriers = { 0 };
    RR_RESERVE_ARRAY(&ImageBarriers, Batch->ImageBarriers.Count, Scratch.Arena);

    for (size_t Index = 0; Index < Batch->BufferBarriers.Count; ++Index)
    {
        Rr_BufferMemoryBarrier *BufferBarrier =
            Batch->BufferBarriers.Data + Index;

        RR_ARRAY(VkBufferMemoryBarrier) * BarriersArray;
        if (BufferBarrier->DstStageMask <= RR_VULKAN_EARLY_STAGES)
        {
            SrcStageMaskEarly |= BufferBarrier->SrcStageMask;
            DstStageMaskEarly |= BufferBarrier->DstStageMask;
            BarriersArray = (void *)&BufferBarriersEarly;
        }
        else
        {
            SrcStageMask |= BufferBarrier->SrcStageMask;
            DstStageMask |= BufferBarrier->DstStageMask;
            BarriersArray = (void *)&BufferBarriers;
        }

        *RR_PUSH_INTO_ARRAY(BarriersArray, Scratch.Arena) =
            (VkBufferMemoryBarrier){
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                .buffer = BufferBarrier->Buffer,
                .srcAccessMask = BufferBarrier->SrcAccessMask,
                .dstAccessMask = BufferBarrier->DstAccessMask,
                .offset = 0,
                .size = VK_WHOLE_SIZE,
                .srcQueueFamilyIndex = BufferBarrier->SrcQueueFamilyIndex,
                .dstQueueFamilyIndex = BufferBarrier->DstQueueFamilyIndex,
            };
    }

    for (size_t Index = 0; Index < Batch->ImageBarriers.Count; ++Index)
    {
        Rr_ImageMemoryBarrier *ImageBarrier = Batch->ImageBarriers.Data + Index;

        RR_ARRAY(VkImageMemoryBarrier) * BarriersArray;
        if (ImageBarrier->DstStageMask <= RR_VULKAN_EARLY_STAGES)
        {
            SrcStageMaskEarly |= ImageBarrier->SrcStageMask;
            DstStageMaskEarly |= ImageBarrier->DstStageMask;
            BarriersArray = (void *)&ImageBarriersEarly;
        }
        else
        {
            SrcStageMask |= ImageBarrier->SrcStageMask;
            DstStageMask |= ImageBarrier->DstStageMask;
            BarriersArray = (void *)&ImageBarriers;
        }

        *RR_PUSH_INTO_ARRAY(BarriersArray, Scratch.Arena) =
            (VkImageMemoryBarrier){
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .image = ImageBarrier->Image,
                .srcAccessMask = ImageBarrier->SrcAccessMask,
                .dstAccessMask = ImageBarrier->DstAccessMask,
                .oldLayout = ImageBarrier->OldLayout,
                .newLayout = ImageBarrier->NewLayout,
                .subresourceRange = ImageBarrier->SubresourceRange,
                .srcQueueFamilyIndex = ImageBarrier->SrcQueueFamilyIndex,
                .dstQueueFamilyIndex = ImageBarrier->DstQueueFamilyIndex,
            };
    }

    if (BufferBarriersEarly.Count > 0 || ImageBarriersEarly.Count > 0)
    {
        Device->CmdPipelineBarrier(
            CommandBuffer,
            SrcStageMaskEarly,
            DstStageMaskEarly,
            0,
            0,
            NULL,
            (uint32_t)BufferBarriersEarly.Count,
            BufferBarriersEarly.Data,
            (uint32_t)ImageBarriersEarly.Count,
            ImageBarriersEarly.Data);
    }

    if (BufferBarriers.Count > 0 || ImageBarriers.Count > 0)
    {
        Device->CmdPipelineBarrier(
            CommandBuffer,
            SrcStageMask,
            DstStageMask,
            0,
            0,
            NULL,
            (uint32_t)BufferBarriers.Count,
            BufferBarriers.Data,
            (uint32_t)ImageBarriers.Count,
            ImageBarriers.Data);
    }

    RR_CLEAR_ARRAY(&Batch->ImageBarriers);
    RR_CLEAR_ARRAY(&Batch->BufferBarriers);
    Batch->VulkanHandleToBarrier = NULL;

    Rr_DestroyScratch(Scratch);
}

void Rr_ExecuteGraph(
    Rr_Graph *Graph,
    uint32_t QueueFamilyIndex,
    VkCommandBuffer EarlyCommandBuffer,
    VkCommandBuffer LateCommandBuffer)
{
    Rr_BeginFrameSection("Rr.ExecuteGraph");

    if (Graph->Nodes.Count == 0)
    {
        return;
    }

    assert(
        EarlyCommandBuffer != VK_NULL_HANDLE ||
        LateCommandBuffer != VK_NULL_HANDLE);

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Device *Device = &gRenderer->Device;

    if (EarlyCommandBuffer == VK_NULL_HANDLE)
    {
        EarlyCommandBuffer = LateCommandBuffer;
    }
    if (LateCommandBuffer == VK_NULL_HANDLE)
    {
        LateCommandBuffer = EarlyCommandBuffer;
    }

    Rr_NodeArray SortedNodes = { 0 };
    RR_RESERVE_ARRAY(&SortedNodes, Graph->Nodes.Count, Scratch.Arena);

    Rr_ProcessGraphNodes(Graph, &SortedNodes, Scratch.Arena);

#ifdef RR_USE_GPU_DEBUG_UTILS
    assert(Graph->DebugLabelNames.Count == Graph->DebugLabelStates.Count);
    size_t DebugLabelCount = Graph->DebugLabelStates.Count;
    size_t EarlyDebugLabelCount = 0;
    bool *EarlyDebugLabelStates = NULL;
    size_t LateDebugLabelCount = 0;
    bool *LateDebugLabelStates = NULL;
    if (DebugLabelCount)
    {
        EarlyDebugLabelStates =
            RR_ALLOC(sizeof(bool) * DebugLabelCount, Scratch.Arena);
        LateDebugLabelStates =
            RR_ALLOC(sizeof(bool) * DebugLabelCount, Scratch.Arena);
    }
#endif

    /* Resolve all referenced resources. */

    uint32_t BufferOwnershipTransferCount = 0;
    for (size_t Index = 0; Index < Graph->BufferResources.Count; ++Index)
    {
        Rr_GraphResource *Resource = Graph->BufferResources.Data + Index;
        Rr_AllocatedBuffer *AllocatedBuffer =
            Rr_GetCurrentAllocatedBuffer(Resource->Container);
        Resource->Allocated = AllocatedBuffer;
        Resource->SyncState = AllocatedBuffer->SyncState;
        if (Resource->DstQueueFamilyIndex != VK_QUEUE_FAMILY_IGNORED &&
            Resource->DstQueueFamilyIndex != QueueFamilyIndex)
        {
            BufferOwnershipTransferCount++;
        }
    }

    uint32_t ImageOwnershipTransferCount = 0;
    for (size_t Index = 0; Index < Graph->ImageResources.Count; ++Index)
    {
        Rr_GraphResource *Resource = Graph->ImageResources.Data + Index;
        Rr_AllocatedImage *AllocatedImage =
            Rr_GetCurrentAllocatedImage(Resource->Container);
        Resource->Allocated = AllocatedImage;
        Resource->SyncState = AllocatedImage->SyncState;
        if (Resource->DstQueueFamilyIndex != VK_QUEUE_FAMILY_IGNORED &&
            Resource->DstQueueFamilyIndex != QueueFamilyIndex)
        {
            ImageOwnershipTransferCount++;
        }
    }

    size_t DependencyLevel = SortedNodes.Data[0]->DependencyLevel;

    Rr_BarrierBatch BarrierBatch = { 0 };
    RR_RESERVE_ARRAY(
        &BarrierBatch.BufferBarriers,
        Graph->BufferResources.Count,
        Scratch.Arena);
    RR_RESERVE_ARRAY(
        &BarrierBatch.ImageBarriers,
        Graph->ImageResources.Count,
        Scratch.Arena);

    size_t BatchStartIndex = 0;
    size_t BatchSize = 0;

    bool UseLateCommandBuffer = false;

    for (size_t Index = 0; Index < SortedNodes.Count; ++Index)
    {
        Rr_GraphNode *Node = SortedNodes.Data[Index];
        UseLateCommandBuffer |= Node->UsesLateCommandBuffer;
        DependencyLevel = Node->DependencyLevel;

        for (size_t DepIndex = 0; DepIndex < Node->BufferDeps.Count; ++DepIndex)
        {
            Rr_NodeDependency *Dependency = &Node->BufferDeps.Data[DepIndex];
            Rr_SyncState *DstState = &Dependency->State;

            /* Buffer Synchronization */

            Rr_GraphResource *BufferResource =
                Rr_GetGraphBufferResource(Graph, Dependency->Handle);
            Rr_AllocatedBuffer *AllocatedBuffer = BufferResource->Allocated;
            Rr_SyncState *SrcState = &BufferResource->SyncState;
            VkBuffer Buffer = AllocatedBuffer->Handle;

            /* If reading again, just make sure the memory is "available" to
             * this memory domain. */

            bool TransferOwnership =
                SrcState->QueueFamilyIndex != QueueFamilyIndex &&
                SrcState->QueueFamilyIndex != VK_QUEUE_FAMILY_IGNORED;
            bool IsReadingNow = !(DstState->AccessMask & RR_VULKAN_WRITES);
            bool WasReadingBefore = !(SrcState->AccessMask & RR_VULKAN_WRITES);
            if (!TransferOwnership && IsReadingNow && WasReadingBefore)
            {
                bool IncludesPreviousAccessMask =
                    (SrcState->AccessMask & DstState->AccessMask) ==
                    DstState->AccessMask;
                if (IncludesPreviousAccessMask)
                {
                    /* Skip this barrier! */

                    continue;
                }
            }

            Rr_BufferMemoryBarrier **BufferBarrierRef = RR_FIND_IN_HASH_TRIE(
                &BarrierBatch.VulkanHandleToBarrier,
                Buffer,
                Scratch.Arena);
            Rr_BufferMemoryBarrier *BufferBarrier = *BufferBarrierRef;
            if (BufferBarrier == NULL)
            {
                *BufferBarrierRef = RR_PUSH_INTO_ARRAY(
                    &BarrierBatch.BufferBarriers,
                    Scratch.Arena);
                BufferBarrier = *BufferBarrierRef;
                *BufferBarrier = (Rr_BufferMemoryBarrier){
                    .SrcStageMask = SrcState->StageMask,
                    .DstStageMask = DstState->StageMask,
                    .Buffer = Buffer,
                    .SrcAccessMask = SrcState->AccessMask,
                    .DstAccessMask = DstState->AccessMask,
                    .Offset = 0,
                    .Size = VK_WHOLE_SIZE,
                };
            }
            else
            {
                BufferBarrier->SrcStageMask |= SrcState->StageMask;
                BufferBarrier->DstStageMask |= DstState->StageMask;
                BufferBarrier->SrcAccessMask |= SrcState->AccessMask;
                BufferBarrier->DstAccessMask |= DstState->AccessMask;
            }

            if (TransferOwnership)
            {
                /* BufferBarrier->SrcAccessMask = 0; */
                BufferBarrier->SrcQueueFamilyIndex = SrcState->QueueFamilyIndex;
                BufferBarrier->DstQueueFamilyIndex = QueueFamilyIndex;
            }
            else
            {
                BufferBarrier->SrcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                BufferBarrier->DstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            }

            SrcState->StageMask = BufferBarrier->DstStageMask;
            SrcState->AccessMask = BufferBarrier->DstAccessMask;
            SrcState->QueueFamilyIndex = QueueFamilyIndex;
        }

        for (size_t DepIndex = 0; DepIndex < Node->ImageDeps.Count; ++DepIndex)
        {
            Rr_NodeDependency *Dependency = &Node->ImageDeps.Data[DepIndex];
            Rr_SyncState *DstState = &Dependency->State;

            /* Image Synchronization */

            Rr_GraphResource *ImageResource =
                Rr_GetGraphImageResource(Graph, Dependency->Handle);
            Rr_AllocatedImage *AllocatedImage = ImageResource->Allocated;
            Rr_SyncState *SrcState = &ImageResource->SyncState;
            VkImage Image = AllocatedImage->Handle;

            /* If reading again, just make sure the memory is "available" to
             * this memory domain AND the image is in the same layout. */

            bool TransferOwnership =
                SrcState->QueueFamilyIndex != QueueFamilyIndex &&
                SrcState->QueueFamilyIndex != VK_QUEUE_FAMILY_IGNORED;
            bool IsReadingNow = !(DstState->AccessMask & RR_VULKAN_WRITES);
            bool WasReadingBefore = !(SrcState->AccessMask & RR_VULKAN_WRITES);
            bool IsSameLayout = DstState->Layout == SrcState->Layout;
            if (!TransferOwnership && IsReadingNow && WasReadingBefore &&
                IsSameLayout)
            {
                bool IncludesPreviousAccessMask =
                    (SrcState->AccessMask & DstState->AccessMask) ==
                    DstState->AccessMask;
                if (IncludesPreviousAccessMask)
                {
                    /* Skip this barrier! */

                    continue;
                }
            }

            Rr_ImageMemoryBarrier **ImageBarrierRef = RR_FIND_IN_HASH_TRIE(
                &BarrierBatch.VulkanHandleToBarrier,
                Image,
                Scratch.Arena);
            Rr_ImageMemoryBarrier *ImageBarrier = *ImageBarrierRef;
            if (ImageBarrier == NULL)
            {
                *ImageBarrierRef = RR_PUSH_INTO_ARRAY(
                    &BarrierBatch.ImageBarriers,
                    Scratch.Arena);
                ImageBarrier = *ImageBarrierRef;
                *ImageBarrier = (Rr_ImageMemoryBarrier){
                    .SrcStageMask = SrcState->StageMask,
                    .DstStageMask = DstState->StageMask,
                    .Image = Image,
                    .SrcAccessMask = SrcState->AccessMask,
                    .DstAccessMask = DstState->AccessMask,
                    .OldLayout = SrcState->Layout,
                    .NewLayout = DstState->Layout,
                    .SubresourceRange =
                        (VkImageSubresourceRange){
                            .aspectMask =
                                AllocatedImage->Container->AspectFlags,
                            .baseMipLevel = 0,
                            .levelCount = VK_REMAINING_MIP_LEVELS,
                            .baseArrayLayer = 0,
                            .layerCount = VK_REMAINING_ARRAY_LAYERS,
                        },
                };
            }
            else
            {
                RR_LOG_ABORT("Multiple image layout transitions!");
            }

            if (TransferOwnership)
            {
                /* ImageBarrier->SrcAccessMask = 0; */
                ImageBarrier->SrcQueueFamilyIndex = SrcState->QueueFamilyIndex;
                ImageBarrier->DstQueueFamilyIndex = QueueFamilyIndex;
            }
            else
            {
                ImageBarrier->SrcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                ImageBarrier->DstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            }

            SrcState->StageMask = ImageBarrier->DstStageMask;
            SrcState->AccessMask = ImageBarrier->DstAccessMask;
            SrcState->Layout = ImageBarrier->NewLayout;
            SrcState->QueueFamilyIndex = QueueFamilyIndex;
        }

        BatchSize++;

        bool LastNode = false;
        bool LastNodeThisLevel = false;
        if (Index + 1 < SortedNodes.Count)
        {
            LastNodeThisLevel =
                SortedNodes.Data[Index + 1]->DependencyLevel != DependencyLevel;
        }
        else
        {
            LastNode = true;
        }
        if (LastNode || LastNodeThisLevel)
        {
            /* Execute current batch now! */

            VkCommandBuffer CommandBuffer =
                UseLateCommandBuffer ? LateCommandBuffer : EarlyCommandBuffer;

            Rr_ApplyBarrierBatch(&BarrierBatch, CommandBuffer);

            for (size_t NodeIndex = BatchStartIndex;
                 NodeIndex < BatchStartIndex + BatchSize;
                 ++NodeIndex)
            {
                Rr_GraphNode *BatchedNode = SortedNodes.Data[NodeIndex];

#ifdef RR_USE_GPU_DEBUG_UTILS
                bool *BufferStates = UseLateCommandBuffer
                                         ? LateDebugLabelStates
                                         : EarlyDebugLabelStates;
                size_t *BufferCount = UseLateCommandBuffer
                                          ? &LateDebugLabelCount
                                          : &EarlyDebugLabelCount;
                for (size_t DebugLabelIndex = 0;
                     DebugLabelIndex < BatchedNode->DebugLabelCount;
                     ++DebugLabelIndex)
                {
                    bool NodeLabelEnabled =
                        BatchedNode->DebugLabelStates[DebugLabelIndex];
                    bool BufferLabelEnabled = BufferStates[DebugLabelIndex];
                    if (NodeLabelEnabled && !BufferLabelEnabled)
                    {
                        ++(*BufferCount);
                        BufferStates[DebugLabelIndex] = true;
                        Rr_BeginVulkanCommandBufferLabel(
                            CommandBuffer,
                            Graph->DebugLabelNames.Data[DebugLabelIndex]);
                    }
                    if (!NodeLabelEnabled && BufferLabelEnabled)
                    {
                        --(*BufferCount);
                        BufferStates[DebugLabelIndex] = false;
                        Rr_EndVulkanCommandBufferLabel(CommandBuffer);
                    }
                }
#endif

                Rr_ExecuteGraphNode(Graph, BatchedNode, CommandBuffer);
            }

            BatchStartIndex = Index + 1;
            BatchSize = 0;
            UseLateCommandBuffer = 0;
        }
    }

    /* Write updated sync state back to storage. */

    VkPipelineStageFlags OwnershipTransferStage = 0;

    uint32_t BufferOwnershipTransferIndex = 0;
    VkBufferMemoryBarrier *BufferOwnershipTransferBarriers = NULL;
    if (BufferOwnershipTransferCount)
    {
        BufferOwnershipTransferBarriers = RR_ALLOC_NO_ZERO(
            sizeof(VkBufferMemoryBarrier) * BufferOwnershipTransferCount,
            Scratch.Arena);
    }
    for (size_t Index = 0; Index < Graph->BufferResources.Count; ++Index)
    {
        Rr_GraphResource *Resource = Graph->BufferResources.Data + Index;
        Rr_AllocatedBuffer *AllocatedBuffer = Resource->Allocated;
        AllocatedBuffer->SyncState = Resource->SyncState;

        if (Resource->DstQueueFamilyIndex != VK_QUEUE_FAMILY_IGNORED)
        {
            OwnershipTransferStage |= Resource->SyncState.StageMask;
            BufferOwnershipTransferBarriers[BufferOwnershipTransferIndex++] =
                (VkBufferMemoryBarrier){
                    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                    .srcAccessMask = Resource->SyncState.AccessMask,
                    .dstAccessMask = 0,
                    .srcQueueFamilyIndex = Resource->SyncState.QueueFamilyIndex,
                    .dstQueueFamilyIndex = Resource->DstQueueFamilyIndex,
                    .buffer = AllocatedBuffer->Handle,
                    .offset = 0,
                    .size = VK_WHOLE_SIZE,
                };
            Resource->DstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        }
    }

    uint32_t ImageOwnershipTransferIndex = 0;
    VkImageMemoryBarrier *ImageOwnershipTransferBarriers = NULL;
    if (ImageOwnershipTransferCount)
    {
        ImageOwnershipTransferBarriers = RR_ALLOC_NO_ZERO(
            sizeof(VkImageMemoryBarrier) * ImageOwnershipTransferCount,
            Scratch.Arena);
    }
    for (size_t Index = 0; Index < Graph->ImageResources.Count; ++Index)
    {
        Rr_GraphResource *Resource = Graph->ImageResources.Data + Index;
        Rr_AllocatedImage *AllocatedImage = Resource->Allocated;
        AllocatedImage->SyncState = Resource->SyncState;

        if (Resource->DstQueueFamilyIndex != VK_QUEUE_FAMILY_IGNORED)
        {
            OwnershipTransferStage |= Resource->SyncState.StageMask;
            ImageOwnershipTransferBarriers[ImageOwnershipTransferIndex++] =
                (VkImageMemoryBarrier){
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .srcAccessMask = Resource->SyncState.AccessMask,
                    .dstAccessMask = 0,
                    .oldLayout = Resource->SyncState.Layout,
                    .newLayout = Resource->SyncState.Layout,
                    .srcQueueFamilyIndex = Resource->SyncState.QueueFamilyIndex,
                    .dstQueueFamilyIndex = Resource->DstQueueFamilyIndex,
                    .image = AllocatedImage->Handle,
                    .subresourceRange =
                        (VkImageSubresourceRange){
                            .aspectMask =
                                AllocatedImage->Container->AspectFlags,
                            .baseMipLevel = 0,
                            .levelCount = VK_REMAINING_MIP_LEVELS,
                            .baseArrayLayer = 0,
                            .layerCount = VK_REMAINING_ARRAY_LAYERS,
                        },
                };
            Resource->DstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        }
    }

    if (BufferOwnershipTransferCount || ImageOwnershipTransferCount)
    {
        Device->CmdPipelineBarrier(
            LateCommandBuffer,
            OwnershipTransferStage,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0,
            NULL,
            BufferOwnershipTransferCount,
            BufferOwnershipTransferBarriers,
            ImageOwnershipTransferCount,
            ImageOwnershipTransferBarriers);
    }

#ifdef RR_USE_GPU_DEBUG_UTILS
    for (size_t Index = 0; Index < EarlyDebugLabelCount; ++Index)
    {
        Rr_EndVulkanCommandBufferLabel(EarlyCommandBuffer);
    }
    for (size_t Index = 0; Index < LateDebugLabelCount; ++Index)
    {
        Rr_EndVulkanCommandBufferLabel(LateCommandBuffer);
    }
#endif

    Rr_DestroyScratch(Scratch);

    Rr_EndFrameSection("Rr.ExecuteGraph");
}

void Rr_ReleaseGraphResources(Rr_Graph *Graph)
{
    Rr_ReleaseDescriptorPoolList(Graph->DescriptorPoolList);
    Rr_DecrementRefCounts(Graph);
}

static inline Rr_GraphImage *Rr_GetGraphHandle(
    Rr_Graph *Graph,
    Rr_GraphResourceArray *ResourceArray,
    Rr_HashTrie **Handles,
    void *Container,
    Rr_AtomicInt *RefCount)
{
    assert(Container != NULL);

    Rr_GraphHandle **GraphHandle =
        RR_FIND_IN_HASH_TRIE(Handles, (Rr_HashTrieKey)Container, Graph->Arena);
    if (*GraphHandle == NULL)
    {
        Rr_GraphImage Handle = {
            .Values.Index = (uint32_t)ResourceArray->Count,
        };
        *RR_PUSH_INTO_ARRAY(ResourceArray, Graph->Arena) = (Rr_GraphResource){
            .Container = Container,
            .DstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        };
        *GraphHandle = RR_ALLOC_TYPE(Rr_GraphHandle, Graph->Arena);
        **GraphHandle = Handle;

        Rr_IncrementAtomicRelaxed(RefCount);
    }

    return *GraphHandle;
}

Rr_GraphBuffer *Rr_GetGraphBufferHandle(Rr_Graph *Graph, Rr_Buffer *Container)
{
    return Rr_GetGraphHandle(
        Graph,
        &Graph->BufferResources,
        &Graph->BufferHandles,
        Container,
        &Container->RefCount);
}

Rr_GraphImage *Rr_GetGraphImageHandle(Rr_Graph *Graph, Rr_Image *Container)
{
    return Rr_GetGraphHandle(
        Graph,
        &Graph->ImageResources,
        &Graph->ImageHandles,
        Container,
        &Container->RefCount);
}

void Rr_MarkSamplerUsed(Rr_Graph *Graph, Rr_Sampler *Sampler)
{
    if (Rr_AddHandleToSet(&Graph->Samplers, Sampler, Graph->Arena))
    {
        Rr_IncrementAtomicRelaxed(&Sampler->RefCount);
    }
}

void Rr_MarkComputePipelineUsed(
    Rr_Graph *Graph,
    Rr_ComputePipeline *ComputePipeline)
{
    if (Rr_AddHandleToSet(
            &Graph->ComputePipelines,
            ComputePipeline,
            Graph->Arena))
    {
        Rr_IncrementAtomicRelaxed(&ComputePipeline->RefCount);
    }
}

void Rr_MarkGraphicsPipelineUsed(
    Rr_Graph *Graph,
    Rr_GraphicsPipeline *GraphicsPipeline)
{
    if (Rr_AddHandleToSet(
            &Graph->GraphicsPipelines,
            GraphicsPipeline,
            Graph->Arena))
    {
        Rr_IncrementAtomicRelaxed(&GraphicsPipeline->RefCount);
    }
}

void Rr_TransferBufferToQueue(
    Rr_Graph *Graph,
    Rr_Buffer *Buffer,
    Rr_QueueType QueueType)
{
    Rr_GraphHandle *Handle = Rr_GetGraphBufferHandle(Graph, Buffer);
    Rr_GraphResource *Resource = Rr_GetGraphBufferResource(Graph, *Handle);
    Resource->DstQueueFamilyIndex = Rr_GetQueue(QueueType)->FamilyIndex;
}

static void Rr_TransferImageToQueue(
    Rr_Graph *Graph,
    Rr_Image *Image,
    Rr_QueueType QueueType)
{
    Rr_GraphHandle *Handle = Rr_GetGraphImageHandle(Graph, Image);
    Rr_GraphResource *Resource = Rr_GetGraphImageResource(Graph, *Handle);
    Resource->DstQueueFamilyIndex = Rr_GetQueue(QueueType)->FamilyIndex;
}

void Rr_TransferImage2DToQueue(
    Rr_Graph *Graph,
    Rr_Image2D *Image,
    Rr_QueueType QueueType)
{
    Rr_TransferImageToQueue(Graph, Image, QueueType);
}

void Rr_TransferImage2DArrayToQueue(
    Rr_Graph *Graph,
    Rr_Image2DArray *Image,
    Rr_QueueType QueueType)
{
    Rr_TransferImageToQueue(Graph, Image, QueueType);
}

void Rr_TransferImage3DToQueue(
    Rr_Graph *Graph,
    Rr_Image3D *Image,
    Rr_QueueType QueueType)
{
    Rr_TransferImageToQueue(Graph, Image, QueueType);
}

void Rr_TransferImageCubeToQueue(
    Rr_Graph *Graph,
    Rr_ImageCube *Image,
    Rr_QueueType QueueType)
{
    Rr_TransferImageToQueue(Graph, Image, QueueType);
}

void Rr_DecrementRefCounts(Rr_Graph *Graph)
{
    for (size_t Index = 0; Index < Graph->BufferResources.Count; ++Index)
    {
        Rr_GraphResource *BufferResource = &Graph->BufferResources.Data[Index];
        Rr_Buffer *Buffer = (Rr_Buffer *)BufferResource->Container;
        Rr_DecrementAtomicRelaxed(&Buffer->RefCount);
    }

    for (size_t Index = 0; Index < Graph->ImageResources.Count; ++Index)
    {
        Rr_GraphResource *ImageResource = &Graph->ImageResources.Data[Index];
        Rr_Image *Image = (Rr_Image *)ImageResource->Container;
        Rr_DecrementAtomicRelaxed(&Image->RefCount);
    }

    for (Rr_HandleTrieHiveIterator It = Graph->Samplers.Hive.Begin;
         It.Element != Graph->Samplers.Hive.End.Element;
         Rr_AdvanceHandleTrieHiveIterator(&It))
    {
        Rr_Sampler *Sampler = (Rr_Sampler *)It.Element->Handle;
        Rr_DecrementAtomicRelaxed(&Sampler->RefCount);
    }

    for (Rr_HandleTrieHiveIterator It = Graph->ComputePipelines.Hive.Begin;
         It.Element != Graph->ComputePipelines.Hive.End.Element;
         Rr_AdvanceHandleTrieHiveIterator(&It))
    {
        Rr_ComputePipeline *ComputePipeline =
            (Rr_ComputePipeline *)It.Element->Handle;
        Rr_DecrementAtomicRelaxed(&ComputePipeline->RefCount);
    }

    for (Rr_HandleTrieHiveIterator It = Graph->GraphicsPipelines.Hive.Begin;
         It.Element != Graph->GraphicsPipelines.Hive.End.Element;
         Rr_AdvanceHandleTrieHiveIterator(&It))
    {
        Rr_GraphicsPipeline *GraphicsPipeline =
            (Rr_GraphicsPipeline *)It.Element->Handle;
        Rr_DecrementAtomicRelaxed(&GraphicsPipeline->RefCount);
    }
}

void Rr_SetNextNodeName(Rr_Graph *Graph, const char *Name)
{
    size_t NameLength = strlen(Name);
    char *NodeName = RR_ALLOC_NO_ZERO(NameLength + 1, Graph->Arena);
    memcpy(NodeName, Name, NameLength + 1);
    Graph->NextNodeName = NodeName;
}

void Rr_BeginGraphLabel(Rr_Graph *Graph, const char *Name)
{
#ifdef RR_USE_GPU_DEBUG_UTILS
    assert(Graph);
    assert(Name);

    for (size_t Index = 0; Index < Graph->DebugLabelNames.Count; ++Index)
    {
        const char *DebugLabel = Graph->DebugLabelNames.Data[Index];
        if (strcmp(DebugLabel, Name) == 0)
        {
            if (Graph->DebugLabelStates.Data[Index])
            {
                RR_LOG_ERROR(
                    "Trying to begin label \"%s\" which is already active!",
                    Name);
            }
            Graph->DebugLabelStates.Data[Index] = true;
            return;
        }
    }

    size_t Length = strlen(Name);
    char *Copy = RR_ALLOC_NO_ZERO(Length + 1, Graph->Arena);
    memcpy(Copy, Name, Length + 1);

    *RR_PUSH_INTO_ARRAY(&Graph->DebugLabelNames, Graph->Arena) = Copy;
    *RR_PUSH_INTO_ARRAY(&Graph->DebugLabelStates, Graph->Arena) = true;
#endif
}

void Rr_EndGraphLabel(Rr_Graph *Graph, const char *Name)
{
#ifdef RR_USE_GPU_DEBUG_UTILS
    assert(Graph);
    assert(Name);

    for (size_t Index = 0; Index < Graph->DebugLabelNames.Count; ++Index)
    {
        const char *DebugLabel = Graph->DebugLabelNames.Data[Index];
        if (strcmp(DebugLabel, Name) == 0)
        {
            if (!Graph->DebugLabelStates.Data[Index])
            {
                RR_LOG_ERROR(
                    "Trying to end label \"%s\" which is already disabled!",
                    Name);
            }
            Graph->DebugLabelStates.Data[Index] = false;
            return;
        }
    }

    RR_LOG_ERROR("Trying to end label \"%s\" which has never been used!", Name);
#endif
}

Rr_TransferNode *Rr_AddTransferNode(Rr_Graph *Graph)
{
    Rr_GraphNode *GraphNode =
        Rr_AddGraphNode(Graph, RR_GRAPH_NODE_TYPE_TRANSFER);

    Rr_TransferNode *TransferNode = &GraphNode->Union.Transfer;
    RR_RESERVE_ARRAY(&TransferNode->Transfers, 2, Graph->Arena);

    return &GraphNode->Union.Transfer;
}

void Rr_TransferBufferData(
    Rr_TransferNode *TransferNode,
    uint64_t Size,
    Rr_Buffer *SrcBuffer,
    uint64_t SrcOffset,
    Rr_Buffer *DstBuffer,
    uint64_t DstOffset)
{
    Rr_GraphNode *Node = (void *)TransferNode;

    Rr_GraphBuffer *SrcBufferHandle =
        Rr_GetGraphBufferHandle(Node->Graph, SrcBuffer);
    Rr_GraphBuffer *DstBufferHandle =
        Rr_GetGraphBufferHandle(Node->Graph, DstBuffer);

    *RR_PUSH_INTO_ARRAY(&TransferNode->Transfers, Node->Graph->Arena) =
        (Rr_Transfer){
            .Size = Size,
            .SrcOffset = SrcOffset,
            .SrcBuffer = *SrcBufferHandle,
            .DstOffset = DstOffset,
            .DstBuffer = *DstBufferHandle,
        };

    Rr_AddBufferDependency(
        Node,
        SrcBufferHandle,
        &(Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .AccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        });

    Rr_AddNodeDependency(
        Node,
        &Node->BufferDeps,
        &Node->Graph->BufferWriteToNode,
        DstBufferHandle,
        &(Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .AccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        },
        true,
        false);
}

void Rr_BlitImage2D(
    Rr_Graph *Graph,
    Rr_Image2D *SrcImage,
    Rr_Image2D *DstImage,
    Rr_IntVec4 SrcRect,
    Rr_IntVec4 DstRect,
    Rr_ImageAspect ImageAspect)
{
    assert(
        (Graph->QueueType == RR_QUEUE_TYPE_MAIN) &&
        "This function requires a graph with graphics capabilities!");

    Rr_GraphNode *GraphNode = Rr_AddGraphNode(Graph, RR_GRAPH_NODE_TYPE_BLIT);

    Rr_GraphImage *SrcImageHandle = Rr_GetGraphImageHandle(Graph, SrcImage);
    Rr_GraphImage *DstImageHandle = Rr_GetGraphImageHandle(Graph, DstImage);

    Rr_BlitNode *BlitNode = &GraphNode->Union.Blit;
    *BlitNode = (Rr_BlitNode){
        .SrcImageHandle = *SrcImageHandle,
        .DstImageHandle = *DstImageHandle,
        .SrcRect = SrcRect,
        .DstRect = DstRect,
    };

    BlitNode->AspectMask = Rr_ToVulkanImageAspect(ImageAspect);

    Rr_AddImageDependency(
        GraphNode,
        SrcImageHandle,
        &(Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .AccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        });

    Rr_AddImageDependency(
        GraphNode,
        DstImageHandle,
        &(Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .AccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .Layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        });
}

void Rr_GenerateMipmaps(Rr_Graph *Graph, Rr_Image *Image)
{
    assert(
        (Graph->QueueType == RR_QUEUE_TYPE_MAIN) &&
        "This function requires a graph with graphics capabilities!");
    assert(Image->LevelCount > 1 && "This image doesn't support mipmaps!");

    Rr_GraphNode *GraphNode =
        Rr_AddGraphNode(Graph, RR_GRAPH_NODE_TYPE_GENERATE_MIPMAPS);

    Rr_GraphImage *ImageHandle = Rr_GetGraphImageHandle(Graph, Image);

    GraphNode->Union.GenerateMipmaps = *ImageHandle;

    Rr_AddImageDependency(
        GraphNode,
        ImageHandle,
        &(Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .AccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .Layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        });
}

Rr_GraphNode *Rr_AddComputeNode(Rr_Graph *Graph)
{
    assert(
        (Graph->QueueType == RR_QUEUE_TYPE_MAIN ||
         Graph->QueueType == RR_QUEUE_TYPE_ASYNC_COMPUTE) &&
        "This function requires a graph with compute capabilities!");

    Rr_GraphNode *GraphNode =
        Rr_AddGraphNode(Graph, RR_GRAPH_NODE_TYPE_COMPUTE);

    Rr_ComputeNode *ComputeNode = &GraphNode->Union.Compute;

    ComputeNode->Encoded.Encoded =
        RR_ALLOC(sizeof(Rr_NodeFunction), Graph->Arena);
    ComputeNode->Encoded.EncodedFirst = ComputeNode->Encoded.Encoded;

    return GraphNode;
}

Rr_GraphNode *Rr_AddGraphicsNode(
    Rr_Graph *Graph,
    size_t ColorTargetCount,
    Rr_ColorTarget *ColorTargets,
    Rr_DepthTarget *DepthTarget)
{
    assert(
        (Graph->QueueType == RR_QUEUE_TYPE_MAIN) &&
        "This function requires a graph with graphics capabilities!");
    assert(ColorTargetCount > 0 || DepthTarget != NULL);

    Rr_GraphNode *GraphNode =
        Rr_AddGraphNode(Graph, RR_GRAPH_NODE_TYPE_GRAPHICS);

    Rr_GraphicsNode *GraphicsNode = &GraphNode->Union.Graphics;
    if (ColorTargetCount > 0)
    {
        GraphicsNode->ColorTargetCount = (uint32_t)ColorTargetCount;
        GraphicsNode->ColorTargets = RR_ALLOC_COPY(
            ColorTargets,
            sizeof(Rr_ColorTarget) * ColorTargetCount,
            Graph->Arena);

        for (size_t Index = 0; Index < ColorTargetCount; ++Index)
        {
            Rr_ColorTarget *ColorTarget = &ColorTargets[Index];

            VkAccessFlags AccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            if (ColorTargets[Index].LoadOp == RR_LOAD_OP_LOAD)
            {
                AccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
            }
            Rr_AddImageDependency(
                GraphNode,
                Rr_GetGraphImageHandle(Graph, ColorTarget->Image),
                &(Rr_SyncState){
                    .StageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .AccessMask = AccessMask,
                    .Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                });

            if (ColorTarget->ResolveImage == NULL ||
                ColorTarget->Image->SampleCount == 1)
            {
                continue;
            }

            Rr_AddImageDependency(
                GraphNode,
                Rr_GetGraphImageHandle(Graph, ColorTarget->ResolveImage),
                &(Rr_SyncState){
                    .StageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .AccessMask = AccessMask,
                    .Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                });
        }
    }
    if (DepthTarget != NULL)
    {
        GraphicsNode->DepthTarget = RR_ALLOC_TYPE(Rr_DepthTarget, Graph->Arena);
        *GraphicsNode->DepthTarget = *DepthTarget;

        VkAccessFlags AccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        if (DepthTarget->LoadOp == RR_LOAD_OP_LOAD)
        {
            AccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        }
        Rr_AddImageDependency(
            GraphNode,
            Rr_GetGraphImageHandle(Graph, DepthTarget->Image),
            &(Rr_SyncState){
                .StageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                             VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                .AccessMask = AccessMask,
                .Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            });
    }

    GraphicsNode->Encoded.Encoded =
        RR_ALLOC(sizeof(Rr_NodeFunction), Graph->Arena);
    GraphicsNode->Encoded.EncodedFirst = GraphicsNode->Encoded.Encoded;

    return GraphNode;
}

void Rr_ClearColorImage2D(
    Rr_Graph *Graph,
    Rr_ColorClear ColorClear,
    Rr_Image2D *Image)
{
    assert(
        ((Graph->QueueType == RR_QUEUE_TYPE_MAIN) ||
         (Graph->QueueType == RR_QUEUE_TYPE_ASYNC_COMPUTE)) &&
        "This function requires a graph with graphics or compute "
        "capabilities!");
    assert(Image != NULL);

    Rr_GraphNode *GraphNode =
        Rr_AddGraphNode(Graph, RR_GRAPH_NODE_TYPE_CLEAR_COLOR_IMAGE);

    Rr_GraphImage *ColorImageHandle = Rr_GetGraphImageHandle(Graph, Image);
    Rr_AddImageDependency(
        GraphNode,
        ColorImageHandle,
        &(Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .AccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .Layout = VK_IMAGE_LAYOUT_GENERAL,
        });

    GraphNode->Union.ClearColorImage =
        (Rr_ClearColorImageNode){ .ColorClear = ColorClear,
                                  .ColorImage = *ColorImageHandle };
}

void Rr_ResolveImage2D(
    Rr_Graph *Graph,
    Rr_Image2D *SrcImage,
    uint32_t SrcImageLayerIndex,
    Rr_Image2D *DstImage,
    uint32_t DstImageLayerIndex,
    Rr_ImageAspect Aspect)
{
    assert(
        Graph->QueueType == RR_QUEUE_TYPE_MAIN &&
        "This function requires a graph with graphics capabilities!");
    assert(SrcImage != NULL && DstImage != NULL);

    Rr_GraphNode *GraphNode =
        Rr_AddGraphNode(Graph, RR_GRAPH_NODE_TYPE_RESOLVE_IMAGE);

    Rr_ResolveImageNode *ResolveImageNode = (Rr_ResolveImageNode *)GraphNode;
    ResolveImageNode->SrcImage = SrcImage;
    ResolveImageNode->SrcImageLayerIndex = SrcImageLayerIndex;
    ResolveImageNode->DstImage = DstImage;
    ResolveImageNode->DstImageLayerIndex = DstImageLayerIndex;
    ResolveImageNode->AspectFlags = Rr_ToVulkanImageAspect(Aspect);

    Rr_GraphImage *SrcImageHandle = Rr_GetGraphImageHandle(Graph, SrcImage);
    Rr_AddImageDependency(
        GraphNode,
        SrcImageHandle,
        &(Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .AccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        });

    Rr_GraphImage *DstImageHandle = Rr_GetGraphImageHandle(Graph, DstImage);
    Rr_AddImageDependency(
        GraphNode,
        DstImageHandle,
        &(Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .AccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .Layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        });
}

static inline Rr_GraphNode *Rr_AddCopyBufferToImageNodeEx(
    Rr_Graph *Graph,
    Rr_Buffer *Buffer,
    uint64_t BufferOffset,
    Rr_IntVec3 Extent,
    Rr_Image *Image,
    uint32_t BaseLayer,
    uint32_t LayerCount,
    uint32_t MipLevel)
{
    assert(Graph != NULL);
    assert(Buffer != NULL);
    assert(Image != NULL);

    Rr_GraphNode *GraphNode =
        Rr_AddGraphNode(Graph, RR_GRAPH_NODE_TYPE_COPY_BUFFER_TO_IMAGE);

    Rr_GraphBuffer *BufferHandle = Rr_GetGraphBufferHandle(Graph, Buffer);
    Rr_AddBufferDependency(
        GraphNode,
        BufferHandle,
        &(Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .AccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        });

    Rr_GraphImage *ImageHandle = Rr_GetGraphImageHandle(Graph, Image);
    Rr_AddImageDependency(
        GraphNode,
        ImageHandle,
        &(Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .AccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .Layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        });

    GraphNode->Union.CopyBufferToImage = (Rr_CopyBufferToImageNode){
        .Buffer = *BufferHandle,
        .BufferOffset = BufferOffset,
        .Image = *ImageHandle,
        .Extent =
            (VkExtent3D){
                (uint32_t)Extent.Width,
                (uint32_t)Extent.Height,
                (uint32_t)Extent.Depth,
            },
        .BaseLayer = BaseLayer,
        .LayerCount = LayerCount,
        .MipLevel = MipLevel,
    };

    return GraphNode;
}

void Rr_CopyBufferToImage2D(
    Rr_Graph *Graph,
    Rr_Buffer *Buffer,
    uint64_t BufferOffset,
    Rr_IntVec2 Extent,
    Rr_Image2D *Image,
    uint32_t MipLevel)
{
    Rr_AddCopyBufferToImageNodeEx(
        Graph,
        Buffer,
        BufferOffset,
        (Rr_IntVec3){ Extent.Width, Extent.Height, 1 },
        (Rr_Image *)Image,
        0,
        1,
        MipLevel);
}

void Rr_CopyBufferToImage2DArray(
    Rr_Graph *Graph,
    Rr_Buffer *Buffer,
    uint64_t BufferOffset,
    Rr_IntVec2 Extent,
    Rr_Image2DArray *Image2DArray,
    uint32_t ArrayIndex,
    uint32_t MipLevel)
{
    Rr_AddCopyBufferToImageNodeEx(
        Graph,
        Buffer,
        BufferOffset,
        (Rr_IntVec3){ Extent.Width, Extent.Height, 1 },
        (Rr_Image *)Image2DArray,
        ArrayIndex,
        1,
        MipLevel);
}

void Rr_CopyBufferToImage3D(
    Rr_Graph *Graph,
    Rr_Buffer *Buffer,
    uint64_t BufferOffset,
    Rr_IntVec3 Extent,
    Rr_Image3D *Image3D,
    uint32_t MipLevel)
{
    Rr_AddCopyBufferToImageNodeEx(
        Graph,
        Buffer,
        BufferOffset,
        (Rr_IntVec3){
            Extent.Width,
            Extent.Height,
            Extent.Depth,
        },
        (Rr_Image *)Image3D,
        0,
        1,
        MipLevel);
}

void Rr_CopyBufferToImageCube(
    Rr_Graph *Graph,
    Rr_Buffer *Buffer,
    uint64_t BufferOffset,
    Rr_IntVec2 Extent,
    Rr_ImageCube *ImageCube,
    Rr_ImageCubeFace Face,
    uint32_t MipLevel)
{
    assert(Face >= RR_IMAGE_CUBE_FACE_FIRST);
    assert(Face <= RR_IMAGE_CUBE_FACE_LAST);

    Rr_AddCopyBufferToImageNodeEx(
        Graph,
        Buffer,
        BufferOffset,
        (Rr_IntVec3){ Extent.Width, Extent.Height, 1 },
        (Rr_Image *)ImageCube,
        (uint32_t)Face,
        1,
        MipLevel);
}

void Rr_CopyBufferToImageCubeEx(
    Rr_Graph *Graph,
    Rr_Buffer *Buffer,
    uint64_t BufferOffset,
    Rr_IntVec2 Extent,
    Rr_ImageCube *ImageCube,
    Rr_ImageCubeFace FirstFace,
    Rr_ImageCubeFace LastFace,
    uint32_t MipLevel)
{
    assert(FirstFace <= LastFace);
    assert(FirstFace >= RR_IMAGE_CUBE_FACE_FIRST);
    assert(LastFace <= RR_IMAGE_CUBE_FACE_LAST);

    Rr_AddCopyBufferToImageNodeEx(
        Graph,
        Buffer,
        BufferOffset,
        (Rr_IntVec3){ Extent.Width, Extent.Height, 1 },
        (Rr_Image *)ImageCube,
        (uint32_t)FirstFace,
        1 + ((uint32_t)LastFace - (uint32_t)FirstFace),
        MipLevel);
}

static inline void Rr_AddCopyImageToBufferNodeEx(
    Rr_Graph *Graph,
    Rr_Image *Image,
    Rr_IntVec3 ImageOffset,
    Rr_IntVec3 ImageExtent,
    Rr_ImageAspect ImageAspect,
    uint32_t ImageMipLevel,
    uint32_t ImageArrayIndex,
    uint32_t ImageArrayCount,
    Rr_Buffer *Buffer,
    uint64_t BufferOffset)
{
    assert(Image);
    assert(Buffer);

    Rr_GraphNode *GraphNode =
        Rr_AddGraphNode(Graph, RR_GRAPH_NODE_TYPE_COPY_IMAGE_TO_BUFFER);

    Rr_GraphImage *ImageHandle = Rr_GetGraphImageHandle(Graph, Image);
    Rr_GraphBuffer *BufferHandle = Rr_GetGraphBufferHandle(Graph, Buffer);

    Rr_AddImageDependency(
        GraphNode,
        ImageHandle,
        &(Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .AccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        });

    Rr_AddBufferDependency(
        GraphNode,
        BufferHandle,
        &(Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .AccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        });

    VkBufferImageCopy *BufferImageCopy =
        RR_ALLOC_NO_ZERO(sizeof(VkBufferImageCopy), Graph->Arena);
    *BufferImageCopy = (VkBufferImageCopy){
        .bufferOffset = BufferOffset,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource =
            (VkImageSubresourceLayers){
                .aspectMask = Rr_ToVulkanImageAspect(ImageAspect),
                .mipLevel = ImageMipLevel,
                .baseArrayLayer = ImageArrayIndex,
                .layerCount = ImageArrayCount,
            },
        .imageOffset = Rr_ToVulkanOffset3D(ImageOffset),
        .imageExtent = Rr_ToVulkanExtent3D(ImageExtent),
    };

    GraphNode->Union.CopyImageToBuffer = (Rr_CopyImageToBufferNode){
        .Image = *ImageHandle,
        .Buffer = *BufferHandle,
        .BufferImageCopyCount = 1,
        .BufferImageCopies = BufferImageCopy,
    };
}

void Rr_CopyImage2DToBuffer(
    Rr_Graph *Graph,
    Rr_Image2D *Image,
    Rr_IntVec2 ImageOffset,
    Rr_IntVec2 ImageExtent,
    Rr_ImageAspect ImageAspect,
    uint32_t ImageMipLevel,
    Rr_Buffer *Buffer,
    uint64_t BufferOffset)
{
    Rr_AddCopyImageToBufferNodeEx(
        Graph,
        Image,
        (Rr_IntVec3){
            .X = ImageOffset.X,
            .Y = ImageOffset.Y,
            .Z = 0,
        },
        (Rr_IntVec3){
            .X = ImageExtent.X,
            .Y = ImageExtent.Y,
            .Z = 1,
        },
        ImageAspect,
        ImageMipLevel,
        0,
        1,
        Buffer,
        BufferOffset);
}

static inline Rr_GraphNode *Rr_AddCopyImageNode(
    Rr_Graph *Graph,
    Rr_Image *SrcImage,
    Rr_IntVec3 SrcOffset,
    Rr_Image *DstImage,
    Rr_IntVec3 DstOffset,
    Rr_IntVec3 Extent,
    uint32_t BaseLayer,
    uint32_t LayerCount,
    uint32_t MipLevel)
{
    Rr_GraphNode *GraphNode =
        Rr_AddGraphNode(Graph, RR_GRAPH_NODE_TYPE_COPY_IMAGE);

    Rr_GraphImage *SrcImageHandle = Rr_GetGraphImageHandle(Graph, SrcImage);
    Rr_GraphImage *DstImageHandle = Rr_GetGraphImageHandle(Graph, DstImage);

    Rr_AddImageDependency(
        GraphNode,
        SrcImageHandle,
        &(Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .AccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        });

    Rr_AddImageDependency(
        GraphNode,
        DstImageHandle,
        &(Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .AccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .Layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        });

    GraphNode->Union.CopyImage = (Rr_CopyImageNode){
        .SrcImage = *SrcImageHandle,
        .SrcOffset = SrcOffset,
        .DstImage = *DstImageHandle,
        .DstOffset = DstOffset,
        .Extent = Extent,
        .BaseLayer = BaseLayer,
        .LayerCount = LayerCount,
        .MipLevel = MipLevel,
    };

    return GraphNode;
}

void Rr_CopyImage2D(
    Rr_Graph *Graph,
    Rr_Image2D *SrcImage,
    Rr_IntVec2 SrcOffset,
    Rr_Image2D *DstImage,
    Rr_IntVec2 DstOffset,
    Rr_IntVec2 Extent,
    uint32_t MipLevel)
{
    assert(Graph);
    assert(SrcImage);
    assert(DstImage);
    assert(Extent.Width >= 1);
    assert(Extent.Height >= 1);

    Rr_AddCopyImageNode(
        Graph,
        (Rr_Image *)SrcImage,
        (Rr_IntVec3){ SrcOffset.X, SrcOffset.Y, 0 },
        (Rr_Image *)DstImage,
        (Rr_IntVec3){ DstOffset.X, DstOffset.Y, 0 },
        (Rr_IntVec3){ Extent.Width, Extent.Height, 1 },
        0,
        1,
        MipLevel);
}

void Rr_CopyImageCube(
    Rr_Graph *Graph,
    Rr_ImageCube *SrcImage,
    Rr_ImageCube *DstImage,
    uint32_t MipLevel)
{
    assert(Graph);
    assert(SrcImage);
    assert(DstImage);
    assert(
        memcmp(&SrcImage->Extent, &DstImage->Extent, sizeof(VkExtent3D)) == 0);

    Rr_AddCopyImageNode(
        Graph,
        (Rr_Image *)SrcImage,
        (Rr_IntVec3){ 0, 0, 0 },
        (Rr_Image *)DstImage,
        (Rr_IntVec3){ 0, 0, 0 },
        (Rr_IntVec3){
            (int32_t)SrcImage->Extent.width,
            (int32_t)SrcImage->Extent.height,
            1,
        },
        0,
        6,
        MipLevel);
}

#define RR_NODE_ENCODE(FunctionType, Struct)                               \
    do                                                                     \
    {                                                                      \
        Rr_Arena *Arena = Node->Graph->Arena;                              \
        Rr_Encoded *Encoded = (Rr_Encoded *)&Node->Union;                  \
        Encoded->Encoded->Next = RR_ALLOC(sizeof(Rr_NodeFunction), Arena); \
        Encoded->Encoded = Encoded->Encoded->Next;                         \
        Encoded->Encoded->Type = FunctionType;                             \
        Encoded->Encoded->Args = RR_ALLOC_NO_ZERO(sizeof(Struct), Arena);  \
        memcpy(Encoded->Encoded->Args, &(Struct), sizeof(Struct));         \
    }                                                                      \
    while (0)

void Rr_BindComputePipeline(
    Rr_GraphNode *Node,
    Rr_ComputePipeline *ComputePipeline)
{
    assert(ComputePipeline != NULL);
    assert(Node->Type == RR_GRAPH_NODE_TYPE_COMPUTE);

    RR_NODE_ENCODE( /* NOLINT */
        RR_NODE_FUNCTION_TYPE_BIND_COMPUTE_PIPELINE,
        ComputePipeline);

    Node->CurrentLayout = ComputePipeline->Layout;

    Rr_MarkComputePipelineUsed(Node->Graph, ComputePipeline);
}

void Rr_Dispatch(
    Rr_GraphNode *Node,
    uint32_t GroupCountX,
    uint32_t GroupCountY,
    uint32_t GroupCountZ)
{
    assert(Node->Type == RR_GRAPH_NODE_TYPE_COMPUTE);
    assert(GroupCountX >= 1);
    assert(GroupCountY >= 1);
    assert(GroupCountZ >= 1);

    RR_NODE_ENCODE(
        RR_NODE_FUNCTION_TYPE_DISPATCH,
        ((Rr_DispatchArgs){
            .GroupCountX = GroupCountX,
            .GroupCountY = GroupCountY,
            .GroupCountZ = GroupCountZ,
        }));
}

void Rr_ComputeBarrier(Rr_GraphNode *Node)
{
    assert(Node->Type == RR_GRAPH_NODE_TYPE_COMPUTE);

    RR_NODE_ENCODE(RR_NODE_FUNCTION_TYPE_COMPUTE_BARRIER, ((uint32_t){ 0 }));
}

void Rr_Draw(
    Rr_GraphNode *Node,
    uint32_t VertexCount,
    uint32_t InstanceCount,
    uint32_t FirstVertex,
    uint32_t FirstInstance)
{
    assert(Node->Type == RR_GRAPH_NODE_TYPE_GRAPHICS);

    RR_NODE_ENCODE(
        RR_NODE_FUNCTION_TYPE_DRAW,
        ((Rr_DrawArgs){
            .VertexCount = VertexCount,
            .InstanceCount = InstanceCount,
            .FirstVertex = FirstVertex,
            .FirstInstance = FirstInstance,
        }));
}

static inline void Rr_DrawIndirectEx(
    Rr_GraphNode *Node,
    Rr_NodeFunctionType Type,
    Rr_Buffer *Buffer,
    uint64_t Offset,
    uint32_t Count,
    uint32_t Stride)
{
    assert(Node->Type == RR_GRAPH_NODE_TYPE_GRAPHICS);

    Rr_GraphBuffer *BufferHandle = Rr_GetGraphBufferHandle(Node->Graph, Buffer);

    RR_NODE_ENCODE(
        Type,
        ((Rr_DrawIndirectArgs){
            .BufferHandle = *BufferHandle,
            .Offset = Offset,
            .Count = Count,
            .Stride = Stride,
        }));

    Rr_AddBufferDependency(
        Node,
        BufferHandle,
        &(Rr_SyncState){
            .AccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
            .StageMask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        });
}

void Rr_DrawIndirect(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint64_t Offset,
    uint32_t Count,
    uint32_t Stride)
{
    Rr_DrawIndirectEx(
        Node,
        RR_NODE_FUNCTION_TYPE_DRAW_INDIRECT,
        Buffer,
        Offset,
        Count,
        Stride);
}

void Rr_DrawIndexed(
    Rr_GraphNode *Node,
    uint32_t IndexCount,
    uint32_t InstanceCount,
    uint32_t FirstIndex,
    int32_t VertexOffset,
    uint32_t FirstInstance)
{
    assert(Node->Type == RR_GRAPH_NODE_TYPE_GRAPHICS);

    RR_NODE_ENCODE(
        RR_NODE_FUNCTION_TYPE_DRAW_INDEXED,
        ((Rr_DrawIndexedArgs){
            .IndexCount = IndexCount,
            .InstanceCount = InstanceCount,
            .FirstIndex = FirstIndex,
            .VertexOffset = VertexOffset,
            .FirstInstance = FirstInstance,
        }));
}

void Rr_DrawIndexedIndirect(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint64_t Offset,
    uint32_t Count,
    uint32_t Stride)
{
    Rr_DrawIndirectEx(
        Node,
        RR_NODE_FUNCTION_TYPE_DRAW_INDEXED_INDIRECT,
        Buffer,
        Offset,
        Count,
        Stride);
}

void Rr_BindVertexBuffer(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Slot,
    uint64_t Offset)
{
    assert(Node->Type == RR_GRAPH_NODE_TYPE_GRAPHICS);

    Rr_GraphBuffer *BufferHandle = Rr_GetGraphBufferHandle(Node->Graph, Buffer);

    RR_NODE_ENCODE(
        RR_NODE_FUNCTION_TYPE_BIND_VERTEX_BUFFER,
        ((Rr_BindIndexBufferArgs){
            .BufferHandle = *BufferHandle,
            .Slot = Slot,
            .Offset = Offset,
        }));

    Rr_AddBufferDependency(
        Node,
        BufferHandle,
        &(Rr_SyncState){
            .AccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
            .StageMask = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        });
}

void Rr_BindIndexBuffer(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Slot,
    uint64_t Offset,
    Rr_IndexType Type)
{
    assert(Node->Type == RR_GRAPH_NODE_TYPE_GRAPHICS);

    Rr_GraphBuffer *BufferHandle = Rr_GetGraphBufferHandle(Node->Graph, Buffer);

    RR_NODE_ENCODE(
        RR_NODE_FUNCTION_TYPE_BIND_INDEX_BUFFER,
        ((Rr_BindIndexBufferArgs){
            .BufferHandle = *BufferHandle,
            .Slot = Slot,
            .Offset = Offset,
            .Type = Rr_ToVulkanIndexType(Type),
        }));

    Rr_AddBufferDependency(
        Node,
        BufferHandle,
        &(Rr_SyncState){
            .AccessMask = VK_ACCESS_INDEX_READ_BIT,
            .StageMask = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        });
}

void Rr_BindGraphicsPipeline(
    Rr_GraphNode *Node,
    Rr_GraphicsPipeline *GraphicsPipeline)
{
    assert(Node->Type == RR_GRAPH_NODE_TYPE_GRAPHICS);
    assert(
        Node->Union.Graphics.ColorTargetCount ==
        GraphicsPipeline->ColorAttachmentCount);
    assert(
        Node->Union.Graphics.DepthTarget != NULL ||
        !GraphicsPipeline->HasDepthStencil);

    RR_NODE_ENCODE( /* NOLINT */
        RR_NODE_FUNCTION_TYPE_BIND_GRAPHICS_PIPELINE,
        ((Rr_GraphicsPipeline *){ GraphicsPipeline }));

    Node->CurrentLayout = GraphicsPipeline->Layout;

    Rr_MarkGraphicsPipelineUsed(Node->Graph, GraphicsPipeline);
}

void Rr_SetViewport(Rr_GraphNode *Node, Rr_Rect *Rect)
{
    assert(Node->Type == RR_GRAPH_NODE_TYPE_GRAPHICS);

    RR_NODE_ENCODE(RR_NODE_FUNCTION_TYPE_SET_VIEWPORT, *Rect);
}

void Rr_SetScissor(Rr_GraphNode *Node, Rr_IntRect *Rect)
{
    assert(Node->Type == RR_GRAPH_NODE_TYPE_GRAPHICS);

    RR_NODE_ENCODE(RR_NODE_FUNCTION_TYPE_SET_SCISSOR, *Rect);
}

void Rr_BindSampler(
    Rr_GraphNode *Node,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding)
{
    Rr_BindSamplerAt(Node, Sampler, Set, Binding, 0);
}

void Rr_BindSamplerAt(
    Rr_GraphNode *Node,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex)
{
    assert(Set < RR_MAX_SETS);
    assert(Binding < RR_MAX_BINDINGS);
    assert(Sampler != NULL);
    assert(Node->CurrentLayout);

    RR_NODE_ENCODE(
        RR_NODE_FUNCTION_TYPE_BIND_SAMPLER,
        ((Rr_BindSamplerArgs){
            .Sampler = Sampler,
            .Set = (uint32_t)Set,
            .Binding = (uint32_t)Binding,
            .ArrayIndex = ArrayIndex,
        }));

    Rr_MarkSamplerUsed(Node->Graph, Sampler);
}

static inline VkPipelineStageFlags Rr_GetVulkanPipelineStageMaskForSetBinding(
    Rr_GraphNode *Node,
    uint32_t SetIndex,
    uint32_t BindingIndex)
{
    VkPipelineStageFlags StageMask = 0;
    VkShaderStageFlags ShaderMask = 0;
    Rr_DescriptorSetLayoutKey const *Key =
        &Node->CurrentLayout->Key.DescriptorSetLayouts[SetIndex]->Key;
    for (uint32_t Index = 0; Index < Key->BindingCount; ++Index)
    {
        Rr_VulkanBinding const *VulkanBinding = &Key->Bindings[Index];
        if (VulkanBinding->Index == BindingIndex)
        {
            ShaderMask = VulkanBinding->Stages;
            break;
        }
    }

    if (ShaderMask & VK_SHADER_STAGE_COMPUTE_BIT)
    {
        StageMask |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    else
    {
        if (ShaderMask & VK_SHADER_STAGE_VERTEX_BIT)
        {
            StageMask |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        }
        if (ShaderMask & VK_SHADER_STAGE_FRAGMENT_BIT)
        {
            StageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
    }

    return StageMask;
}

static void Rr_BindSampledImageEx(
    Rr_GraphNode *Node,
    Rr_Image *Image,
    VkImageViewType ViewType,
    VkImageSubresourceRange *SubresourceRange,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex)
{
    assert(Set < RR_MAX_SETS);
    assert(Binding < RR_MAX_BINDINGS);
    assert(Image);
    assert(Node->CurrentLayout);

    Rr_GraphImage *ImageHandle = Rr_GetGraphImageHandle(Node->Graph, Image);

    RR_NODE_ENCODE(
        RR_NODE_FUNCTION_TYPE_BIND_SAMPLED_IMAGE,
        ((Rr_BindSampledImageArgs){
            .ImageHandle = *ImageHandle,
            .Set = (uint32_t)Set,
            .Binding = (uint32_t)Binding,
            .ArrayIndex = (uint32_t)ArrayIndex,
            .ViewType = ViewType,
            .SubresourceRange = *SubresourceRange,
        }));

    Rr_AddImageDependency(
        Node,
        ImageHandle,
        &(Rr_SyncState){
            .AccessMask = VK_ACCESS_SHADER_READ_BIT,
            .StageMask =
                Rr_GetVulkanPipelineStageMaskForSetBinding(Node, Set, Binding),
            .Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        });
}

void Rr_BindSampledImage2D(
    Rr_GraphNode *Node,
    Rr_Image2D *Image2D,
    uint32_t Set,
    uint32_t Binding)
{
    Rr_BindSampledImage2DAt(Node, Image2D, Set, Binding, 0);
}

void Rr_BindSampledImage2DAt(
    Rr_GraphNode *Node,
    Rr_Image2D *Image2D,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex)
{
    Rr_BindSampledImageEx(
        Node,
        Image2D,
        VK_IMAGE_VIEW_TYPE_2D,
        &(VkImageSubresourceRange){
            .aspectMask = Image2D->AspectFlags,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        Set,
        Binding,
        ArrayIndex);
}

void Rr_BindSampledImage2DArray(
    Rr_GraphNode *Node,
    Rr_Image2DArray *Image2DArray,
    uint32_t Set,
    uint32_t Binding)
{

    Rr_BindSampledImage2DArrayAt(Node, Image2DArray, Set, Binding, 0);
}

void Rr_BindSampledImage2DArrayAt(
    Rr_GraphNode *Node,
    Rr_Image2DArray *Image2DArray,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex)
{
    Rr_BindSampledImageEx(
        Node,
        Image2DArray,
        VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        &(VkImageSubresourceRange){
            .aspectMask = Image2DArray->AspectFlags,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        },
        Set,
        Binding,
        ArrayIndex);
}

void Rr_BindSampledImage3D(
    Rr_GraphNode *Node,
    Rr_Image3D *Image3D,
    uint32_t Set,
    uint32_t Binding)
{
    Rr_BindSampledImage3DAt(Node, Image3D, Set, Binding, 0);
}

void Rr_BindSampledImage3DAt(
    Rr_GraphNode *Node,
    Rr_Image3D *Image3D,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex)
{
    Rr_BindSampledImageEx(
        Node,
        Image3D,
        VK_IMAGE_VIEW_TYPE_3D,
        &(VkImageSubresourceRange){
            .aspectMask = Image3D->AspectFlags,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        Set,
        Binding,
        ArrayIndex);
}

void Rr_BindSampledImageCube(
    Rr_GraphNode *Node,
    Rr_ImageCube *ImageCube,
    uint32_t Set,
    uint32_t Binding)
{
    Rr_BindSampledImageCubeAt(Node, ImageCube, Set, Binding, 0);
}

void Rr_BindSampledImageCubeAt(
    Rr_GraphNode *Node,
    Rr_ImageCube *ImageCube,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex)
{
    Rr_BindSampledImageEx(
        Node,
        ImageCube,
        VK_IMAGE_VIEW_TYPE_CUBE,
        &(VkImageSubresourceRange){
            .aspectMask = ImageCube->AspectFlags,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = 6,
        },
        Set,
        Binding,
        ArrayIndex);
}

static void Rr_BindCombinedImageSamplerEx(
    Rr_GraphNode *Node,
    Rr_Image *Image,
    Rr_Sampler *Sampler,
    VkImageViewType ViewType,
    VkImageSubresourceRange *SubresourceRange,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex)
{
    assert(Set < RR_MAX_SETS);
    assert(Binding < RR_MAX_BINDINGS);
    assert(Sampler != NULL);
    assert(Image);
    assert(Node->CurrentLayout);

    Rr_GraphImage *ImageHandle = Rr_GetGraphImageHandle(Node->Graph, Image);

    RR_NODE_ENCODE(
        RR_NODE_FUNCTION_TYPE_BIND_COMBINED_IMAGE_SAMPLER,
        ((Rr_BindCombinedImageSamplerArgs){
            .ImageHandle = *ImageHandle,
            .Sampler = Sampler,
            .ViewType = ViewType,
            .SubresourceRange = *SubresourceRange,
            .Set = (uint32_t)Set,
            .Binding = (uint32_t)Binding,
            .ArrayIndex = ArrayIndex,
        }));

    Rr_AddImageDependency(
        Node,
        ImageHandle,
        &(Rr_SyncState){
            .AccessMask = VK_ACCESS_SHADER_READ_BIT,
            .StageMask =
                Rr_GetVulkanPipelineStageMaskForSetBinding(Node, Set, Binding),
            .Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        });

    Rr_MarkSamplerUsed(Node->Graph, Sampler);
}

void Rr_BindCombinedImage2DSampler(
    Rr_GraphNode *Node,
    Rr_Image2D *Image2D,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding)
{
    Rr_BindCombinedImage2DSamplerAt(Node, Image2D, Sampler, Set, Binding, 0);
}

void Rr_BindCombinedImage2DSamplerAt(
    Rr_GraphNode *Node,
    Rr_Image2D *Image2D,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex)
{
    Rr_BindCombinedImageSamplerEx(
        Node,
        Image2D,
        Sampler,
        VK_IMAGE_VIEW_TYPE_2D,
        &(VkImageSubresourceRange){
            .aspectMask = Image2D->AspectFlags,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        Set,
        Binding,
        ArrayIndex);
}

void Rr_BindCombinedImage2DArraySampler(
    Rr_GraphNode *Node,
    Rr_Image2DArray *Image2DArray,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding)
{
    Rr_BindCombinedImage2DArraySamplerAt(
        Node,
        Image2DArray,
        Sampler,
        Set,
        Binding,
        0);
}

void Rr_BindCombinedImage2DArraySamplerAt(
    Rr_GraphNode *Node,
    Rr_Image2DArray *Image2DArray,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex)
{
    Rr_BindCombinedImageSamplerEx(
        Node,
        Image2DArray,
        Sampler,
        VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        &(VkImageSubresourceRange){
            .aspectMask = Image2DArray->AspectFlags,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        },
        Set,
        Binding,
        ArrayIndex);
}

void Rr_BindCombinedImage3DSampler(
    Rr_GraphNode *Node,
    Rr_Image3D *Image3D,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding)
{
    Rr_BindCombinedImage3DSamplerAt(Node, Image3D, Sampler, Set, Binding, 0);
}

void Rr_BindCombinedImage3DSamplerAt(
    Rr_GraphNode *Node,
    Rr_Image3D *Image3D,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex)
{
    Rr_BindCombinedImageSamplerEx(
        Node,
        Image3D,
        Sampler,
        VK_IMAGE_VIEW_TYPE_3D,
        &(VkImageSubresourceRange){
            .aspectMask = Image3D->AspectFlags,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        },
        Set,
        Binding,
        ArrayIndex);
}

void Rr_BindCombinedImageCubeSampler(
    Rr_GraphNode *Node,
    Rr_ImageCube *ImageCube,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding)
{
    Rr_BindCombinedImageCubeSamplerAt(
        Node,
        ImageCube,
        Sampler,
        Set,
        Binding,
        0);
}

void Rr_BindCombinedImageCubeSamplerAt(
    Rr_GraphNode *Node,
    Rr_ImageCube *ImageCube,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex)
{
    Rr_BindCombinedImageSamplerEx(
        Node,
        ImageCube,
        Sampler,
        VK_IMAGE_VIEW_TYPE_CUBE,
        &(VkImageSubresourceRange){
            .aspectMask = ImageCube->AspectFlags,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = 6,
        },
        Set,
        Binding,
        ArrayIndex);
}

void Rr_BindUniformBuffer(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Set,
    uint32_t Binding,
    uint64_t Offset,
    uint64_t Size)
{
    Rr_BindUniformBufferAt(Node, Buffer, Set, Binding, 0, Offset, Size);
}

void Rr_BindUniformBufferAt(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex,
    uint64_t Offset,
    uint64_t Size)
{
    assert(Set < RR_MAX_SETS);
    assert(Binding < RR_MAX_BINDINGS);
    assert(Size > 0);
    assert(Size <= Rr_GetBufferSize(Buffer));
    assert(Node->CurrentLayout);

    Rr_GraphBuffer *BufferHandle = Rr_GetGraphBufferHandle(Node->Graph, Buffer);

    RR_NODE_ENCODE(
        RR_NODE_FUNCTION_TYPE_BIND_UNIFORM_BUFFER,
        ((Rr_BindUniformBufferArgs){
            .BufferHandle = *BufferHandle,
            .Size = Size,
            .Offset = Offset,
            .Set = Set,
            .Binding = Binding,
            .ArrayIndex = ArrayIndex,
        }));

    Rr_AddBufferDependency(
        Node,
        BufferHandle,
        &(Rr_SyncState){
            .AccessMask = VK_ACCESS_UNIFORM_READ_BIT,
            .StageMask =
                Rr_GetVulkanPipelineStageMaskForSetBinding(Node, Set, Binding),
        });
}

static void Rr_BindStorageBufferEx(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex,
    bool ReadWrite,
    uint64_t Offset,
    uint64_t Size)
{
    assert(Set < RR_MAX_SETS);
    assert(Binding < RR_MAX_BINDINGS);
    assert(Size > 0);
    assert(Size <= Rr_GetBufferSize(Buffer));
    assert(Node->CurrentLayout);

    Rr_GraphBuffer *BufferHandle = Rr_GetGraphBufferHandle(Node->Graph, Buffer);

    RR_NODE_ENCODE(
        RR_NODE_FUNCTION_TYPE_BIND_STORAGE_BUFFER,
        ((Rr_BindStorageBufferArgs){
            .BufferHandle = *BufferHandle,
            .Size = Size,
            .Offset = Offset,
            .Set = Set,
            .Binding = Binding,
            .ArrayIndex = ArrayIndex,
        }));

    VkAccessFlags AccessMask = VK_ACCESS_SHADER_READ_BIT;
    if (ReadWrite)
    {
        AccessMask |= VK_ACCESS_SHADER_WRITE_BIT;
    }

    Rr_AddStorageBufferDependency(
        Node,
        BufferHandle,
        &(Rr_SyncState){
            .AccessMask = AccessMask,
            .StageMask =
                Rr_GetVulkanPipelineStageMaskForSetBinding(Node, Set, Binding),
        });
}

void Rr_BindStorageBuffer(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Set,
    uint32_t Binding,
    uint64_t Offset,
    uint64_t Size)
{
    Rr_BindStorageBufferEx(Node, Buffer, Set, Binding, 0, false, Offset, Size);
}

void Rr_BindStorageBufferAt(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex,
    uint64_t Offset,
    uint64_t Size)
{
    Rr_BindStorageBufferEx(
        Node,
        Buffer,
        Set,
        Binding,
        ArrayIndex,
        false,
        Offset,
        Size);
}

void Rr_BindStorageBufferRW(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Set,
    uint32_t Binding,
    uint64_t Offset,
    uint64_t Size)
{
    Rr_BindStorageBufferEx(Node, Buffer, Set, Binding, 0, true, Offset, Size);
}

void Rr_BindStorageBufferRWAt(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex,
    uint64_t Offset,
    uint64_t Size)
{
    Rr_BindStorageBufferEx(
        Node,
        Buffer,
        Set,
        Binding,
        ArrayIndex,
        true,
        Offset,
        Size);
}

static void Rr_BindStorageImageEx(
    Rr_GraphNode *Node,
    Rr_Image *Image,
    VkImageViewType ViewType,
    uint32_t LayerCount,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex,
    bool ReadWrite)
{
    assert(Set < RR_MAX_SETS);
    assert(Binding < RR_MAX_BINDINGS);
    assert(Node->CurrentLayout);

    Rr_GraphImage *ImageHandle = Rr_GetGraphImageHandle(Node->Graph, Image);

    RR_NODE_ENCODE(
        RR_NODE_FUNCTION_TYPE_BIND_STORAGE_IMAGE,
        ((Rr_BindStorageImageArgs){
            .ImageHandle = *ImageHandle,
            .Set = (uint32_t)Set,
            .Binding = (uint32_t)Binding,
            .ArrayIndex = (uint32_t)ArrayIndex,
            .ViewType = ViewType,
            .SubresourceRange =
                (VkImageSubresourceRange){
                    .aspectMask = Image->AspectFlags,
                    .baseMipLevel = 0,
                    .levelCount = VK_REMAINING_MIP_LEVELS,
                    .baseArrayLayer = 0,
                    .layerCount = LayerCount,
                },
        }));

    VkAccessFlags AccessMask = VK_ACCESS_SHADER_READ_BIT;
    if (ReadWrite)
    {
        AccessMask |= VK_ACCESS_SHADER_WRITE_BIT;
    }

    Rr_AddStorageImageDependency(
        Node,
        ImageHandle,
        &(Rr_SyncState){
            .AccessMask = AccessMask,
            .StageMask =
                Rr_GetVulkanPipelineStageMaskForSetBinding(Node, Set, Binding),
            .Layout = VK_IMAGE_LAYOUT_GENERAL,
        });
}

void Rr_BindStorageImage2D(
    Rr_GraphNode *Node,
    Rr_Image2D *Image2D,
    uint32_t Set,
    uint32_t Binding)
{
    Rr_BindStorageImageEx(
        Node,
        Image2D,
        VK_IMAGE_VIEW_TYPE_2D,
        1,
        Set,
        Binding,
        0,
        false);
}

void Rr_BindStorageImage2DAt(
    Rr_GraphNode *Node,
    Rr_Image2D *Image2D,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex)
{
    Rr_BindStorageImageEx(
        Node,
        Image2D,
        VK_IMAGE_VIEW_TYPE_2D,
        1,
        Set,
        Binding,
        ArrayIndex,
        false);
}

void Rr_BindStorageImage2DRW(
    Rr_GraphNode *Node,
    Rr_Image2D *Image2D,
    uint32_t Set,
    uint32_t Binding)
{
    Rr_BindStorageImageEx(
        Node,
        Image2D,
        VK_IMAGE_VIEW_TYPE_2D,
        1,
        Set,
        Binding,
        0,
        true);
}

void Rr_BindStorageImage2DRWAt(
    Rr_GraphNode *Node,
    Rr_Image2D *Image2D,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex)
{
    Rr_BindStorageImageEx(
        Node,
        Image2D,
        VK_IMAGE_VIEW_TYPE_2D,
        1,
        Set,
        Binding,
        ArrayIndex,
        true);
}

void Rr_BindStorageImage2DArray(
    Rr_GraphNode *Node,
    Rr_Image2DArray *Image2DArray,
    uint32_t Set,
    uint32_t Binding)
{
    Rr_BindStorageImageEx(
        Node,
        Image2DArray,
        VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        VK_REMAINING_ARRAY_LAYERS,
        Set,
        Binding,
        0,
        false);
}

void Rr_BindStorageImage2DArrayAt(
    Rr_GraphNode *Node,
    Rr_Image2DArray *Image2DArray,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex)
{
    Rr_BindStorageImageEx(
        Node,
        Image2DArray,
        VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        VK_REMAINING_ARRAY_LAYERS,
        Set,
        Binding,
        ArrayIndex,
        false);
}

void Rr_BindStorageImage2DArrayRW(
    Rr_GraphNode *Node,
    Rr_Image2DArray *Image2DArray,
    uint32_t Set,
    uint32_t Binding)
{
    Rr_BindStorageImageEx(
        Node,
        Image2DArray,
        VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        VK_REMAINING_ARRAY_LAYERS,
        Set,
        Binding,
        0,
        true);
}

void Rr_BindStorageImage2DArrayRWAt(
    Rr_GraphNode *Node,
    Rr_Image2DArray *Image2DArray,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex)
{
    Rr_BindStorageImageEx(
        Node,
        Image2DArray,
        VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        VK_REMAINING_ARRAY_LAYERS,
        Set,
        Binding,
        ArrayIndex,
        true);
}

void Rr_BeginNodeLabel(Rr_GraphNode *Node, const char *Name)
{
#ifdef RR_USE_GPU_DEBUG_UTILS
    assert(Node);

    size_t Length = strlen(Name) + 1;
    char *NameBuffer = RR_ALLOC_NO_ZERO(Length, Node->Graph->Arena);
    memcpy(NameBuffer, Name, Length);

    RR_NODE_ENCODE(RR_NODE_FUNCTION_TYPE_DEBUG_LABEL, NameBuffer);
#endif
}

void Rr_EndNodeLabel(Rr_GraphNode *Node)
{
#ifdef RR_USE_GPU_DEBUG_UTILS
    assert(Node);

    char *NameBuffer = NULL;

    RR_NODE_ENCODE(RR_NODE_FUNCTION_TYPE_DEBUG_LABEL, NameBuffer);
#endif
}
