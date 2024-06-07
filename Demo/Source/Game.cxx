#include "Game.hxx"

#include "DevTools.hxx"
#include "DemoAssets.inc"

#include <imgui/imgui.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_scancode.h>

#include <Rr/Rr.h>

#include <string>
#include <format>
#include <array>

typedef struct SUber3DGlobals
{
    Rr_Mat4 View;
    Rr_Mat4 Intermediate;
    Rr_Mat4 Proj;
    Rr_Vec4 AmbientLightColor;
    Rr_Vec4 DirectionalLightDirection;
    Rr_Vec4 DirectionalLightColor;
    Rr_Vec4 DirectionalLightIntensity;
} SUber3DGlobals;

typedef struct SUber3DMaterial
{
    Rr_Vec3 Emissive;
} SUber3DMaterial;

typedef struct SUber3DDraw
{
    Rr_Mat4 Model;
} SUber3DDraw;

typedef struct SUber3DPushConstants
{
    Rr_Mat4 Reserved;
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

static Rr_StaticMesh* CottageMesh;
static Rr_Image* CottageTexture;
static Rr_Material* CottageMaterial;

static Rr_StaticMesh* PocMesh;
static Rr_Image* PocDiffuseImage;

static Rr_Material* MarbleMaterial;
static Rr_Image* MarbleDiffuse;
static Rr_Image* MarbleNormal;
static Rr_Image* MarbleSpecular;
static Rr_StaticMesh* MarbleMesh;

static Rr_StaticMesh* AvocadoMesh;

static Rr_String LoadingString;
static Rr_String TestString;
static Rr_String DebugString;
static Rr_LoadingContext* LoadingContext;

static bool bLoaded = false;

static void InitInputMappings()
{
    InputMappings[EIA_UP].Primary = SDL_SCANCODE_W;
    InputMappings[EIA_DOWN].Primary = SDL_SCANCODE_S;
    InputMappings[EIA_LEFT].Primary = SDL_SCANCODE_A;
    InputMappings[EIA_RIGHT].Primary = SDL_SCANCODE_D;
    InputMappings[EIA_FULLSCREEN].Primary = SDL_SCANCODE_F11;
    InputMappings[EIA_DEBUGOVERLAY].Primary = SDL_SCANCODE_F1;
    InputMappings[EIA_TEST].Primary = SDL_SCANCODE_F2;
}

static void InitUber3DPipeline(Rr_App* App)
{
    Rr_Asset Uber3DVERT = Rr_LoadAsset(DEMO_ASSET_UBER3D_VERT_SPV);
    Rr_Asset Uber3DFRAG = Rr_LoadAsset(DEMO_ASSET_UBER3D_FRAG_SPV);

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
        App,
        Builder,
        { sizeof(SUber3DGlobals),
            sizeof(SUber3DMaterial),
            sizeof(SUber3DDraw) });
}

static void InitGlobals(Rr_App* App)
{
    ShaderGlobals.AmbientLightColor = { 0.1f, 0.1f, 0.1f, 1.0f };
    ShaderGlobals.DirectionalLightColor = { 1.0f, 0.95f, 0.93f, 1.0f };
    ShaderGlobals.DirectionalLightIntensity = { 1.0f, 1.0f, 1.0f, 1.0f };

    ShaderGlobals.Proj = Rr_Perspective(0.7643276f, Rr_GetAspectRatio(App));
    ShaderGlobals.Intermediate = Rr_VulkanMatrix();
}

static void OnLoadingComplete(Rr_App* App, const void* Userdata)
{
    Rr_Image* MarbleTextures[2] = { MarbleDiffuse, MarbleSpecular };
    MarbleMaterial = Rr_CreateMaterial(App, Uber3DPipeline, MarbleTextures, 2);

    Rr_Image* CottageTextures[1] = { CottageTexture };
    CottageMaterial = Rr_CreateMaterial(App, Uber3DPipeline, CottageTextures, 1);

    bLoaded = true;

    LoadingContext = nullptr;
}

