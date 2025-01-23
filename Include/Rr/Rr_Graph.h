#pragma once

#include <Rr/Rr_App.h>
#include <Rr/Rr_String.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_Graph Rr_Graph;
typedef struct Rr_GraphNode Rr_GraphNode;

typedef enum Rr_GraphNodeType
{
    RR_GRAPH_NODE_TYPE_BUILTIN,
    RR_GRAPH_NODE_TYPE_GRAPHICS,
    RR_GRAPH_NODE_TYPE_BLIT,
    RR_GRAPH_NODE_TYPE_PRESENT,
} Rr_GraphNodeType;

#ifdef __cplusplus
}
#endif
