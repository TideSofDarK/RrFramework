#include "Game.hxx"

#include <string>
#include <format>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <imgui/cimgui.h>

#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#define CGLM_FORCE_LEFT_HANDED
#include <cglm/cam.h>
#include <cglm/vec3.h>
#include <cglm/mat4.h>
#include <cglm/euler.h>
#include <cglm/cglm.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_scancode.h>

#include <Rr/Rr_Math.h>
#include <Rr/Rr_Image.h>
#include <Rr/Rr_App.h>
#include <Rr/Rr_Mesh.h>
#include <Rr/Rr_Renderer.h>
#include <Rr/Rr_Input.h>
#include <Rr/Rr_Pipeline.h>
#include <Rr/Rr_Material.h>
#include <Rr/Rr_Load.h>

#include "DevTools.hxx"

typedef struct SFrameData
{
    float Reserved;
} SFrameData;

typedef struct SUber3DGlobals
{
    mat4 View;
    mat4 Intermediate;
    mat4 Proj;
    vec4 AmbientLightColor;
    vec4 DirectionalLightDirection;
    vec4 DirectionalLightColor;
    vec4 DirectionalLightIntensity;
} SUber3DGlobals;

typedef struct SUber3DMaterial
{
    vec3 Emissive;
} SUber3DMaterial;

typedef struct SUber3DDraw
{
    mat4 Model;
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

static Rr_InputMapping InputMappings[RR_MAX_INPUT_MAPPINGS];

static SUber3DGlobals ShaderGlobals;

static Rr_GenericPipeline* Uber3DPipeline;

// static Rr_Image SceneDepthImage;
// static Rr_Image SceneColorImage;

static Rr_MeshBuffers CottageMesh;
static Rr_Image* CottageTexture;
static Rr_Material CottageMaterial;

static Rr_MeshBuffers PocMesh;
static Rr_Image* PocDiffuseImage;

static Rr_Material MarbleMaterial;
static Rr_Image* MarbleDiffuse;
static Rr_Image* MarbleNormal;
static Rr_Image* MarbleSpecular;
static Rr_MeshBuffers MarbleMesh;

static Rr_String TestString;
static Rr_String DebugString;
static Rr_LoadingContext* LoadingContext;

static bool bLoaded = false;

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
    Rr_ExternAsset(Uber3DVERT);
    Rr_ExternAsset(Uber3DFRAG);

    Rr_PipelineBuilder* Builder = Rr_CreatePipelineBuilder();
    Rr_VertexInput VertexInput = {
        .Attributes = {
            { .Type = RR_VERTEX_INPUT_TYPE_VEC3, .Location = 0 },
            { .Type = RR_VERTEX_INPUT_TYPE_FLOAT, .Location = 1 },
            { .Type = RR_VERTEX_INPUT_TYPE_VEC3, .Location = 2 },
            { .Type = RR_VERTEX_INPUT_TYPE_FLOAT, .Location = 3 },
            { .Type = RR_VERTEX_INPUT_TYPE_VEC4, .Location = 4 },
        }
    };
    Rr_EnablePerVertexInputAttributes(Builder, &VertexInput);
    Rr_EnableVertexStage(Builder, &Uber3DVERT);
    Rr_EnableFragmentStage(Builder, &Uber3DFRAG);
    Rr_EnableDepthTest(Builder);
    Rr_EnableRasterizer(Builder, RR_POLYGON_MODE_FILL);
    Uber3DPipeline = Rr_BuildGenericPipeline(
        Renderer,
        Builder,
        sizeof(SUber3DGlobals),
        sizeof(SUber3DMaterial),
        sizeof(SUber3DDraw));
}

static void InitGlobals(Rr_Renderer* const Renderer)
{
    glm_vec4_copy((vec4){ 0.1f, 0.1f, 0.1f, 1.0f }, ShaderGlobals.AmbientLightColor);
    glm_vec4_copy((vec4){ 1.0f, 0.95f, 0.93f, 1.0f }, ShaderGlobals.DirectionalLightColor);
    glm_vec4_copy((vec4){ 1.0f, 1.0f, 1.0f, 1.0f }, ShaderGlobals.DirectionalLightIntensity);

    Rr_Perspective(ShaderGlobals.Proj, 0.7643276f, Rr_GetAspectRatio(Renderer));
    Rr_VulkanMatrix(ShaderGlobals.Intermediate);
}

static void OnLoadingComplete(Rr_Renderer* Renderer, const void* Userdata)
{
    Rr_Image* MarbleTextures[2] = { MarbleDiffuse, MarbleSpecular };
    MarbleMaterial = Rr_CreateMaterial(Renderer, MarbleTextures, 2);

    Rr_Image* CottageTextures[1] = { CottageTexture };
    CottageMaterial = Rr_CreateMaterial(Renderer, CottageTextures, 1);

    bLoaded = true;

    LoadingContext = nullptr;
}

