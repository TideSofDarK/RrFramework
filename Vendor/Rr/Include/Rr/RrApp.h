#pragma once

typedef struct Rr_AppConfig
{
    const char* Title;
} Rr_AppConfig;

void Rr_Run(Rr_AppConfig* Config);
