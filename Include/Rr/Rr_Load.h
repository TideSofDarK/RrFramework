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

#include <Rr/Rr_App.h>
#include <Rr/Rr_Asset.h>
#include <Rr/Rr_GLTF.h>
#include <Rr/Rr_Image.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_LoadThread Rr_LoadThread;
typedef struct Rr_LoadContext Rr_LoadContext;

typedef enum Rr_LoadResult
{
    RR_LOAD_RESULT_READY,
    RR_LOAD_RESULT_WRONG_LOAD_TYPE,
    RR_LOAD_RESULT_NO_TASKS
} Rr_LoadResult;

typedef enum Rr_LoadType
{
    RR_LOAD_TYPE_IMAGE_RGBA8_FROM_PNG,
    RR_LOAD_TYPE_GLTF_ASSET,
    RR_LOAD_TYPE_CUSTOM,
} Rr_LoadType;

typedef struct Rr_LoadGLTFOptions Rr_LoadGLTFOptions;
struct Rr_LoadGLTFOptions
{
    struct Rr_GLTFContext *GLTFContext;
};

typedef struct Rr_LoadTask Rr_LoadTask;
struct Rr_LoadTask
{
    Rr_LoadType LoadType;
    Rr_AssetRef AssetRef;
    union
    {
        Rr_LoadGLTFOptions GLTF;
    } Options;
    union
    {
        void **Any;
        Rr_Image **Image;
        Rr_GLTFAsset **GLTFAsset;
    } Out;
};

typedef void (*Rr_LoadCallback)(void *UserData);

extern Rr_LoadThread *Rr_CreateLoadThread(void);

extern void Rr_DestroyLoadThread(Rr_LoadThread *LoadThread);

extern Rr_LoadTask Rr_LoadGLTFAssetTask(
    Rr_AssetRef AssetRef,
    Rr_GLTFContext *Context,
    Rr_GLTFAsset **Out);

extern Rr_LoadTask Rr_LoadImageRGBA8FromPNGTask(
    Rr_AssetRef AssetRef,
    Rr_Image **Out);

extern Rr_LoadContext *Rr_LoadAsync(
    Rr_LoadThread *LoadThread,
    size_t TaskCount,
    Rr_LoadTask *Tasks,
    Rr_LoadCallback LoadCallback,
    void *UserData);

extern Rr_LoadResult Rr_LoadImmediate(
    Rr_Renderer *Renderer,
    size_t TaskCount,
    Rr_LoadTask *Tasks);

extern void Rr_GetLoadProgress(
    Rr_LoadContext *LoadContext,
    size_t *OutCurrent,
    size_t *OutTotal);

#ifdef __cplusplus
}
#endif
