#pragma once

#include <Rr/Rr_Input.h>
#include <Rr/Rr_Platform.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_AppConfig Rr_AppConfig;
struct Rr_AppConfig
{
    const char *Title;
    const char *Version;
    const char *Package;
    void (*InitFunc)(void *UserData);
    void (*EventFunc)(Rr_Event *Event);
    void (*IterateFunc)(void *UserData);
    void (*CleanupFunc)(void *UserData);
    void *UserData;
};

extern void Rr_Run(Rr_AppConfig *Config);

extern void Rr_SetFrameLimiterEnabled(bool Enabled);

extern struct Rr_Renderer *Rr_GetRenderer(void);

extern Rr_IntVec2 Rr_GetWindowSize(void);

extern void Rr_SetWindowTitle(const char *Title);

extern double Rr_GetFramesPerSecond(void);

extern void Rr_ToggleFullscreen(void);

extern float Rr_GetAspectRatio(void);

extern double Rr_GetDeltaSeconds(void);

extern double Rr_GetTimeSeconds(void);

extern void Rr_SetRelativeMouseMode(bool IsRelative);

#ifdef __cplusplus
}
#endif
