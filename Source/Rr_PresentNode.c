#include "Rr_PresentNode.h"

#include "Rr_App.h"

Rr_GraphNode *Rr_AddPresentNode(
    Rr_App *App,
    const char *Name,
    Rr_PresentNodeInfo *Info,
    Rr_GraphNode **Dependencies,
    size_t DependencyCount)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_GraphNode *GraphNode = Rr_AddGraphNode(Frame, RR_GRAPH_NODE_TYPE_PRESENT, Name, Dependencies, DependencyCount);

    Rr_PresentNode *PresentNode = &GraphNode->Union.PresentNode;
    *PresentNode = (Rr_PresentNode){
        .Info = *Info,
    };

    return GraphNode;
}

bool Rr_BatchPresentNode(Rr_App *App, Rr_Graph *Graph, Rr_GraphBatch *Batch, Rr_PresentNode *Node)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    return Rr_SyncImage(
        App,
        Graph,
        Batch,
        Frame->SwapchainImage->Handle,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

void Rr_ExecutePresentNode(Rr_App *App, Rr_PresentNode *Node)
{
    // Rr_Renderer *Renderer = &App->Renderer;
    // Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    // VkCommandBuffer CommandBuffer = Frame->MainCommandBuffer;

    // Rr_ImageBarrier SwapchainImageTransition = {
    //     .CommandBuffer = CommandBuffer,
    //     .Image = Frame->CurrentSwapchainImage->Handle,
    //     .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
    //     .AccessMask = VK_ACCESS_NONE,
    //     .StageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    // };
    //
    // Rr_ChainImageBarrier(
    //     &SwapchainImageTransition,
    //     VK_PIPELINE_STAGE_TRANSFER_BIT,
    //     VK_ACCESS_TRANSFER_WRITE_BIT,
    //     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    //
    //
    // Rr_ChainImageBarrier(
    //     &SwapchainImageTransition,
    //     VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
    //     0,
    //     VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}
