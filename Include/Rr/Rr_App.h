/*
 * MIT License
 *
 * Copyright (c) 2024-2025 Alexandr Semenov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

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
    Rr_WindowFlags WindowFlags;
    void (*InitFunc)(void *UserData);
    void (*EventFunc)(void *UserData, Rr_Event *Event);
    void (*IterateFunc)(void *UserData);
    void (*CleanupFunc)(void *UserData);
    void *UserData;
};

extern void Rr_Run(Rr_AppConfig *Config);

extern void Rr_SetFrameLimiterEnabled(bool Enabled);

extern double Rr_GetFramesPerSecond(void);

extern double Rr_GetDeltaSeconds(void);

extern double Rr_GetTimeSeconds(void);

extern uint64_t Rr_GetTimeMS(void);

extern uint64_t Rr_GetTimeNS(void);

extern void Rr_Quit(void);

#ifdef __cplusplus
}
#endif
