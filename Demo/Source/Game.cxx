#include "Game.hxx"

#include "DevTools.hxx"
#include "DemoAssets.inc"
#include "Rr/Rr_Draw.h"

#include <imgui/imgui.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_scancode.h>

#include <Rr/Rr.h>

#include <string>
#include <format>
#include <array>

struct SUber3DGlobals
{
    Rr_Mat4 View;
    Rr_Mat4 Intermediate;
    Rr_Mat4 Proj;
    Rr_Vec4 AmbientLightColor;
    Rr_Vec4 DirectionalLightDirection;
    Rr_Vec4 DirectionalLightColor;
    Rr_Vec4 DirectionalLightIntensity;
};

enum EInputAction
{
    EIA_UP,
    EIA_DOWN,
    EIA_LEFT,
    EIA_RIGHT,
    EIA_FULLSCREEN,
    EIA_DEBUGOVERLAY,
    EIA_TEST,
    EIA_COUNT,
};

struct SEntity
{
};

template <typename TGlobals, typename TMaterial, typename TDraw>
struct TPipeline
{
protected:
    Rr_App* App;

public:
    using SGlobals = TGlobals;
    using SMaterial = TMaterial;
    using SDraw = TDraw;

    Rr_GenericPipeline* GenericPipeline{};
    Rr_GLTFLoader GLTFLoader{};

    explicit TPipeline(Rr_App* App)
        : App(App)
    {
    }

    ~TPipeline()
    {
        Rr_DestroyGenericPipeline(App, GenericPipeline);
    }

    TPipeline(TPipeline&& Rhs) = delete;
    TPipeline& operator=(TPipeline&& Rhs) = delete;
    TPipeline(const TPipeline& Rhs) = delete;
    TPipeline& operator=(const TPipeline& Rhs) = delete;
};

struct SUnlitGlobals
{
    Rr_Mat4 View;
    Rr_Mat4 Intermediate;
    Rr_Mat4 Proj;
};

struct SUnlitMaterial
{
    Rr_Vec3 Emissive;
};

struct SUnlitDraw
{
    Rr_Mat4 Model;
};

struct SUnlitPipeline : public TPipeline<SUnlitGlobals, SUnlitMaterial, SUnlitDraw>
{
public:
    explicit SUnlitPipeline(Rr_App* App)
        : TPipeline(App)
    {
        Rr_Asset VertexShader = Rr_LoadAsset(DEMO_ASSET_UNLIT_VERT_SPV);
        Rr_Asset FragmentShader = Rr_LoadAsset(DEMO_ASSET_UNLIT_FRAG_SPV);

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
        Rr_EnableVertexStage(Builder, &VertexShader);
        Rr_EnableFragmentStage(Builder, &FragmentShader);
        Rr_EnableDepthTest(Builder);
        Rr_EnableRasterizer(Builder, RR_POLYGON_MODE_FILL);
        GenericPipeline = Rr_BuildGenericPipeline(
            App,
            Builder,
            { sizeof(SGlobals),
                sizeof(SMaterial),
                sizeof(SDraw) });

        GLTFLoader.GenericPipeline = GenericPipeline;
        GLTFLoader.BaseTexture = 0;
        GLTFLoader.NormalTexture = 1;
        GLTFLoader.SpecularTexture = 2;
    }
};

struct SUber3DMaterial
{
    Rr_Vec3 Emissive;
};

struct SUber3DDraw
{
    Rr_Mat4 Model;
};

struct SUber3DPushConstants
{
    Rr_Mat4 Reserved;
};

struct SUber3DPipeline : public TPipeline<SUber3DGlobals, SUber3DMaterial, SUber3DDraw>
{
public:
    explicit SUber3DPipeline(Rr_App* App)
        : TPipeline(App)
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
        GenericPipeline = Rr_BuildGenericPipeline(
            App,
            Builder,
            { sizeof(SGlobals),
                sizeof(SMaterial),
                sizeof(SDraw) });

        GLTFLoader.GenericPipeline = GenericPipeline;
        GLTFLoader.BaseTexture = 0;
        GLTFLoader.NormalTexture = 1;
        GLTFLoader.SpecularTexture = 2;
    }
};

struct SCamera
{
    f32 Pitch{};
    f32 Yaw{};
    Rr_Vec3 Position{};
    Rr_Mat4 ViewMatrix = Rr_M4D(1.0f);

    [[nodiscard]] Rr_Vec3 GetForwardVector() const
    {
        return Rr_Norm(Rr_InvGeneral(ViewMatrix).Columns[2].XYZ);
    }

    [[nodiscard]] Rr_Vec3 GetRightVector() const
    {
        return Rr_Norm(Rr_InvGeneral(ViewMatrix).Columns[0].XYZ);
    }