static void Init(Rr_App* App)
{
#if RR_DEBUG
    Rr_Array_Test();
#endif
    InitInputMappings();

    Rr_Renderer* Renderer = Rr_GetRenderer(App);

    Rr_ExternAsset(MarbleDiffusePNG);
    Rr_ExternAsset(MarbleSpecularPNG);
    Rr_ExternAsset(MarbleOBJ);
    Rr_ExternAsset(CottagePNG);
    Rr_ExternAsset(CottageOBJ);

    LoadingContext = Rr_CreateLoadingContext(Renderer, 8);
    Rr_LoadColorImageFromPNG(LoadingContext, &MarbleDiffusePNG, &MarbleDiffuse);
    Rr_LoadColorImageFromPNG(LoadingContext, &MarbleSpecularPNG, &MarbleSpecular);
    Rr_LoadMeshFromOBJ(LoadingContext, &MarbleOBJ, &MarbleMesh);
    Rr_LoadColorImageFromPNG(LoadingContext, &CottagePNG, &CottageTexture);
    Rr_LoadMeshFromOBJ(LoadingContext, &CottageOBJ, &CottageMesh);
    Rr_LoadAsync(LoadingContext, OnLoadingComplete, Renderer);

    // Rr_ExternAsset(POCDepthEXR);
    // SceneDepthImage = Rr_CreateDepthImageFromEXR(&POCDepthEXR, Renderer);
    //
    // Rr_ExternAsset(POCColorPNG);
    // SceneColorImage = Rr_CreateImageFromPNG(Renderer, &POCColorPNG, VK_IMAGE_USAGE_SAMPLED_BIT, false, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    //
    //    Rr_Asset DoorFrameOBJ;
    //    Rr_ExternAssetAs(&DoorFrameOBJ, DoorFrameOBJ);
    //    MonkeyMesh = Rr_CreateMesh_FromOBJ(Renderer, &DoorFrameOBJ);

    // Rr_ExternAsset(PocMeshOBJ);
    // PocMesh = Rr_CreateMeshFromOBJ(Renderer, &PocMeshOBJ);
    //
    // Rr_ExternAsset(PocDiffusePNG);
    // PocDiffuseImage = Rr_CreateImageFromPNG(Renderer, &PocDiffusePNG, VK_IMAGE_USAGE_SAMPLED_BIT, false, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    InitUber3DPipeline(Renderer);
    InitGlobals(Renderer);

    TestString = Rr_CreateString("A quick brown fox @#$ \nNew line test...\n\nA couple of new lines...");
    DebugString = Rr_CreateString("$c3Colored $c1text$c2");
}

static void Cleanup(Rr_App* App)
{
    Rr_Renderer* Renderer = Rr_GetRenderer(App);

    //    for (int Index = 0; Index < RR_FRAME_OVERLAP; Index++)
    //    {
    //        SFrameData* PerFrameData = (SFrameData*)Renderer->PerFrameDatas + Index;
    //    }
    //    SDL_free(Renderer->PerFrameDatas);

    Rr_DestroyMaterial(Renderer, &CottageMaterial);
    Rr_DestroyMaterial(Renderer, &MarbleMaterial);

    // Rr_DestroyImage(Renderer, &SceneDepthImage);
    // Rr_DestroyImage(Renderer, &SceneColorImage);
    Rr_DestroyImage(Renderer, CottageTexture);
    Rr_DestroyImage(Renderer, PocDiffuseImage);
    Rr_DestroyImage(Renderer, MarbleDiffuse);
    Rr_DestroyImage(Renderer, MarbleSpecular);

    Rr_DestroyMesh(Renderer, &CottageMesh);
    Rr_DestroyMesh(Renderer, &PocMesh);
    Rr_DestroyMesh(Renderer, &MarbleMesh);

    Rr_DestroyGenericPipeline(Renderer, Uber3DPipeline);

    Rr_DestroyString(&TestString);
    Rr_DestroyString(&DebugString);
}

