#include "Rr_PresentNode.h"

#include "Rr_App.h"

Rr_GraphNode *Rr_AddPresentNode(
    Rr_App *App,
    const char *Name,
    Rr_Image *Image,
    Rr_PresentMode Mode)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_GraphNode *GraphNode = Rr_AddGraphNode(Frame, RR_GRAPH_NODE_TYPE_PRESENT, Name);

    Rr_PresentNode *PresentNode = &GraphNode->Union.PresentNode;
    PresentNode->Mode = Mode;
    PresentNode->Image = Rr_GetCurrentAllocatedImage(App, Image);

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
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    VkFramebuffer Framebuffer = Renderer->Swapchain.Framebuffers.Data[Frame->SwapchainImageIndex];

    VkRenderPassBeginInfo RenderPassBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = NULL,
        .framebuffer = Framebuffer,
        .renderArea = (VkRect2D){ { 0, 0 }, { Renderer->Swapchain.Extent.width, Renderer->Swapchain.Extent.height } },
        .renderPass = Renderer->PresentRenderPass,
        .clearValueCount = 0,
        .pClearValues = NULL,
    };
    vkCmdBeginRenderPass(Frame->PresentCommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(Frame->PresentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Renderer->PresentPipeline->Handle);
    vkCmdDraw(Frame->PresentCommandBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(Frame->PresentCommandBuffer);
}
