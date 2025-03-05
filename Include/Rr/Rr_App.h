#pragma once

#include <Rr/Rr_Input.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_App Rr_App;

typedef struct Rr_AppConfig Rr_AppConfig;
struct Rr_AppConfig
{
    const char *Title;
    const char *Version;
    const char *Package;
    void (*InitFunc)(Rr_App *App, void *UserData);
    void (*CleanupFunc)(Rr_App *App, void *UserData);
    void (*IterateFunc)(Rr_App *App, void *UserData);
    void (*FileDroppedFunc)(Rr_App *App, const char *Path);
    void *UserData;
};

extern void Rr_Run(Rr_AppConfig *Config);

extern void Rr_SetFrameLimiterEnabled(Rr_App *App, bool Enabled);

extern struct Rr_Renderer *Rr_GetRenderer(Rr_App *App);

extern void Rr_DebugOverlay(Rr_App *App);

extern void Rr_ToggleFullscreen(Rr_App *App);

extern float Rr_GetAspectRatio(Rr_App *App);

extern double Rr_GetDeltaSeconds(Rr_App *App);

extern double Rr_GetTimeSeconds(Rr_App *App);

extern void Rr_SetRelativeMouseMode(Rr_App *App, bool IsRelative);

#ifdef __cplusplus
}
#endif
