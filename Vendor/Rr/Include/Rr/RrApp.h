#pragma once

#include <cglm/ivec2.h>

typedef struct Rr_App Rr_App;

typedef struct Rr_AppConfig
{
    const char* Title;
    ivec2 ReferenceResolution;
    void (*InitFunc)(Rr_App* App);
    void (*CleanupFunc)(Rr_App* App);
    void (*UpdateFunc)(Rr_App* App);
} Rr_AppConfig;

void Rr_Run(Rr_AppConfig* Config);
