#pragma once

#include <Rr/Rr_Buffer.h>
#include <Rr/Rr_Graph.h>

#ifdef __cplusplus
extern "C" {
#endif

extern Rr_GraphNode *Rr_AddTransferNode(
    Rr_App *App,
    const char *Name,
    Rr_Buffer *DstBuffer,
    Rr_Data Data,
    size_t *InOutOffset,
    Rr_GraphNode **Dependencies,
    size_t DependencyCount);

#ifdef __cplusplus
}
#endif
