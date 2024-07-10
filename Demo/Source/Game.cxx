#include "Game.hxx"

#include "DemoAssets.inc"
#include "DevTools.hxx"
#include "Rr/Rr_Graph.h"

#include <imgui/imgui.h>

#include <Rr/Rr.h>

#include <array>
#include <format>
#include <string>

struct SUber3DGlobals
{
    Rr_Mat4 View;
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

template <typename TGlobals, typename TMaterial, typename TPerDraw>
struct TPipeline
{
protected:
    Rr_App *App;

public:
    using SGlobals = TGlobals;
    using SMaterial = TMaterial;
    using SPerDraw = TPerDraw;

    Rr_GenericPipeline *GenericPipeline{};

    explicit TPipeline(Rr_App *InApp) :
        App(InApp)
    {
    }

    ~TPipeline() { Rr_DestroyGenericPipeline(App, GenericPipeline); }

    TPipeline(TPipeline &&Rhs) = delete;
    TPipeline &operator=(TPipeline &&Rhs) = delete;
    TPipeline(const TPipeline &Rhs) = delete;
    TPipeline &operator=(const TPipeline &Rhs) = delete;
};

struct SShadowPassGlobals
{
    Rr_Mat4 View;
    Rr_Mat4 Proj;
};

struct SShadowPassMaterial
{
    Rr_Vec3 Emissive;
};

struct SShadowPerDraw
{
    Rr_Mat4 Model;
};

struct SShadowPassPipeline
    : TPipeline<SShadowPassGlobals, SShadowPassMaterial, SShadowPerDraw>
{
    explicit SShadowPassPipeline(Rr_App *InApp) :
        TPipeline(InApp)
    {
        Rr_Asset VertexShader = Rr_LoadAsset(DEMO_ASSET_SHADOWPASS_VERT_SPV);

        Rr_PipelineBuilder *Builder = Rr_CreatePipelineBuilder();
        Rr_EnableVertexStage(Builder, &VertexShader);
        Rr_EnableDepthTest(Builder);
        Rr_EnableRasterizer(Builder, RR_POLYGON_MODE_FILL);
        GenericPipeline = Rr_BuildGenericPipeline(
            App,
            Builder,
            sizeof(SGlobals),
            sizeof(SMaterial),
            sizeof(SPerDraw));
    }
};

struct SUnlitGlobals
{
    Rr_Mat4 View;
    Rr_Mat4 Proj;
};

struct SUnlitMaterial
{
    Rr_Vec3 Emissive;
};

struct SUnlitPerDraw
{
    Rr_Mat4 Model;
};

struct SUnlitPipeline : TPipeline<SUnlitGlobals, SUnlitMaterial, SUnlitPerDraw>
{
    explicit SUnlitPipeline(Rr_App *InApp) :
        TPipeline(InApp)
    {
        Rr_Asset VertexShader = Rr_LoadAsset(DEMO_ASSET_UNLIT_VERT_SPV);
        Rr_Asset FragmentShader = Rr_LoadAsset(DEMO_ASSET_UNLIT_FRAG_SPV);

        Rr_PipelineBuilder *Builder = Rr_CreatePipelineBuilder();

        Rr_EnableVertexStage(Builder, &VertexShader);
        Rr_EnableFragmentStage(Builder, &FragmentShader);
        Rr_EnableColorAttachment(Builder, false);
        Rr_EnableDepthTest(Builder);
        Rr_EnableRasterizer(Builder, RR_POLYGON_MODE_FILL);
        GenericPipeline = Rr_BuildGenericPipeline(
            App,
            Builder,
            sizeof(SGlobals),
            sizeof(SMaterial),
            sizeof(SPerDraw));
    }
};

struct SUber3DMaterial
{
    Rr_Vec3 Emissive;
};

struct SUber3DPerDraw
{
    Rr_Mat4 Model;
};

struct SUber3DPushConstants
{
    Rr_Mat4 Reserved;
};

struct SUber3DPipeline
    : TPipeline<SUber3DGlobals, SUber3DMaterial, SUber3DPerDraw>
{
    explicit SUber3DPipeline(Rr_App *InApp) :
        TPipeline(InApp)
    {
        Rr_Asset Uber3DVERT = Rr_LoadAsset(DEMO_ASSET_UBER3D_VERT_SPV);
        Rr_Asset Uber3DFRAG = Rr_LoadAsset(DEMO_ASSET_UBER3D_FRAG_SPV);

        Rr_PipelineBuilder *Builder = Rr_CreatePipelineBuilder();
        Rr_EnableVertexStage(Builder, &Uber3DVERT);
        Rr_EnableFragmentStage(Builder, &Uber3DFRAG);
        Rr_EnableColorAttachment(Builder, false);
        Rr_EnableDepthTest(Builder);
        Rr_EnableRasterizer(Builder, RR_POLYGON_MODE_FILL);
        GenericPipeline = Rr_BuildGenericPipeline(
            App,
            Builder,
            sizeof(SGlobals),
            sizeof(SMaterial),
            sizeof(SPerDraw));
    }
};

struct SCamera
{
    float Pitch{};
    float Yaw{};
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
        float CosPitch = cos(Pitch * Rr_DegToRad);
        float SinPitch = sin(Pitch * Rr_DegToRad);
        float CosYaw = cos(Yaw * Rr_DegToRad);
        float SinYaw = sin(Yaw * Rr_DegToRad);

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
    Rr_App *App{};

