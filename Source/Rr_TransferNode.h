#pragma once

#include <Rr/Rr_TransferNode.h>

struct Rr_AllocatedBuffer;
struct Rr_GraphBatch;

typedef struct Rr_TransferNode Rr_TransferNode;
struct Rr_TransferNode
{
    struct Rr_AllocatedBuffer *DstBuffer;
    size_t DstOffset;
    struct Rr_AllocatedBuffer *SrcBuffer;
    size_t SrcOffset;
    size_t Size;
};

extern bool Rr_BatchTransferNode(Rr_App *App, struct Rr_GraphBatch *Batch, Rr_TransferNode *Node);

extern void Rr_ExecuteTransferNode(Rr_App *App, Rr_TransferNode *Node, Rr_Arena *Arena);
