#pragma once

#include <Rr/Rr_Memory.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Rr_App;

extern Rr_Arena *Rr_GetFrameArena(struct Rr_App *App);

#ifdef __cplusplus
}
#endif
