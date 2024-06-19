#pragma once

#include "Rr_Input.h"

#ifdef __cplusplus
extern "C"
{
#endif

struct Rr_Arena;
struct Rr_ArenaScratch;

typedef struct Rr_App Rr_App;

typedef struct Rr_AppConfig Rr_AppConfig;
struct Rr_AppConfig
{
    Rr_CString Title;
    void (*InitFunc)(Rr_App *App, void *UserData);
    void (*CleanupFunc)(Rr_App *App, void *UserData);
    void (*IterateFunc)(Rr_App *App, void *UserData);
    void (*FileDroppedFunc)(Rr_App *App, Rr_CString Path);
    void *UserData;
};

extern void
Rr_Run(Rr_AppConfig *Config);

extern void
Rr_DebugOverlay(Rr_App *App);

extern void
Rr_ToggleFullscreen(Rr_App *App);

extern Rr_F32
Rr_GetAspectRatio(Rr_App *App);

extern Rr_F64
Rr_GetDeltaSeconds(Rr_App *App);

extern Rr_F64
Rr_GetTimeSeconds(Rr_App *App);

extern void
Rr_SetInputConfig(Rr_App *App, Rr_InputConfig *InputConfig);

extern Rr_InputState
Rr_GetInputState(Rr_App *App);

extern void
Rr_SetRelativeMouseMode(Rr_Bool bRelative);

#ifdef __cplusplus
}
#endif
