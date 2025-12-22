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

#ifndef RR_APP_H
#define RR_APP_H

#include <Rr/Rr_Platform.h>

typedef struct Rr_AppConfig Rr_AppConfig;
struct Rr_AppConfig
{
    const char *Title;
    Rr_WindowFlags WindowFlags;
    void (*InitFunc)(void);
    void (*EventFunc)(Rr_Event *Event);
    void (*IterateFunc)(void);
    void (*CleanupFunc)(void);
};

RR_EXTERN void Rr_Run(Rr_AppConfig *Config);

RR_EXTERN void Rr_InitThreadContext(void);

RR_EXTERN void Rr_CleanupThreadContext(void);

RR_EXTERN void Rr_SetTargetFrameRate(uint32_t FramesPerSecond);

RR_EXTERN void Rr_SetBackgroundFrameRate(uint32_t FramesPerSecond);

RR_EXTERN double Rr_GetFramesPerSecond(void);

RR_EXTERN double Rr_GetDeltaSeconds(void);

RR_EXTERN double Rr_GetTimeSeconds(void);

RR_EXTERN uint64_t Rr_GetTimeMS(void);

RR_EXTERN uint64_t Rr_GetTimeNS(void);

RR_EXTERN void Rr_Quit(void);

RR_EXTERN bool Rr_QuitRequested(void);

#endif