static void Init(Rr_App* App)
{
#if RR_DEBUG
    Rr_Array_Test();
#endif
    InitInputMappings();

    InitUber3DPipeline(App);
    InitGlobals(App);

    Rr_GLTFLoader GLTFLoader = {};
    GLTFLoader.GenericPipeline = Uber3DPipeline;
    GLTFLoader.BaseTexture = 0;
    GLTFLoader.NormalTexture = 1;
    GLTFLoader.SpecularTexture = 2;

    std::array LoadTasks = {
        Rr_LoadColorImageFromPNG(DEMO_ASSET_MARBLEDIFFUSE_PNG, &MarbleDiffuse),
        Rr_LoadColorImageFromPNG(DEMO_ASSET_MARBLESPECULAR_PNG, &MarbleSpecular),
        Rr_LoadColorImageFromPNG(DEMO_ASSET_COTTAGE_PNG, &CottageTexture),
        Rr_LoadStaticMeshFromGLTF(DEMO_ASSET_AVOCADO_GLB, &GLTFLoader, 0, &AvocadoMesh),
        Rr_LoadStaticMeshFromOBJ(DEMO_ASSET_MARBLE_OBJ, &MarbleMesh),
        Rr_LoadStaticMeshFromOBJ(DEMO_ASSET_COTTAGE_OBJ, &CottageMesh),
    };
    LoadingContext = Rr_LoadAsync(App, LoadTasks.data(), LoadTasks.size(), OnLoadingComplete, App);

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

    TestString = Rr_CreateString("A quick brown fox @#$ \nNew line test...\n\nA couple of new lines...");
    DebugString = Rr_CreateString("$c3Colored $c1text$c2");
    LoadingString = Rr_CreateEmptyString(128);
}

static void Cleanup(Rr_App* App)
{
    Rr_DestroyMaterial(App, CottageMaterial);
    Rr_DestroyMaterial(App, MarbleMaterial);

    // Rr_DestroyImage(App, &SceneDepthImage);
    // Rr_DestroyImage(App, &SceneColorImage);
    Rr_DestroyImage(App, CottageTexture);
    Rr_DestroyImage(App, PocDiffuseImage);
    Rr_DestroyImage(App, MarbleDiffuse);
    Rr_DestroyImage(App, MarbleSpecular);

    Rr_DestroyStaticMesh(App, CottageMesh);
    Rr_DestroyStaticMesh(App, PocMesh);
    Rr_DestroyStaticMesh(App, MarbleMesh);
    Rr_DestroyStaticMesh(App, AvocadoMesh);

    Rr_DestroyGenericPipeline(App, Uber3DPipeline);

    Rr_DestroyString(&TestString);
    Rr_DestroyString(&DebugString);
    Rr_DestroyString(&LoadingString);
}

