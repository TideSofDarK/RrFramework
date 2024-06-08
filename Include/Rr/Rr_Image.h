#pragma once

#include "Rr/Rr_App.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct Rr_Image Rr_Image;

extern void Rr_DestroyImage(Rr_App* App, Rr_Image* AllocatedImage);

#ifdef __cplusplus
}
#endif