static void Update(Rr_App* App)
{
    Rr_InputState InputState = Rr_GetInputState(App);

    Rr_KeyState Up = Rr_GetKeyState(InputState, EIA_UP);
    if (Up == RR_KEYSTATE_PRESSED)
    {
    }

    if (Rr_GetKeyState(InputState, EIA_FULLSCREEN) == RR_KEYSTATE_PRESSED)
    {
        Rr_ToggleFullscreen(App);
    }

    igBegin("RotTest", NULL, ImGuiWindowFlags_AlwaysAutoResize);
    static vec3 CameraPos = { -7.35889f, -4.0f, -6.92579f };
    static vec3 CameraEuler = { 90.0f - 63.5593f, -46.6919f, 0.0f };
    static vec3 LightDirEuler = { 90.0f - 37.261f, 99.6702f, 3.16371f };
    igSliderFloat3("CameraPos", CameraPos, -8.0f, 8.0f, "%f", ImGuiSliderFlags_None);
    igSliderFloat3("CameraRot", CameraEuler, -190.0f, 190.0f, "%f", ImGuiSliderFlags_None);
    igSliderFloat3("LightDir", LightDirEuler, -100.0f, 100.0f, "%f", ImGuiSliderFlags_None);

    mat4 LightDirTemp;
    glm_euler_xyz((vec3){ glm_rad(LightDirEuler[0]), glm_rad(LightDirEuler[1]), glm_rad(LightDirEuler[2]) }, LightDirTemp);
    glm_vec3_copy(LightDirTemp[2], ShaderGlobals.DirectionalLightDirection);

    mat4 Temp;
    glm_mat4_identity(Temp);
    glm_vec3_copy(CameraPos, Temp[3]);

    mat4 Temp2;
    glm_euler_xyz((vec3){ glm_rad(CameraEuler[0]), glm_rad(CameraEuler[1]), glm_rad(CameraEuler[2]) }, Temp2);

    glm_mat4_mul(Temp2, Temp, ShaderGlobals.View);
    igEnd();

    static bool bShowDebugOverlay = false;
    if (Rr_GetKeyState(InputState, EIA_DEBUGOVERLAY) == RR_KEYSTATE_PRESSED)
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
    Rr_Renderer* Renderer = Rr_GetRenderer(App);

    glm_perspective_resize(Rr_GetAspectRatio(Renderer), ShaderGlobals.Proj);

    /* Rendering */
    Rr_BeginRenderingInfo BeginRenderingInfo = {
        .Pipeline = Uber3DPipeline,
        //        .InitialDepth = &SceneDepthImage,
        //        .InitialColor = &SceneColorImage,
        .GlobalsData = &ShaderGlobals
    };
    Rr_RenderingContext RenderingContext = Rr_BeginRendering(Renderer, &BeginRenderingInfo);

    const u64 Ticks = SDL_GetTicks();
    const f32 Time = (float)((double)Ticks / 1000.0 * 2);

    std::string LoadingString;
    Rr_String RrLoadingString = {};
    if (LoadingContext != nullptr)
    {
        u32 Current, Total;
        Rr_GetLoadProgress(LoadingContext, &Current, &Total);
        LoadingString = std::format("Loading: {}/{}\n", Current, Total);

        RrLoadingString = Rr_CreateString(LoadingString.c_str());
    }

    if (bLoaded)
    {
        mat4 Transform;

        SUber3DDraw CottageDraw;
        glm_euler_xyz((vec3){ 0.0f, SDL_fmodf(Time, SDL_PI_F * 2.0f), 0.0f }, Transform);
        glm_mat4_identity(CottageDraw.Model);
        glm_scale_uni(CottageDraw.Model, 0.75f);
        glm_mat4_mul(Transform, CottageDraw.Model, CottageDraw.Model);
        CottageDraw.Model[3][0] = 3.5f;
        CottageDraw.Model[3][1] = 0.5f;
        CottageDraw.Model[3][2] = 3.5f;
        Rr_DrawMesh(&RenderingContext, &CottageMaterial, &CottageMesh, &CottageDraw);

        SUber3DDraw MarbleDraw;
        glm_mat4_identity(MarbleDraw.Model);
        MarbleDraw.Model[3][1] = 0.1f;
        Rr_DrawMesh(&RenderingContext, &MarbleMaterial, &MarbleMesh, &MarbleDraw);

        Rr_DrawDefaultText(&RenderingContext, &TestString, vec2{ 50.0f, 50.0f });
        Rr_DrawCustomText(
            &RenderingContext,
            nullptr,
            &DebugString,
            vec2{ 450.0f, 54.0f },
            28.0f,
            RR_DRAW_TEXT_FLAGS_NONE_BIT);
    }
    else
    {
        Rr_DrawCustomText(
            &RenderingContext,
            nullptr,
            &RrLoadingString,
            vec2{ 25.0f, 540.0f - 25 - 32.0f },
            32.0f,
            RR_DRAW_TEXT_FLAGS_ANIMATION_BIT);
    }

    Rr_EndRendering(&RenderingContext);

    Rr_DestroyString(&RrLoadingString);
}

static void OnFileDropped(Rr_App* App, const char* Path)
{
    HandleFileDrop(Path);
}

void RunGame()
{
    Rr_InputConfig InputConfig = {
        .Mappings = InputMappings,
        .Count = EIA_COUNT
    };
    Rr_AppConfig Config = {
        .Title = "VulkanPlayground",
        .ReferenceResolution = { 1920 / 2, 1080 / 2 },
        .InputConfig = &InputConfig,
        .PerFrameDataSize = sizeof(SFrameData),
        .InitFunc = Init,
        .CleanupFunc = Cleanup,
        .UpdateFunc = Update,
        .DrawFunc = Draw,

        .FileDroppedFunc = OnFileDropped
    };
    Rr_Run(&Config);
}