    std::array<Rr_InputMapping, EIA_COUNT> InputMappings{};

    SUber3DGlobals ShaderGlobals{};

    SUber3DPipeline Uber3DPipeline;
    SUnlitPipeline UnlitPipeline;
    SShadowPassPipeline ShadowPipeline;

    // Rr_Image SceneDepthImage;
    // Rr_Image SceneColorImage;

    Rr_StaticMesh *ArrowMesh{};

    Rr_StaticMesh *CottageMesh{};
    Rr_Image *CottageDiffuse{};
    Rr_Image *CottageNormal{};
    Rr_Material *CottageMaterial{};

    Rr_StaticMesh *PocMesh{};
    Rr_Image *PocDiffuseImage{};

    Rr_StaticMesh *MarbleMesh{};

    Rr_StaticMesh *AvocadoMesh{};

    Rr_String LoadingString{};
    Rr_String TestString{};
    Rr_String DebugString{};
    Rr_LoadingContext *LoadingContext{};

    Rr_DrawTarget *ShadowMap;
    Rr_DrawTarget *TestTarget;

    Rr_GLTFLoader UnlitGLTFLoader;
    Rr_GLTFLoader Uber3DGLTFLoader;

    bool IsLoaded = false;

public:
    void InitInputMappings()
    {
        InputMappings[EIA_UP].Primary = RR_SCANCODE_W;
        InputMappings[EIA_DOWN].Primary = RR_SCANCODE_S;
        InputMappings[EIA_LEFT].Primary = RR_SCANCODE_A;
        InputMappings[EIA_RIGHT].Primary = RR_SCANCODE_D;
        InputMappings[EIA_FULLSCREEN].Primary = RR_SCANCODE_F11;
        InputMappings[EIA_DEBUGOVERLAY].Primary = RR_SCANCODE_F1;
        InputMappings[EIA_TEST].Primary = RR_SCANCODE_F2;

        Rr_InputConfig InputConfig = {
            .Mappings = InputMappings.data(),
            .Count = InputMappings.size(),
        };

        Rr_SetInputConfig(App, &InputConfig);
    }

    void InitGlobals()
    {
        ShaderGlobals.AmbientLightColor = { 0.01f, 0.01f, 0.01f, 1.0f };
        ShaderGlobals.DirectionalLightColor = { 1.0f, 0.95f, 0.93f, 1.0f };
        ShaderGlobals.DirectionalLightIntensity = { 1.0f, 1.0f, 1.0f, 1.0f };

        ShaderGlobals.Proj = Rr_Perspective_LH_ZO(
            0.7643276f,
            Rr_GetAspectRatio(App),
            0.5f,
            50.0f);
    }

    void OnLoadingComplete()
    {
        std::array CottageTextures = { CottageDiffuse, CottageNormal };
        CottageMaterial = Rr_CreateMaterial(
            App,
            Uber3DPipeline.GenericPipeline,
            CottageTextures.data(),
            CottageTextures.size());

        IsLoaded = true;

        LoadingContext = nullptr;
    }

