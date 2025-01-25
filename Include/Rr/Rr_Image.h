#pragma once

#include <Rr/Rr_App.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_Image Rr_Image;

extern Rr_Image *Rr_CreateColorAttachmentImage(Rr_App *App, uint32_t Width, uint32_t Height);

extern void Rr_DestroyImage(Rr_App *App, Rr_Image *Image);

extern Rr_Image *Rr_GetDummyColorTexture(Rr_App *App);

extern Rr_Image *Rr_GetDummyNormalTexture(Rr_App *App);

#ifdef __cplusplus
}
#endif
