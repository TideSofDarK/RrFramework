#pragma once

typedef struct SRrAppConfig
{
    const char* Title;
} SRrAppConfig;

void RrApp_Run(SRrAppConfig* Config);