    [[nodiscard]] Rr_Mat4 GetViewMatrix()
    {
        f32 CosPitch = cos(Pitch * Rr_DegToRad);
        f32 SinPitch = sin(Pitch * Rr_DegToRad);
        f32 CosYaw = cos(Yaw * Rr_DegToRad);
        f32 SinYaw = sin(Yaw * Rr_DegToRad);

        Rr_Vec3 XAxis{ CosYaw, 0.0f, -SinYaw };
        Rr_Vec3 YAxis{ SinYaw * SinPitch, CosPitch, CosYaw * SinPitch };
        Rr_Vec3 ZAxis{ SinYaw * CosPitch, -SinPitch, CosPitch * CosYaw };

        ViewMatrix = {
            XAxis.X,
            YAxis.X,
            ZAxis.X,
            0.0f,
            XAxis.Y,
            YAxis.Y,
            ZAxis.Y,
            0.0f,
            XAxis.Z,
            YAxis.Z,
            ZAxis.Z,
            0.0f,
            -Rr_Dot(XAxis, Position),
            -Rr_Dot(YAxis, Position),
            -Rr_Dot(ZAxis, Position),
            1.0f,
        };

        return ViewMatrix;
    }
};

struct SGame
{
private:
    Rr_App* App{};

    Rr_InputMapping InputMappings[RR_MAX_INPUT_MAPPINGS]{};

    SUber3DGlobals ShaderGlobals{};

    SUber3DPipeline Uber3DPipeline;
    SUnlitPipeline UnlitPipeline;

    // Rr_Image SceneDepthImage;
    // Rr_Image SceneColorImage;

    Rr_StaticMesh* ArrowMesh{};

    Rr_StaticMesh* CottageMesh{};
    Rr_Image* CottageDiffuse{};
    Rr_Image* CottageNormal{};
    Rr_Material* CottageMaterial{};

    Rr_StaticMesh* PocMesh{};
    Rr_Image* PocDiffuseImage{};

    Rr_StaticMesh* MarbleMesh{};

    Rr_StaticMesh* AvocadoMesh{};

    Rr_String LoadingString{};
    Rr_String TestString{};
    Rr_String DebugString{};
    Rr_LoadingContext* LoadingContext{};

    Rr_DrawTarget* ShadowMap{};

    bool bLoaded = false;

public:
    void InitInputMappings()
    {
        InputMappings[EIA_UP].Primary = SDL_SCANCODE_W;
        InputMappings[EIA_DOWN].Primary = SDL_SCANCODE_S;
        InputMappings[EIA_LEFT].Primary = SDL_SCANCODE_A;
        InputMappings[EIA_RIGHT].Primary = SDL_SCANCODE_D;
        InputMappings[EIA_FULLSCREEN].Primary = SDL_SCANCODE_F11;
        InputMappings[EIA_DEBUGOVERLAY].Primary = SDL_SCANCODE_F1;
        InputMappings[EIA_TEST].Primary = SDL_SCANCODE_F2;

        Rr_InputConfig InputConfig = {
            .Mappings = InputMappings,
            .Count = EIA_COUNT
        };

        Rr_SetInputConfig(App, &InputConfig);
    }

    void InitGlobals()
    {
        ShaderGlobals.AmbientLightColor = { 0.01f, 0.01f, 0.01f, 1.0f };
        ShaderGlobals.DirectionalLightColor = { 1.0f, 0.95f, 0.93f, 1.0f };
        ShaderGlobals.DirectionalLightIntensity = { 1.0f, 1.0f, 1.0f, 1.0f };

        ShaderGlobals.Proj = Rr_Perspective_LH_ZO(0.7643276f, Rr_GetAspectRatio(App), 0.5f, 50.0f);
        ShaderGlobals.Intermediate = Rr_VulkanMatrix();
    }

    void OnLoadingComplete()
    {
        std::array CottageTextures = { CottageDiffuse, CottageNormal };
        CottageMaterial = Rr_CreateMaterial(App, Uber3DPipeline.GenericPipeline, CottageTextures.data(), CottageTextures.size());

        bLoaded = true;

        LoadingContext = nullptr;
    }

    static void OnLoadingComplete(Rr_App* App, void* Userdata)
    {
        auto* Game = reinterpret_cast<SGame*>(Rr_GetUserData(App));
        Game->OnLoadingComplete();
    }