static void Iterate(Rr_App* App)
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

    ImGui::Begin("RotTest", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    static Rr_Vec3 CameraPos = { -7.35889f, -4.0f, -6.92579f };
    static Rr_Vec3 CameraEuler = { 90.0f - 63.5593f, -46.6919f, 0.0f };
    static Rr_Vec3 LightDirEuler = { 90.0f - 37.261f, 99.6702f, 3.16371f };
    ImGui::SliderFloat3("CameraPos", CameraPos.Elements, -8.0f, 8.0f, "%f", ImGuiSliderFlags_None);
    ImGui::SliderFloat3("CameraRot", CameraEuler.Elements, -190.0f, 190.0f, "%f", ImGuiSliderFlags_None);
    ImGui::SliderFloat3("LightDir", LightDirEuler.Elements, -100.0f, 100.0f, "%f", ImGuiSliderFlags_None);

    Rr_Vec3 LightRotation = LightDirEuler * Rr_DegToRad;
    ShaderGlobals.DirectionalLightDirection = Rr_EulerXYZ(LightRotation).Columns[2];

    Rr_Vec3 CameraRotation = CameraEuler * Rr_DegToRad;
    ShaderGlobals.View = Rr_EulerXYZ(CameraRotation) * Rr_Translate(CameraPos);
    ImGui::End();

    static bool bShowDebugOverlay = false;
    if (Rr_GetKeyState(InputState, EIA_DEBUGOVERLAY) == RR_KEYSTATE_PRESSED)
    {
        bShowDebugOverlay = !bShowDebugOverlay;
    }
    if (bShowDebugOverlay)
    {
        Rr_DebugOverlay(App);
    }

    Rr_PerspectiveResize(Rr_GetAspectRatio(App), &ShaderGlobals.Proj);

    /* Rendering */
    Rr_DrawContextInfo BeginRenderingInfo = {
        .DrawTarget = nullptr,
        .InitialColor = nullptr,
        .InitialDepth = nullptr,
        .Sizes = Rr_GetGenericPipelineSizes(Uber3DPipeline)
    };
    Rr_DrawContext* RenderingContext = Rr_CreateDrawContext(App, &BeginRenderingInfo, (byte*)&ShaderGlobals);

    const u64 Ticks = SDL_GetTicks();
    const f32 Time = (f32)((f64)Ticks / 1000.0 * 2);

    if (bLoaded)
    {
        SUber3DDraw CottageDraw = { 0 };
        CottageDraw.Model =
            Rr_Scale({ 0.75f, 0.75f, 0.75f })
            * Rr_Rotate_LH(SDL_fmodf(Time, SDL_PI_F * 2.0f), { 0.0f, 1.0f, 0.0f })
            * Rr_Translate({ 3.5f, 0.5f, 3.5f });
        CottageDraw.Model[3][0] = 3.5f;
        CottageDraw.Model[3][1] = 0.5f;
        CottageDraw.Model[3][2] = 3.5f;
        // Rr_DrawStaticMeshOverrideMaterials(RenderingContext, &CottageMaterial, 1, CottageMesh, Rr_MakeData(CottageDraw));
        Rr_DrawStaticMeshOverrideMaterials(RenderingContext, &CottageMaterial, 1, AvocadoMesh, Rr_MakeData(CottageDraw));

        SUber3DDraw MarbleDraw = { 0 };
        MarbleDraw.Model = Rr_Translate({ 0.0f, 0.1f, 0.0f });
        Rr_DrawStaticMeshOverrideMaterials(RenderingContext, &MarbleMaterial, 1, MarbleMesh, Rr_MakeData(MarbleDraw));

        Rr_DrawDefaultText(RenderingContext, &TestString, { 50.0f, 50.0f });

        Rr_DrawCustomText(
            RenderingContext,
            nullptr,
            &DebugString,
            { 450.0f, 54.0f },
            28.0f,
            RR_DRAW_TEXT_FLAGS_NONE_BIT);
    }
    else
    {
        if (LoadingContext != nullptr)
        {
            u32 Current, Total;
            Rr_GetLoadProgress(LoadingContext, &Current, &Total);

            std::string Formatted = std::format("Loading: {}/{}\n", Current, Total);
            Rr_SetString(&LoadingString, Formatted.c_str(), Formatted.length());
        }

        Rr_DrawCustomText(
            RenderingContext,
            nullptr,
            &LoadingString,
            { 25.0f, 540.0f - 25 - 32.0f },
            32.0f,
            RR_DRAW_TEXT_FLAGS_ANIMATION_BIT);
    }
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
        .Title = "RrDemo",
        .InputConfig = &InputConfig,
        .InitFunc = Init,
        .CleanupFunc = Cleanup,
        .IterateFunc = Iterate,
        .FileDroppedFunc = OnFileDropped
    };
    Rr_Run(&Config);
}
