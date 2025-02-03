#pragma once

#include <Rr/Rr_Graph.h>
#include <Rr/Rr_Image.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum Rr_PresentMode
{
    RR_PRESENT_MODE_NORMAL,
    RR_PRESENT_MODE_STRETCH,
    RR_PRESENT_MODE_FIT,
} Rr_PresentMode;

extern Rr_GraphNode *Rr_AddPresentNode(
    Rr_App *App,
    const char *Name,
    Rr_Image *Image,
    Rr_PresentMode Mode);

#ifdef __cplusplus
}
#endif
