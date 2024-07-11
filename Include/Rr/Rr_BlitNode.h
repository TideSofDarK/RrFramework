#pragma once

#include "Rr/Rr_Graph.h"
#include "Rr/Rr_Image.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum Rr_BlitMode
{
    RR_BLIT_MODE_COLOR,
    RR_BLIT_MODE_DEPTH,
} Rr_BlitMode;

typedef struct Rr_BlitNodeInfo Rr_BlitNodeInfo;
struct Rr_BlitNodeInfo
{
    Rr_Image *SrcImage;
    Rr_Image *DstImage;
    Rr_IntVec4 SrcRect;
    Rr_IntVec4 DstRect;
    Rr_BlitMode Mode;
};

extern Rr_GraphNode *Rr_AddBlitNode(
    Rr_App *App,
    const char *Name,
    Rr_BlitNodeInfo *Info,
    Rr_GraphNode **Dependencies,
    size_t DependencyCount);

#ifdef __cplusplus
}
#endif