    void Iterate()
    {
        Rr_InputState InputState = Rr_GetInputState(App);
        Rr_KeyStates Keys = InputState.Keys;

        f32 DeltaTime = Rr_GetDeltaTime(App);

        if (Rr_GetKeyState(Keys, EIA_FULLSCREEN) == RR_KEYSTATE_PRESSED)
        {
            Rr_ToggleFullscreen(App);
        }

        ImGui::Begin("RotTest", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        static SCamera Camera = {
            .Pitch = 90.0f - 63.5593f,
            .Yaw = -46.6919f,
            .Position = { -7.35889f, 4.0f, -6.92579f }
        };

        // static Rr_Vec3 LightDirEuler = { 90.0f - 37.261f, 99.6702f, 3.16371f };
        static Rr_Vec3 LightDirEuler = { -100.0f, 18.0f, 3.16371f };
        ImGui::SliderFloat2("CameraPitchYaw", &Camera.Pitch, -360.0f, 360.0f, "%f", ImGuiSliderFlags_None);
        ImGui::SliderFloat3("CameraPos", Camera.Position.Elements, -100.0f, 100.0f, "%.3f", ImGuiSliderFlags_None);
        ImGui::SliderFloat3("LightDir", LightDirEuler.Elements, -100.0f, 100.0f, "%f", ImGuiSliderFlags_None);

        Rr_Vec3 LightRotation = LightDirEuler * Rr_DegToRad;
        ShaderGlobals.DirectionalLightDirection = Rr_EulerXYZ(LightRotation).Columns[2];

        Rr_Vec3 CameraForward = Camera.GetForwardVector();
        Rr_Vec3 CameraLeft = Camera.GetRightVector();
        constexpr f32 CameraSpeed = 0.1f;
        if (Rr_GetKeyState(Keys, EIA_UP) == RR_KEYSTATE_HELD)
        {
            Camera.Position -= CameraForward * DeltaTime * CameraSpeed;
        }
        if (Rr_GetKeyState(Keys, EIA_LEFT) == RR_KEYSTATE_HELD)
        {
            Camera.Position -= CameraLeft * DeltaTime * CameraSpeed;
        }
        if (Rr_GetKeyState(Keys, EIA_DOWN) == RR_KEYSTATE_HELD)
        {
            Camera.Position += CameraForward * DeltaTime * CameraSpeed;
        }
        if (Rr_GetKeyState(Keys, EIA_RIGHT) == RR_KEYSTATE_HELD)
        {
            Camera.Position += CameraLeft * DeltaTime * CameraSpeed;
        }

        if (InputState.MouseState & SDL_BUTTON_RMASK)
        {
            SDL_SetRelativeMouseMode(SDL_TRUE);
            constexpr f32 Sensitivity = 0.12f;
            Camera.Yaw = Rr_WrapMax(Camera.Yaw - (InputState.MousePositionDelta.X *  Sensitivity), 360.0f);
            Camera.Pitch = Rr_WrapMinMax(Camera.Pitch - (InputState.MousePositionDelta.Y *  Sensitivity), -90.0f, 90.0f);
        }
        else
        {
            SDL_SetRelativeMouseMode(SDL_FALSE);
        }

        ShaderGlobals.View = Camera.GetViewMatrix();

        ImGui::End();

        static bool bShowDebugOverlay = false;
        if (Rr_GetKeyState(Keys, EIA_DEBUGOVERLAY) == RR_KEYSTATE_PRESSED)
        {
            bShowDebugOverlay = !bShowDebugOverlay;
        }
        if (bShowDebugOverlay)
        {
            Rr_DebugOverlay(App);
        }

        Rr_PerspectiveResize(Rr_GetAspectRatio(App), &ShaderGlobals.Proj);

        /* Rendering */
        Rr_DrawContextInfo DrawContextInfo = {
            .DrawTarget = nullptr,
            .InitialColor = nullptr,
            .InitialDepth = nullptr,
            .Viewport = {},
            .Sizes = Rr_GetGenericPipelineSizes(Uber3DPipeline.GenericPipeline),
        };
        Rr_DrawContext* DrawContext = Rr_CreateDrawContext(App, &DrawContextInfo, (byte*)&ShaderGlobals);

        const u64 Ticks = SDL_GetTicks();
        const f32 Time = (f32)((f64)Ticks / 1000.0 * 2);

        if (bLoaded)
        {
            SUnlitPipeline::SDraw ArrowDraw = { 0 };
            ArrowDraw.Model = Rr_EulerXYZ(LightRotation);
            ArrowDraw.Model[3][1] = 5.0f;
            Rr_DrawStaticMesh(DrawContext, ArrowMesh, Rr_MakeData(ArrowDraw));

            SUber3DDraw CottageDraw = { 0 };
            CottageDraw.Model = Rr_Scale({ 1.f, 1.f, 1.f });
            CottageDraw.Model[3][1] = 0.1f;
            Rr_DrawStaticMeshOverrideMaterials(DrawContext, &CottageMaterial, 1, CottageMesh, Rr_MakeData(CottageDraw));

            SUber3DDraw AvocadoDraw = { 0 };
            AvocadoDraw.Model =
                Rr_Scale({ 0.75f, 0.75f, 0.75f })
                * Rr_Rotate_LH(fmodf(Time, SDL_PI_F * 2.0f), { 0.0f, 1.0f, 0.0f })
                * Rr_Translate({ 3.5f, 0.5f, 3.5f });
            AvocadoDraw.Model[3][0] = 3.5f;
            AvocadoDraw.Model[3][1] = 0.5f;
            AvocadoDraw.Model[3][2] = 3.5f;
            Rr_DrawStaticMesh(DrawContext, AvocadoMesh, Rr_MakeData(AvocadoDraw));

            SUber3DDraw MarbleDraw = { 0 };
            MarbleDraw.Model = Rr_Translate({ 0.0f, 0.1f, 0.0f });
            Rr_DrawStaticMesh(DrawContext, MarbleMesh, Rr_MakeData(MarbleDraw));

            Rr_DrawDefaultText(DrawContext, &TestString, { 50.0f, 50.0f });

            Rr_DrawCustomText(
                DrawContext,
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
                DrawContext,
                nullptr,
                &LoadingString,
                { 25.0f, 540.0f - 25 - 32.0f },
                32.0f,
                RR_DRAW_TEXT_FLAGS_ANIMATION_BIT);
        }
    }

    explicit SGame(Rr_App* App)
        : App(App), Uber3DPipeline(App), UnlitPipeline(App)
    {
        InitInputMappings();

        InitGlobals();

        std::array LoadTasks = {
            Rr_LoadColorImageFromPNG(DEMO_ASSET_COTTAGEDIFFUSE_PNG, &CottageDiffuse),
            Rr_LoadColorImageFromPNG(DEMO_ASSET_COTTAGENORMAL_PNG, &CottageNormal),
            Rr_LoadStaticMeshFromGLTF(DEMO_ASSET_AVOCADO_GLB, &Uber3DPipeline.GLTFLoader, 0, &AvocadoMesh),
            Rr_LoadStaticMeshFromGLTF(DEMO_ASSET_MARBLE_GLB, &Uber3DPipeline.GLTFLoader, 0, &MarbleMesh),
            Rr_LoadStaticMeshFromGLTF(DEMO_ASSET_ARROW_GLB, &UnlitPipeline.GLTFLoader, 0, &ArrowMesh),
            Rr_LoadStaticMeshFromOBJ(DEMO_ASSET_COTTAGE_OBJ, &CottageMesh),
        };
        LoadingContext = Rr_LoadAsync(App, LoadTasks.data(), LoadTasks.size(), OnLoadingComplete, App);

        ShadowMap = Rr_CreateDrawTargetDepthOnly(App, 1024, 1024);

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

    ~SGame()
    {
        Rr_DestroyMaterial(App, CottageMaterial);

        // Rr_DestroyImage(App, &SceneDepthImage);
        // Rr_DestroyImage(App, &SceneColorImage);
        Rr_DestroyImage(App, CottageDiffuse);
        Rr_DestroyImage(App, CottageNormal);
        Rr_DestroyImage(App, PocDiffuseImage);

        Rr_DestroyStaticMesh(App, CottageMesh);
        Rr_DestroyStaticMesh(App, PocMesh);
        Rr_DestroyStaticMesh(App, MarbleMesh);
        Rr_DestroyStaticMesh(App, AvocadoMesh);
        Rr_DestroyStaticMesh(App, ArrowMesh);

        Rr_DestroyString(&TestString);
        Rr_DestroyString(&DebugString);
        Rr_DestroyString(&LoadingString);

        Rr_DestroyDrawTarget(App, ShadowMap);
    }
};

static void Init(Rr_App* App)
{
    auto* Game = new SGame(App);

    Rr_SetUserData(App, Game);
}

static void Iterate(Rr_App* App)
{
    auto* Game = reinterpret_cast<SGame*>(Rr_GetUserData(App));
    Game->Iterate();
}

static void Cleanup(Rr_App* App)
{
    delete reinterpret_cast<SGame*>(Rr_GetUserData(App));
}

static void OnFileDropped(Rr_App* App, const char* Path)
{
    HandleFileDrop(Path);
}

void RunGame()
{
    Rr_AppConfig Config = {
        .Title = "RrDemo",
        .InitFunc = Init,
        .CleanupFunc = Cleanup,
        .IterateFunc = Iterate,
        .FileDroppedFunc = OnFileDropped,
    };
    Rr_Run(&Config);
}
