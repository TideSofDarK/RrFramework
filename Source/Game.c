#include "Game.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <imgui/cimgui.h>

#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
// #define CGLM_FORCE_LEFT_HANDED
#include <cglm/cam.h>
#include <cglm/vec3.h>
#include <cglm/mat4.h>
#include <cglm/euler.h>
#include <cglm/cglm.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_scancode.h>

#include <Rr/RrMath.h>
#include <Rr/RrDefines.h>
#include <Rr/RrImage.h>
#include <Rr/RrApp.h>
#include <Rr/RrBuffer.h>
#include <Rr/RrTypes.h>
#include <Rr/RrMesh.h>
#include <Rr/RrRenderer.h>
#include <Rr/RrInput.h>
#include <Rr/RrDescriptor.h>
#include <Rr/RrPipeline.h>
#include <Rr/RrMaterial.h>

#include "Render.h"
#include "DevTools.h"

typedef struct SFrameData
{
    float Reserved;
} SFrameData;

typedef struct SUber3DGlobals
{
    mat4 View;
    mat4 Intermediate;
    mat4 Proj;
    vec4 AmbientColor;
} SUber3DGlobals;

typedef struct SUber3DMaterial
{
    vec3 Emissive;
} SUber3DMaterial;

typedef struct SUber3DDraw
{
    mat4 Model;
    VkDeviceAddress VertexBufferAddress;
} SUber3DDraw;

typedef struct SUber3DPushConstants
{
    mat4 Reserved;
} SUber3DPushConstants;

typedef enum EInputAction
{
    EIA_UP,
    EIA_DOWN,
    EIA_LEFT,
    EIA_RIGHT,
    EIA_FULLSCREEN,
    EIA_DEBUGOVERLAY,
    EIA_TEST,
    EIA_COUNT,
} EInputAction;

static Rr_InputMapping InputMappings[RR_MAX_INPUT_MAPPINGS] = { 0 };

static SUber3DGlobals ShaderGlobals;

static Rr_MeshBuffers MonkeyMesh;

static Rr_Pipeline Uber3DPipeline = { 0 };

static Rr_Image SceneDepthImage;
static Rr_Image SceneColorImage;

static Rr_MeshBuffers CottageMesh = { 0 };
static Rr_Image CottageTexture;
static Rr_Material CottageMaterial;

static Rr_MeshBuffers PocMesh;
static Rr_Image PocDiffuseImage;

static Rr_Material MonkeyMaterial;
// static Rr_Image MonkeyImage;

static void InitInputMappings(void)
{
    InputMappings[EIA_UP].Primary = SDL_SCANCODE_W;
    InputMappings[EIA_DOWN].Primary = SDL_SCANCODE_S;
    InputMappings[EIA_LEFT].Primary = SDL_SCANCODE_A;
    InputMappings[EIA_RIGHT].Primary = SDL_SCANCODE_D;
    InputMappings[EIA_FULLSCREEN].Primary = SDL_SCANCODE_F11;
    InputMappings[EIA_DEBUGOVERLAY].Primary = SDL_SCANCODE_F1;
    InputMappings[EIA_TEST].Primary = SDL_SCANCODE_F2;
}

static void InitUber3DPipeline(Rr_Renderer* const Renderer)
{
    Rr_Asset VertexShaderSPV, FragmentShaderSPV;
    RrAsset_Extern(&VertexShaderSPV, Uber3DVERT);
    RrAsset_Extern(&FragmentShaderSPV, Uber3DFRAG);

    Rr_PipelineBuilder Builder = Rr_GenericPipelineBuilder();
    Builder.GlobalsSize = sizeof(SUber3DGlobals);
    Builder.MaterialSize = sizeof(SUber3DMaterial);
    Builder.DrawSize = sizeof(SUber3DDraw);
    Rr_EnableVertexStage(&Builder, &VertexShaderSPV);
    Rr_EnableFragmentStage(&Builder, &FragmentShaderSPV);
    Rr_EnableDepthTest(&Builder);
    Rr_EnableRasterizer(&Builder, RR_POLYGON_MODE_FILL);
    Uber3DPipeline = Rr_BuildGenericPipeline(Renderer, &Builder);
}

static void InitGlobals(Rr_Renderer* const Renderer)
{
    glm_vec4_copy((vec4){ 1.0f, 1.0f, 1.0f, 0.5f }, ShaderGlobals.AmbientColor);

    Rr_Perspective(ShaderGlobals.Proj, 0.7643276f, Rr_GetAspectRatio(Renderer));
    Rr_VulkanMatrix(ShaderGlobals.Intermediate);
}