    static void OnLoadingComplete(Rr_App *App, void *UserData)
    {
        auto *Game = static_cast<SGame *>(UserData);
        Game->OnLoadingComplete();
    }

    void DrawScene(Rr_GraphNode *Node)
    {
        const auto Time = static_cast<float>((Rr_GetTimeSeconds(App) * 2.0));

        SUber3DPerDraw CottageDraw = {};
        CottageDraw.Model = Rr_Scale({ 1.f, 1.f, 1.f });
        CottageDraw.Model[3][1] = 0.1f;
        Rr_DrawStaticMeshOverrideMaterials(
            App,
            Node,
            &CottageMaterial,
            1,
            CottageMesh,
            RR_MAKE_DATA(CottageDraw));

        SUber3DPerDraw AvocadoDraw = {};
        AvocadoDraw.Model =
            Rr_Scale({ 0.75f, 0.75f, 0.75f }) *
            Rr_Rotate_LH(fmodf(Time, RR_PI32 * 2.0f), { 0.0f, 1.0f, 0.0f }) *
            Rr_Translate({ 3.5f, 0.5f, 3.5f });
        AvocadoDraw.Model[3][0] = 3.5f;
        AvocadoDraw.Model[3][1] = 0.5f;
        AvocadoDraw.Model[3][2] = 3.5f;
        Rr_DrawStaticMesh(App, Node, AvocadoMesh, RR_MAKE_DATA(AvocadoDraw));

        SUber3DPerDraw MarbleDraw = {};
        MarbleDraw.Model = Rr_Translate({ 0.0f, 0.1f, 0.0f });
        Rr_DrawStaticMesh(App, Node, MarbleMesh, RR_MAKE_DATA(MarbleDraw));
    }

