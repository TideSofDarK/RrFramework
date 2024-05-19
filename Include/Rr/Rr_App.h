#pragma once

#include "Rr_Input.h"
#include "Rr_Math.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct Rr_App Rr_App;

typedef struct Rr_AppConfig
{
    const char* Title;
    Rr_IntVec2 ReferenceResolution;
    Rr_InputConfig* InputConfig;
    void (*InitFunc)(Rr_App* App);
    void (*CleanupFunc)(Rr_App* App);
    void (*UpdateFunc)(Rr_App* App);
    void (*DrawFunc)(Rr_App* App);

    void (*FileDroppedFunc)(Rr_App* App, const char* Path);
} Rr_AppConfig;

void Rr_Run(Rr_AppConfig* Config);
void Rr_DebugOverlay(Rr_App* App);
void Rr_ToggleFullscreen(Rr_App* App);
float Rr_GetAspectRatio(Rr_App* App);
Rr_InputState Rr_GetInputState(Rr_App* App);

#ifdef __cplusplus
}
#endif