static void Init(Rr_App* App)
{
    Rr_Array_Test();
    InitInputMappings();

    Rr_Renderer* const Renderer = &App->Renderer;

    Rr_Asset POCDepthEXR;
    RrAsset_Extern(&POCDepthEXR, POCDepthEXR);
    SceneDepthImage = Rr_CreateImageFromEXR(&POCDepthEXR, &App->Renderer);

    Rr_Asset POCColorPNG;
    RrAsset_Extern(&POCColorPNG, POCColorPNG);
    SceneColorImage = Rr_CreateImageFromPNG(&POCColorPNG, &App->Renderer, VK_IMAGE_USAGE_SAMPLED_BIT, false, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    //
    //    Rr_Asset DoorFrameOBJ;
    //    RrAsset_Extern(&DoorFrameOBJ, DoorFrameOBJ);
    //    MonkeyMesh = Rr_CreateMesh_FromOBJ(Renderer, &DoorFrameOBJ);

    Rr_Asset CottageOBJ;
    RrAsset_Extern(&CottageOBJ, CottageOBJ);
    CottageMesh = Rr_CreateMesh_FromOBJ(Renderer, &CottageOBJ);

    Rr_Asset CottagePNG;
    RrAsset_Extern(&CottagePNG, CottagePNG);
    CottageTexture = Rr_CreateImageFromPNG(&CottagePNG, &App->Renderer, VK_IMAGE_USAGE_SAMPLED_BIT, false, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    Rr_Asset PocMeshOBJ;
    RrAsset_Extern(&PocMeshOBJ, PocMeshOBJ);
    PocMesh = Rr_CreateMesh_FromOBJ(Renderer, &PocMeshOBJ);

    Rr_Asset PocDiffusePNG;
    RrAsset_Extern(&PocDiffusePNG, PocDiffusePNG);
    PocDiffuseImage = Rr_CreateImageFromPNG(&PocDiffusePNG, &App->Renderer, VK_IMAGE_USAGE_SAMPLED_BIT, false, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    Rr_Asset MonkeyOBJ;
    RrAsset_Extern(&MonkeyOBJ, DoorFrameOBJ);
    MonkeyMesh = Rr_CreateMesh_FromOBJ(Renderer, &MonkeyOBJ);

    InitUber3DPipeline(Renderer);
    InitGlobals(Renderer);

    MonkeyMaterial = Rr_CreateMaterial(Renderer, NULL, 0);
    Rr_Image* CottageTextures[1] = {&CottageTexture};
    CottageMaterial = Rr_CreateMaterial(Renderer, CottageTextures, 1);
}

static void Cleanup(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;
    VkDevice Device = Renderer->Device;

    for (int Index = 0; Index < RR_FRAME_OVERLAP; Index++)
    {
        SFrameData* PerFrameData = (SFrameData*)Renderer->PerFrameDatas + Index;
    }
    SDL_free(Renderer->PerFrameDatas);

    Rr_DestroyMaterial(Renderer, &MonkeyMaterial);
    Rr_DestroyMaterial(Renderer, &CottageMaterial);

    Rr_DestroyImage(Renderer, &SceneDepthImage);
    Rr_DestroyImage(Renderer, &SceneColorImage);
    Rr_DestroyImage(Renderer, &CottageTexture);
    Rr_DestroyImage(Renderer, &PocDiffuseImage);

    Rr_DestroyMesh(Renderer, &MonkeyMesh);
    Rr_DestroyMesh(Renderer, &CottageMesh);
    Rr_DestroyMesh(Renderer, &PocMesh);

    Rr_DestroyPipeline(Renderer, &Uber3DPipeline);
}

static void Update(Rr_App* App)
{
    Rr_KeyState Up = Rr_GetKeyState(App->InputState, EIA_UP);
    if (Up == RR_KEYSTATE_PRESSED)
    {
    }

    if (Rr_GetKeyState(App->InputState, EIA_FULLSCREEN) == RR_KEYSTATE_PRESSED)
    {
        Rr_ToggleFullscreen(App);
    }

    igBegin("RotTest", NULL, ImGuiWindowFlags_AlwaysAutoResize);
    static vec3 CameraPos = { -7.35889f, -4.0f, -6.92579f };
    static vec3 CameraEuler = { 90.0f - 63.5593f, -46.6919f, 0.0f };
    igSliderFloat3("CameraPos", CameraPos, -8.0f, 8.0f, "%f", ImGuiSliderFlags_None);
    igSliderFloat3("CameraRot", CameraEuler, -190.0f, 190.0f, "%f", ImGuiSliderFlags_None);

    mat4 Temp;
    glm_mat4_identity(Temp);
    glm_vec3_copy(CameraPos, Temp[3]);

    mat4 Temp2;
    glm_euler_xyz((vec3){ glm_rad(CameraEuler[0]), glm_rad(CameraEuler[1]), glm_rad(CameraEuler[2]) }, Temp2);

    glm_mat4_mul(Temp2, Temp, ShaderGlobals.View);
    igEnd();

    static b8 bShowDebugOverlay = false;
    if (Rr_GetKeyState(App->InputState, EIA_DEBUGOVERLAY) == RR_KEYSTATE_PRESSED)
    {
        bShowDebugOverlay = !bShowDebugOverlay;
    }
    if (bShowDebugOverlay)
    {
        Rr_DebugOverlay(App);
    }
}

static void Draw(Rr_App* const App)
{
    Rr_Renderer* Renderer = &App->Renderer;
    Rr_Frame* Frame = Rr_GetCurrentFrame(Renderer);

    glm_perspective_resize(Rr_GetAspectRatio(Renderer), ShaderGlobals.Proj);

    /* Rendering */
    Rr_BeginRenderingInfo BeginRenderingInfo = {
        .Pipeline = &Uber3DPipeline,
        .InitialDepth = &SceneDepthImage,
        .InitialColor = &SceneColorImage,
        .GlobalsData = &ShaderGlobals
    };
    Rr_RenderingContext RenderingContext = Rr_BeginRendering(Renderer, &BeginRenderingInfo);

    u64 Ticks = SDL_GetTicks();
    float Time = (float)((double)Ticks / 1000.0 * 2);
    SUber3DDraw Draw;
    mat4 Transform;
    glm_euler_xyz((vec3){ 0.0f, SDL_fmodf(Time, SDL_PI_F * 2.0f), 0.0f }, Transform);
    glm_mat4_identity(Draw.Model);
    glm_scale_uni(Draw.Model, 0.5f);
    Draw.Model[3][1] = 0.5f;
    Draw.Model[3][2] = 3.5f;
    glm_mat4_mul(Transform, Draw.Model, Draw.Model);
    Draw.VertexBufferAddress = MonkeyMesh.VertexBufferAddress;

    Rr_DrawMeshInfo DrawMeshInfo = {
        .Material = &MonkeyMaterial,
        .MeshBuffers = &MonkeyMesh,
        .DrawData = &Draw
    };

    SUber3DDraw Draw2;
    glm_euler_xyz((vec3){ 0.0f, SDL_fmodf(Time, SDL_PI_F * 2.0f), 0.0f }, Transform);
    glm_mat4_identity(Draw2.Model);
    glm_scale_uni(Draw2.Model, 0.5f);
    glm_mat4_mul(Transform, Draw2.Model, Draw2.Model);
    Draw2.Model[3][0] = 3.5f;
    Draw2.Model[3][1] = 0.5f;
    Draw2.Model[3][2] = 3.5f;
    Draw2.VertexBufferAddress = CottageMesh.VertexBufferAddress;

    Rr_DrawMeshInfo DrawMeshInfo2 = {
        .Material = &MonkeyMaterial,
        .MeshBuffers = &CottageMesh,
        .DrawData = &Draw2
    };

    Rr_DrawMesh(&RenderingContext, &DrawMeshInfo);
    Rr_DrawMesh(&RenderingContext, &DrawMeshInfo2);


    Rr_EndRendering(&RenderingContext);
}

static void OnFileDropped(Rr_App* App, const char* Path)
{
    HandleFileDrop(Path);
}

void RunGame(void)
{
    Rr_AppConfig Config = {
        .Title = "VulkanPlayground",
        .ReferenceResolution = { 1920 / 2, 1080 / 2 },
        .InputConfig = &(Rr_InputConfig){
            .Mappings = InputMappings,
            .Count = EIA_COUNT },
        .PerFrameDataSize = sizeof(SFrameData),
        .InitFunc = Init,
        .CleanupFunc = Cleanup,
        .UpdateFunc = Update,
        .DrawFunc = Draw,

        .FileDroppedFunc = OnFileDropped
    };
    Rr_Run(&Config);
}
