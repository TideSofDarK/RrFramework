#pragma once

#include "Rr/Rr_App.h"
#include "Rr_Asset.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_Font Rr_Font;

#define RR_TEXT_BUFFER_SIZE (1024 * 1024)
#define RR_TEXT_MAX_COLORS  8
#define RR_TEXT_MAX_GLYPHS  2048

void Rr_InitTextRenderer(Rr_App *App);

void Rr_CleanupTextRenderer(Rr_App *App);

Rr_Font *
Rr_CreateFont(Rr_App *App, Rr_AssetRef FontPNGRef, Rr_AssetRef FontJSONRef);

void Rr_DestroyFont(Rr_App *App, Rr_Font *Font);

#ifdef __cplusplus
}
#endif
