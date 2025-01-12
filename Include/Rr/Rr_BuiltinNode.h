#pragma once

#include "Rr/Rr_Graph.h"
#include "Rr/Rr_Text.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum Rr_DrawTextFlags
{
    RR_DRAW_TEXT_FLAGS_NONE_BIT = 0,
    RR_DRAW_TEXT_FLAGS_ANIMATION_BIT = 1
} Rr_DrawTextFlags;

extern Rr_GraphNode *Rr_AddBuiltinNode(
    Rr_App *App,
    const char *Name,
    Rr_GraphNode **Dependencies,
    size_t DependencyCount);

extern void Rr_DrawCustomText(
    Rr_App *App,
    Rr_GraphNode *Node,
    Rr_Font *Font,
    Rr_String *String,
    Rr_Vec2 Position,
    float Size,
    Rr_DrawTextFlags Flags);

extern void Rr_DrawDefaultText(Rr_App *App, Rr_GraphNode *Node, Rr_String *String, Rr_Vec2 Position);

#ifdef __cplusplus
}
#endif
