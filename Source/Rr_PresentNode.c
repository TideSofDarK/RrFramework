#include "Rr_PresentNode.h"

#include "Rr_App.h"

Rr_GraphNode *Rr_AddPresentNode(
    Rr_App *App,
    const char *Name,
    Rr_PresentMode Mode)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_GraphNode *GraphNode = Rr_AddGraphNode(Frame, RR_GRAPH_NODE_TYPE_PRESENT, Name);

    Rr_PresentNode *PresentNode = &GraphNode->Union.PresentNode;
    PresentNode->Mode = Mode;

    return GraphNode;
}

bool Rr_BatchPresentNode(Rr_App *App, Rr_GraphBatch *Batch, Rr_PresentNode *Node)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    VkImage Handle = Rr_GetCurrentAllocatedImage(App, &Frame->SwapchainImage)->Handle;

    if(Rr_BatchImagePossible(&Batch->LocalSync, Handle) != true)
    {
        return false;
    }

    Rr_BatchImage(
        App,
        Batch,
        Handle,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    return true;
}

void Rr_ExecutePresentNode(Rr_App *App, Rr_PresentNode *Node)
{
}
