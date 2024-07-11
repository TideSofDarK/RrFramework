#pragma once

#include "Rr/Rr_Graph.h"

#ifdef __cplusplus
extern "C" {
#endif

extern Rr_GraphNode *Rr_AddBlitNode(
    Rr_App *App,
    const char *Name,
    Rr_GraphNode **Dependencies,
    size_t DependencyCount);

#ifdef __cplusplus
}
#endif
