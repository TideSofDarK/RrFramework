#include "Rr_BlitNode.h"

#include "Rr_App.h"
#include "Rr_Graph.h"
#include "Rr_Log.h"
#include "Rr_Renderer.h"

Rr_GraphNode *Rr_AddBlitNode(
    Rr_App *App,
    const char *Name,
    Rr_BlitNodeInfo *Info,
    Rr_GraphNode **Dependencies,
    size_t DependencyCount)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_GraphNode *GraphNode = Rr_AddGraphNode(
        Frame,
        RR_GRAPH_NODE_TYPE_BLIT,
        Name,
        Dependencies,
        DependencyCount);

    Rr_BlitNode *BlitNode = &GraphNode->Union.BlitNode;
    *BlitNode = (Rr_BlitNode){
        .Info = *Info,
    };

    switch (Info->Mode)
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

Rr_Bool Rr_BatchBlitNode(
    Rr_App *App,
    Rr_Graph *Graph,
    Rr_GraphBatch *Batch,
    Rr_BlitNode *Node)
{
    if (Rr_SyncImage(
            App,
            Graph,
            Batch,
            Node->Info.SrcImage->Handle,
            Node->AspectMask,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) != RR_TRUE)
    {
        return RR_FALSE;
    }
    if (Rr_SyncImage(
            App,
            Graph,
            Batch,
            Node->Info.DstImage->Handle,
            Node->AspectMask,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) != RR_TRUE)
    {
        return RR_FALSE;
    }

    return RR_TRUE;
}

void Rr_ExecuteBlitNode(Rr_App *App, Rr_BlitNode *Node)
{
    Rr_Frame *Frame = Rr_GetCurrentFrame(&App->Renderer);
    Rr_BlitColorImage(
        Frame->MainCommandBuffer,
        Node->Info.SrcImage->Handle,
        Node->Info.DstImage->Handle,
        Node->Info.SrcRect,
        Node->Info.DstRect,
        Node->AspectMask);
}
