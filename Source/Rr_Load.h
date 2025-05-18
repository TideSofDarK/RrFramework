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

#include <Rr/Rr_Load.h>

#include "Rr_Vulkan.h"

#include <SDL3/SDL_mutex.h>

typedef struct Rr_PendingLoad Rr_PendingLoad;
struct Rr_PendingLoad
{
    Rr_LoadCallback LoadingCallback;
    void *UserData;
};

struct Rr_LoadThread
{
    RR_ARRAY(Rr_LoadContext) LoadContexts;

    SDL_Thread *Handle;
    SDL_Semaphore *Semaphore;
    SDL_Mutex *Mutex;

    Rr_Arena *Arena;
};

struct Rr_LoadContext
{
    SDL_Semaphore *Semaphore;
    Rr_LoadCallback LoadingCallback;
    void *UserData;
    Rr_LoadTask *Tasks;
    size_t TaskCount;
};

typedef struct Rr_LoadAsyncContext Rr_LoadAsyncContext;
struct Rr_LoadAsyncContext
{
    VkCommandPool GraphicsCommandPool;
    VkCommandPool TransferCommandPool;
    VkFence Fence;
    VkSemaphore Semaphore;
};
