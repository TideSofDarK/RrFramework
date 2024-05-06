#pragma once

#include "Rr_Defines.h"
#include "Rr_String.h"
#include "Rr_Asset.h"
#include "Rr_Math.h"
#include "Rr_Input.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct Rr_LoadTask Rr_LoadTask;
typedef struct Rr_UploadContext Rr_UploadContext;
typedef struct Rr_Buffer Rr_Buffer;
typedef struct Rr_Image Rr_Image;
typedef struct Rr_StagingBuffer Rr_StagingBuffer;
typedef struct Rr_DescriptorAllocator Rr_DescriptorAllocator;
typedef struct Rr_DescriptorWriter Rr_DescriptorWriter;
typedef struct Rr_DescriptorPoolSizeRatio Rr_DescriptorPoolSizeRatio;
typedef struct Rr_DescriptorLayoutBuilder Rr_DescriptorLayoutBuilder;
typedef struct Rr_DescriptorSetLayout Rr_DescriptorSetLayout;
typedef struct Rr_GenericPipelineBuffers Rr_GenericPipelineBuffers;
typedef struct Rr_GenericPipeline Rr_GenericPipeline;
typedef struct Rr_PipelineBuilder Rr_PipelineBuilder;
typedef struct Rr_LoadingContext Rr_LoadingContext;
typedef struct Rr_PendingLoad Rr_PendingLoad;
typedef struct Rr_Material Rr_Material;
typedef struct Rr_StaticMesh Rr_StaticMesh;
typedef struct Rr_RawMesh Rr_RawMesh;
typedef struct Rr_Asset Rr_Asset;
typedef struct Rr_Frame Rr_Frame;
typedef struct Rr_Font Rr_Font;
typedef struct Rr_Renderer Rr_Renderer;
typedef struct Rr_InputConfig Rr_InputConfig;
typedef struct Rr_AppConfig Rr_AppConfig;
typedef struct Rr_App Rr_App;

/* Pipeline */
typedef enum Rr_VertexInputType
{
    RR_VERTEX_INPUT_TYPE_NONE,
    RR_VERTEX_INPUT_TYPE_FLOAT,
    RR_VERTEX_INPUT_TYPE_UINT,
    RR_VERTEX_INPUT_TYPE_VEC2,
    RR_VERTEX_INPUT_TYPE_VEC3,
    RR_VERTEX_INPUT_TYPE_VEC4,
} Rr_VertexInputType;

typedef struct Rr_VertexInputAttribute
{
    Rr_VertexInputType Type;
    u32 Location;
} Rr_VertexInputAttribute;

typedef struct Rr_VertexInput
{
    Rr_VertexInputAttribute Attributes[RR_PIPELINE_MAX_VERTEX_INPUT_ATTRIBUTES];
} Rr_VertexInput;

typedef enum Rr_PolygonMode
{
    RR_POLYGON_MODE_FILL,
    RR_POLYGON_MODE_LINE
} Rr_PolygonMode;

Rr_PipelineBuilder* Rr_CreatePipelineBuilder(void);
void Rr_EnableTriangleFan(Rr_PipelineBuilder* PipelineBuilder);
void Rr_EnablePerVertexInputAttributes(Rr_PipelineBuilder* PipelineBuilder, Rr_VertexInput* VertexInput);
void Rr_EnablePerInstanceInputAttributes(Rr_PipelineBuilder* PipelineBuilder, Rr_VertexInput* VertexInput);
void Rr_EnableVertexStage(Rr_PipelineBuilder* PipelineBuilder, const Rr_Asset* SPVAsset);
void Rr_EnableFragmentStage(Rr_PipelineBuilder* PipelineBuilder, const Rr_Asset* SPVAsset);
// void Rr_EnableAdditionalColorAttachment(Rr_PipelineBuilder* PipelineBuilder, VkFormat Format);
void Rr_EnableRasterizer(Rr_PipelineBuilder* PipelineBuilder, Rr_PolygonMode PolygonMode);
void Rr_EnableDepthTest(Rr_PipelineBuilder* PipelineBuilder);
void Rr_EnableAlphaBlend(Rr_PipelineBuilder* PipelineBuilder);
Rr_GenericPipeline* Rr_BuildGenericPipeline(
    Rr_Renderer* Renderer,
    Rr_PipelineBuilder* PipelineBuilder,
    size_t GlobalsSize,
    size_t MaterialSize,
    size_t DrawSize);
void Rr_DestroyGenericPipeline(Rr_Renderer* Renderer, Rr_GenericPipeline* Pipeline);

