#pragma once

typedef struct Rr_Font Rr_Font;

#include "Rr_Asset.h"
#include "Rr_Renderer.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define RR_TEXT_BUFFER_SIZE (1024 * 1024)
#define RR_TEXT_DEFAULT_SIZE (0.0f)
#define RR_TEXT_MAX_COLORS 8
#define RR_TEXT_MAX_GLYPHS 2048

void Rr_InitTextRenderer(Rr_Renderer* Renderer);
void Rr_CleanupTextRenderer(Rr_Renderer* Renderer);

Rr_Font* Rr_CreateFont(Rr_Renderer* Renderer, Rr_Asset* FontPNG, Rr_Asset* FontJSON);
void Rr_DestroyFont(Rr_Renderer* Renderer, Rr_Font* Font);

#ifdef __cplusplus
}
#endif
