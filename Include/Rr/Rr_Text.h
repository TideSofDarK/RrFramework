#pragma once

#include <Rr/Rr_Asset.h>
#include <Rr/Rr_Renderer.h>
#include <Rr/Rr_String.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_Font Rr_Font;

#define RR_TEXT_BUFFER_SIZE (1024 * 1024)
#define RR_TEXT_MAX_COLORS  8
#define RR_TEXT_MAX_GLYPHS  2048

void Rr_InitTextRenderer(Rr_App *App);

void Rr_CleanupTextRenderer(Rr_App *App);

Rr_Font *Rr_CreateFont(
    Rr_Renderer *Renderer,
    Rr_AssetRef FontPNGRef,
    Rr_AssetRef FontJSONRef);

void Rr_DestroyFont(Rr_Renderer *Renderer, Rr_Font *Font);

Rr_Vec2 Rr_CalculateTextSize(Rr_Font *Font, float FontSize, Rr_String *String);

#ifdef __cplusplus
}
#endif