    void Iterate()
    {
        auto [Keys, MousePosition, MousePositionDelta, MouseState] =
            Rr_GetInputState(App);

        auto DeltaTime = static_cast<float>(Rr_GetDeltaSeconds(App));

        if (Rr_GetKeyState(Keys, EIA_FULLSCREEN) == RR_KEYSTATE_PRESSED)
        {
            Rr_ToggleFullscreen(App);
        }

        ImGui::Begin("RotTest", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        static SCamera Camera = {
            .Pitch = 90.0f - 63.5593f,
            .Yaw = -46.6919f,
            .Position = { -7.35889f, 4.0f, -6.92579f },
        };

        // static Rr_Vec3 LightDirEuler = { 90.0f - 37.261f, 99.6702f, 3.16371f
        // };
        static Rr_Vec3 LightDirEuler = { -100.0f, 18.0f, 3.16371f };
        ImGui::SliderFloat2(
            "CameraPitchYaw",
            &Camera.Pitch,
            -360.0f,
            360.0f,
            "%f",
            ImGuiSliderFlags_None);
        ImGui::SliderFloat3(
            "CameraPos",
            Camera.Position.Elements,
            -100.0f,
            100.0f,
            "%.3f",
            ImGuiSliderFlags_None);
        ImGui::SliderFloat3(
            "LightDir",
            LightDirEuler.Elements,
            -100.0f,
            100.0f,
            "%f",
            ImGuiSliderFlags_None);
        ImGui::Separator();
        static Rr_Vec3 ShadowEye = { 3, 3, 3 };
        static Rr_Vec3 ShadowCenter = { 0, 0, 0 };
        ImGui::SliderFloat3(
            "ShadowEye",
            ShadowEye.Elements,
            -10.0f,
            10.0f,
            "%.3f",
            ImGuiSliderFlags_None);
        ImGui::SliderFloat3(
            "ShadowCenter",
            ShadowCenter.Elements,
            -10.0f,
            10.0f,
            "%f",
            ImGuiSliderFlags_None);

        Rr_Vec3 LightRotation = LightDirEuler * Rr_DegToRad;
        ShaderGlobals.DirectionalLightDirection =
            Rr_EulerXYZ(LightRotation).Columns[2];

        Rr_Vec3 CameraForward = Camera.GetForwardVector();
        Rr_Vec3 CameraLeft = Camera.GetRightVector();
        constexpr float CameraSpeed = 0.1f;
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

        if (MouseState & RR_MOUSE_BUTTON_RIGHT_MASK)
        {
            Rr_SetRelativeMouseMode(true);
            constexpr float Sensitivity = 0.2f;
            Camera.Yaw = Rr_WrapMax(
                Camera.Yaw - (MousePositionDelta.X * Sensitivity),
                360.0f);
            Camera.Pitch = Rr_WrapMinMax(
                Camera.Pitch - (MousePositionDelta.Y * Sensitivity),
                -90.0f,
                90.0f);
        }
        else
        {
            Rr_SetRelativeMouseMode(false);
        }

        ShaderGlobals.View = Rr_VulkanMatrix() * Camera.GetViewMatrix();

        ImGui::End();

        static bool ShowDebugOverlay = false;
        if (Rr_GetKeyState(Keys, EIA_DEBUGOVERLAY) == RR_KEYSTATE_PRESSED)
        {
            ShowDebugOverlay = !ShowDebugOverlay;
        }
        if (ShowDebugOverlay)
        {
            Rr_DebugOverlay(App);
        }

        Rr_PerspectiveResize(Rr_GetAspectRatio(App), &ShaderGlobals.Proj);

        /* Create Nodes */

        Rr_GraphicsNodeInfo TestNodeInfo = {
            .Name = "test_pass",
            .DrawTarget = TestTarget,
            .InitialColor = nullptr,
            .InitialDepth = nullptr,
            .Viewport = { 0, 0, 1024, 1024 },
            .BasePipeline = Uber3DPipeline.GenericPipeline,
            .OverridePipeline = nullptr,
        };
        SUber3DGlobals TestGlobals = ShaderGlobals;
        TestGlobals.View =
            Rr_LookAt_LH(ShadowEye, ShadowCenter, { 0.0, 1.0f, 0.0f }),
        TestGlobals.Proj = Rr_Orthographic_LH_ZO(
            -512 * 0.05f,
            512 * 0.05f,
            -512 * 0.05f,
            512 * 0.05f,
            -1000.0f,
            1000.0f);
        Rr_GraphNode *TestNode = Rr_AddGraphicsNode(
            App,
            &TestNodeInfo,
            reinterpret_cast<char *>(&TestGlobals),
            nullptr,
            0);

        Rr_GraphicsNodeInfo ShadowNodeInfo = {
            .Name = "shadow_pass",
            .DrawTarget = ShadowMap,
            .InitialColor = nullptr,
            .InitialDepth = nullptr,
            .Viewport = { 0, 0, 1024, 1024 },
            .BasePipeline = ShadowPipeline.GenericPipeline,
            .OverridePipeline = ShadowPipeline.GenericPipeline,
        };
        SShadowPassPipeline::SGlobals ShadowGlobals = {
            .View = Rr_LookAt_LH(ShadowEye, ShadowCenter, { 0.0, 1.0f, 0.0f }),
            .Proj = Rr_Orthographic_LH_ZO(
                -512 * 0.05f,
                512 * 0.05f,
                -512 * 0.05f,
                512 * 0.05f,
                -1000.0f,
                1000.0f),
        };
        Rr_GraphNode *ShadowNode = Rr_AddGraphicsNode(
            App,
            &ShadowNodeInfo,
            reinterpret_cast<char *>(&ShadowGlobals),
            nullptr,
            0);

        std::array NodeDependencies = { TestNode, ShadowNode };
        // std::array<Rr_GraphNode *, 0> NodeDependencies{};

        Rr_GraphicsNodeInfo NodeInfo = {
            .Name = "pbr_pass",
            .DrawTarget = nullptr,
            .InitialColor = nullptr,
            .InitialDepth = nullptr,
            .Viewport = {},
            .BasePipeline = Uber3DPipeline.GenericPipeline,
            .OverridePipeline = nullptr,
        };
        Rr_GraphNode *Node = Rr_AddGraphicsNode(
            App,
            &NodeInfo,
            reinterpret_cast<char *>(&ShaderGlobals),
            NodeDependencies.data(),
            NodeDependencies.size());

        Rr_GraphNode *BuiltinNode = Rr_AddBuiltinNode(App, &Node, 1);

        Rr_PresentNodeInfo PresentInfo = {
            .Name = "present",
            .Mode = RR_PRESENT_MODE_STRETCH,
        };

        /* @TODO: Add dependencies later! */
        Rr_AddPresentNode(App, &PresentInfo, &BuiltinNode, 1);

        if (IsLoaded)
        {
            DrawScene(ShadowNode);
            DrawScene(TestNode);

            SUnlitPipeline::SPerDraw ArrowDraw = {};
            ArrowDraw.Model = Rr_EulerXYZ(LightRotation);
            ArrowDraw.Model[3][1] = 5.0f;
            Rr_DrawStaticMesh(App, Node, ArrowMesh, RR_MAKE_DATA(ArrowDraw));

            DrawScene(Node);

            Rr_DrawDefaultText(App, BuiltinNode, &TestString, { 50.0f, 50.0f });

            Rr_DrawCustomText(
                App,
                BuiltinNode,
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
                uint32_t Current, Total;
                Rr_GetLoadProgress(LoadingContext, &Current, &Total);

                std::string Formatted =
                    std::format("Загружайу: {}/{}\n", Current, Total);
                Rr_SetString(
                    &LoadingString,
                    Formatted.c_str(),
                    Formatted.length());
            }

            Rr_DrawCustomText(
                App,
                BuiltinNode,
                nullptr,
                &LoadingString,
                { 25.0f, 540.0f - 25 - 32.0f },
                32.0f,
                RR_DRAW_TEXT_FLAGS_ANIMATION_BIT);
        }
    }

    explicit SGame(Rr_App *InApp) :
        App(InApp),
        Uber3DPipeline(App),
        UnlitPipeline(App),
        ShadowPipeline(App),
        ShadowMap(Rr_CreateDrawTargetDepthOnly(App, 1024, 1024)),
        TestTarget(Rr_CreateDrawTarget(App, 1024, 1024)),
        UnlitGLTFLoader({
            .GenericPipeline = UnlitPipeline.GenericPipeline,
            .BaseTexture = 0,
            .NormalTexture = 1,
            .SpecularTexture = 2,
        }),
        Uber3DGLTFLoader({
            .GenericPipeline = Uber3DPipeline.GenericPipeline,
            .BaseTexture = 0,
            .NormalTexture = 1,
            .SpecularTexture = 2,
        })
    {
        InitInputMappings();

        InitGlobals();

        std::array LoadTasks = {
            Rr_LoadColorImageFromPNG(
                DEMO_ASSET_COTTAGEDIFFUSE_PNG,
                &CottageDiffuse),
            Rr_LoadColorImageFromPNG(
                DEMO_ASSET_COTTAGENORMAL_PNG,
                &CottageNormal),
            Rr_LoadStaticMeshFromGLTF(
                DEMO_ASSET_AVOCADO_GLB,
                &Uber3DGLTFLoader,
                0,
                &AvocadoMesh),
            Rr_LoadStaticMeshFromGLTF(
                DEMO_ASSET_MARBLE_GLB,
                &Uber3DGLTFLoader,
                0,
                &MarbleMesh),
            Rr_LoadStaticMeshFromGLTF(
                DEMO_ASSET_ARROW_GLB,
                &UnlitGLTFLoader,
                0,
                &ArrowMesh),
            Rr_LoadStaticMeshFromOBJ(DEMO_ASSET_COTTAGE_OBJ, &CottageMesh),
        };
        LoadingContext = Rr_LoadAsync(
            App,
            LoadTasks.data(),
            LoadTasks.size(),
            OnLoadingComplete,
            this);

        TestString = Rr_CreateString("A quick brown fox @#$ \nNew line "
                                     "test...\n\nA couple of new lines...");
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
        Rr_DestroyDrawTarget(App, TestTarget);
    }
};

static void Init(Rr_App *App, void *UserData) { new (UserData) SGame(App); }

static void Iterate(Rr_App *App, void *UserData)
{
    static_cast<SGame *>(UserData)->Iterate();
}

static void Cleanup(Rr_App *App, void *UserData)
{
    static_cast<SGame *>(UserData)->~SGame();
}

static void OnFileDropped(Rr_App *App, const char *Path)
{
    HandleFileDrop(Path);
}

void RunGame()
{
    std::byte Game[sizeof(SGame)];
    Rr_AppConfig Config = { .Title = "RrDemo",
                            .InitFunc = Init,
                            .CleanupFunc = Cleanup,
                            .IterateFunc = Iterate,
                            .FileDroppedFunc = OnFileDropped,
                            .UserData = Game };
    Rr_Run(&Config);
}