/* Renderer */
typedef enum Rr_DrawTextFlags
{
    RR_DRAW_TEXT_FLAGS_NONE_BIT = 0,
    RR_DRAW_TEXT_FLAGS_ANIMATION_BIT = 1
} Rr_DrawTextFlags;

/* Rendering */
typedef struct Rr_BeginRenderingInfo
{
    Rr_GenericPipeline* Pipeline;
    Rr_Image* InitialColor;
    Rr_Image* InitialDepth;
    Rr_Image* AdditionalAttachment;
    const void* GlobalsData;
} Rr_BeginRenderingInfo;
typedef struct Rr_DrawMeshInfo
{
    const Rr_Material* Material;
    const Rr_StaticMesh* StaticMesh;
    char DrawData[RR_PIPELINE_MAX_DRAW_SIZE];
} Rr_DrawMeshInfo;
typedef struct Rr_DrawTextInfo
{
    Rr_Font* Font;
    Rr_String String;
    vec2 Position;
    f32 Size;
    Rr_DrawTextFlags Flags;
} Rr_DrawTextInfo;
typedef struct Rr_RenderingContext
{
    Rr_BeginRenderingInfo* Info;
    Rr_Renderer* Renderer;
    Rr_DrawMeshInfo* DrawMeshArray;
    Rr_DrawTextInfo* DrawTextArray;
} Rr_RenderingContext;
Rr_RenderingContext Rr_BeginRendering(Rr_Renderer* Renderer, Rr_BeginRenderingInfo* Info);
void Rr_DrawStaticMesh(
    Rr_RenderingContext* RenderingContext,
    Rr_Material* Material,
    Rr_StaticMesh* StaticMesh,
    const void* DrawData);
void Rr_DrawCustomText(
    Rr_RenderingContext* RenderingContext,
    Rr_Font* Font,
    Rr_String* String,
    vec2 Position,
    f32 Size,
    Rr_DrawTextFlags Flags);
void Rr_DrawDefaultText(Rr_RenderingContext* RenderingContext, Rr_String* String, vec2 Position);
void Rr_EndRendering(Rr_RenderingContext* RenderingContext);

float Rr_GetAspectRatio(Rr_Renderer* Renderer);

/* Static Mesh */
void Rr_DestroyStaticMesh(Rr_Renderer* Renderer, Rr_StaticMesh* Mesh);

/* Image */
void Rr_DestroyImage(Rr_Renderer* Renderer, Rr_Image* AllocatedImage);

/* Material */
Rr_Material* Rr_CreateMaterial(Rr_Renderer* Renderer, Rr_Image** Textures, size_t TextureCount);
void Rr_DestroyMaterial(Rr_Renderer* Renderer, Rr_Material* Material);

/* Load */
typedef enum Rr_LoadStatus
{
    RR_LOAD_STATUS_READY,
    RR_LOAD_STATUS_PENDING,
    RR_LOAD_STATUS_LOADING,
    RR_LOAD_STATUS_NO_TASKS
} Rr_LoadStatus;

typedef void (*Rr_LoadingCallback)(Rr_Renderer* Renderer, const void* Userdata);

Rr_LoadingContext* Rr_CreateLoadingContext(Rr_Renderer* Renderer, size_t InitialTaskCount);
void Rr_LoadColorImageFromPNG(Rr_LoadingContext* LoadingContext, const Rr_Asset* Asset, Rr_Image** OutImage);
void Rr_LoadMeshFromOBJ(Rr_LoadingContext* LoadingContext, const Rr_Asset* Asset, Rr_StaticMesh** OutStaticMesh);
void Rr_LoadAsync(Rr_LoadingContext* LoadingContext, Rr_LoadingCallback LoadingCallback, const void* Userdata);
Rr_LoadStatus Rr_LoadImmediate(Rr_LoadingContext* LoadingContext);
void Rr_GetLoadProgress(const Rr_LoadingContext* LoadingContext, u32* OutCurrent, u32* OutTotal);

/* App */
struct Rr_AppConfig
{
    const char* Title;
    ivec2 ReferenceResolution;
    Rr_InputConfig* InputConfig;
    void (*InitFunc)(Rr_App* App);
    void (*CleanupFunc)(Rr_App* App);
    void (*UpdateFunc)(Rr_App* App);
    void (*DrawFunc)(Rr_App* App);

    void (*FileDroppedFunc)(Rr_App* App, const char* Path);
};

void Rr_Run(Rr_AppConfig* Config);
void Rr_DebugOverlay(Rr_App* App);
void Rr_ToggleFullscreen(Rr_App* App);
Rr_Renderer* Rr_GetRenderer(Rr_App* App);
Rr_InputState Rr_GetInputState(Rr_App* App);

#ifdef __cplusplus
}
#endif
