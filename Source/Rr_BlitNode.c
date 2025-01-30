#include "Rr_BlitNode.h"

#include "Rr_App.h"
#include "Rr_Graph.h"
#include "Rr_Log.h"
#include "Rr_Renderer.h"

Rr_GraphNode *Rr_AddBlitNode(
    Rr_App *App,
    const char *Name,
    Rr_Image *SrcImage,
    Rr_Image *DstImage,
    Rr_IntVec4 SrcRect,
    Rr_IntVec4 DstRect,
    Rr_BlitMode Mode,
    Rr_GraphNode **Dependencies,
    size_t DependencyCount)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_GraphNode *GraphNode = Rr_AddGraphNode(Frame, RR_GRAPH_NODE_TYPE_BLIT, Name, Dependencies, DependencyCount);

    Rr_BlitNode *BlitNode = &GraphNode->Union.BlitNode;
    *BlitNode = (Rr_BlitNode){
        .SrcImage = Rr_GetCurrentAllocatedImage(App, SrcImage),
        .DstImage = Rr_GetCurrentAllocatedImage(App, DstImage),
        .SrcRect = SrcRect,
        .DstRect = DstRect,
    };

    switch(Mode)
    {
        case RR_BLIT_MODE_COLOR:
        {
            BlitNode->AspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }
        break;
        case RR_BLIT_MODE_DEPTH:
        {
            BlitNode->AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        }
        break;
        default:
        {
            Rr_LogAbort("Unsupported blit mode!");
        }
        break;
    }

    return GraphNode;
}

bool Rr_BatchBlitNode(Rr_App *App, Rr_Graph *Graph, Rr_GraphBatch *Batch, Rr_BlitNode *Node)
{
    if(Rr_BatchImagePossible(&Batch->LocalSync, Node->SrcImage->Handle) != true ||
       Rr_BatchImagePossible(&Batch->LocalSync, Node->DstImage->Handle) != true)
    {
        return false;
    }

    Rr_BatchImage(
        App,
        Batch,
        Node->SrcImage->Handle,
        Node->AspectMask,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    Rr_BatchImage(
        App,
        Batch,
        Node->DstImage->Handle,
        Node->AspectMask,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    return true;
}

static inline bool Rr_ClampBlitRect(Rr_IntVec4 *Rect, VkExtent3D *Extent)
{
    Rect->X = RR_CLAMP(0, Rect->X, (int)Extent->width);
    Rect->Y = RR_CLAMP(0, Rect->Y, (int)Extent->height);
    Rect->Width = RR_CLAMP(1, Rect->Width, (int)Extent->width - Rect->X);
    Rect->Height = RR_CLAMP(1, Rect->Height, (int)Extent->height - Rect->Y);

    return Rect->Width > 0 && Rect->Height > 0;
}

void Rr_ExecuteBlitNode(Rr_App *App, Rr_BlitNode *Node)
{
    Rr_Frame *Frame = Rr_GetCurrentFrame(&App->Renderer);

    if(Rr_ClampBlitRect(&Node->SrcRect, &Node->SrcImage->Container->Extent) &&
       Rr_ClampBlitRect(&Node->DstRect, &Node->DstImage->Container->Extent))
    {
        Rr_BlitColorImage(
            Frame->CommandBuffer.Handle,
            Node->SrcImage->Handle,
            Node->DstImage->Handle,
            Node->SrcRect,
            Node->DstRect,
            Node->AspectMask);
    }
}
