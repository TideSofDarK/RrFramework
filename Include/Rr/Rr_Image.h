#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

struct Rr_App;

typedef struct Rr_Image Rr_Image;

extern void Rr_DestroyImage(struct Rr_App* App, Rr_Image* AllocatedImage);

#ifdef __cplusplus
}
#endif
