#include "Game.hxx"

#include "DemoAssets.inc"

#include <Rr/Rr.h>

#include <array>
#include <format>
#include <iostream>
#include <random>
#include <string>

// struct SUber3DGlobals
// {
//     Rr_Mat4 View;
//     Rr_Mat4 Proj;
//     Rr_Vec4 AmbientLightColor;
//     Rr_Vec4 DirectionalLightDirection;
//     Rr_Vec4 DirectionalLightColor;
//     Rr_Vec4 DirectionalLightIntensity;
// };
//
// enum EInputAction
// {
//     EIA_UP,
//     EIA_DOWN,
//     EIA_LEFT,
//     EIA_RIGHT,
//     EIA_FULLSCREEN,
//     EIA_DEBUGOVERLAY,
//     EIA_TEST,
//     EIA_COUNT,
// };
//
// struct SEntity
// {
// };
//
// template <typename TGlobals, typename TMaterial, typename TPerDraw> struct
// TPipeline
// {
// protected:
//     Rr_App *App;
//
// public:
//     using SGlobals = TGlobals;
//     using SMaterial = TMaterial;
//     using SPerDraw = TPerDraw;
//
//     Rr_GenericPipeline *GenericPipeline{};
//
//     explicit TPipeline(Rr_App *InApp)
//         : App(InApp)
//     {
//     }
//
//     ~TPipeline()
//     {
//         Rr_DestroyGenericPipeline(App, GenericPipeline);
//     }
//
//     TPipeline(TPipeline &&Rhs) = delete;
//     TPipeline &operator=(TPipeline &&Rhs) = delete;
//     TPipeline(const TPipeline &Rhs) = delete;
//     TPipeline &operator=(const TPipeline &Rhs) = delete;
// };
//
// struct SShadowPassGlobals
// {
//     Rr_Mat4 View;
//     Rr_Mat4 Proj;
// };
//
// struct SShadowPassMaterial
// {
//     Rr_Vec3 Emissive;
// };
//
// struct SShadowPerDraw
// {
//     Rr_Mat4 Model;
// };
//
// struct SShadowPassPipeline : TPipeline<SShadowPassGlobals,
// SShadowPassMaterial, SShadowPerDraw>
// {
//     explicit SShadowPassPipeline(Rr_App *InApp)
//         : TPipeline(InApp)
//     {
//         Rr_Asset VertexShader = Rr_LoadAsset(DEMO_ASSET_SHADOWPASS_VERT_SPV);
//
//         Rr_PipelineBuilder *Builder = Rr_CreatePipelineBuilder();
//         Rr_EnableVertexStage(Builder, &VertexShader);
//         Rr_EnableDepthTest(Builder);
//         Rr_EnableRasterizer(Builder, RR_POLYGON_MODE_FILL);
//         GenericPipeline = Rr_BuildGenericPipeline(App, Builder,
//         sizeof(SGlobals), sizeof(SMaterial), sizeof(SPerDraw));
//     }
// };
//
// struct SUnlitGlobals
// {
//     Rr_Mat4 View;
//     Rr_Mat4 Proj;
// };
//
// struct SUnlitMaterial
// {
//     Rr_Vec3 Emissive;
// };
//
// struct SUnlitPerDraw
// {
//     Rr_Mat4 Model;
// };
//
// struct SUnlitPipeline : TPipeline<SUnlitGlobals, SUnlitMaterial,
// SUnlitPerDraw>
// {
//     explicit SUnlitPipeline(Rr_App *InApp)
//         : TPipeline(InApp)
//     {
//         Rr_Asset VertexShader = Rr_LoadAsset(DEMO_ASSET_UNLIT_VERT_SPV);
//         Rr_Asset FragmentShader = Rr_LoadAsset(DEMO_ASSET_UNLIT_FRAG_SPV);
//
//         Rr_PipelineBuilder *Builder = Rr_CreatePipelineBuilder();
//
//         Rr_EnableVertexStage(Builder, &VertexShader);
//         Rr_EnableFragmentStage(Builder, &FragmentShader);
//         Rr_EnableColorAttachment(Builder, false);
//         Rr_EnableDepthTest(Builder);
//         Rr_EnableRasterizer(Builder, RR_POLYGON_MODE_FILL);
//         GenericPipeline = Rr_BuildGenericPipeline(App, Builder,
//         sizeof(SGlobals), sizeof(SMaterial), sizeof(SPerDraw));
//     }
// };
//
// struct SUber3DMaterial
// {
//     Rr_Vec3 Emissive;
// };
//
// struct SUber3DPerDraw
// {
//     Rr_Mat4 Model;
// };
//
// struct SUber3DPushConstants
// {
//     Rr_Mat4 Reserved;
// };
//
// struct SUber3DPipeline : TPipeline<SUber3DGlobals, SUber3DMaterial,
// SUber3DPerDraw>
// {
//     explicit SUber3DPipeline(Rr_App *InApp)
//         : TPipeline(InApp)
//     {
//         Rr_Asset Uber3DVERT = Rr_LoadAsset(DEMO_ASSET_UBER3D_VERT_SPV);
//         Rr_Asset Uber3DFRAG = Rr_LoadAsset(DEMO_ASSET_UBER3D_FRAG_SPV);
//
//         Rr_PipelineBuilder *Builder = Rr_CreatePipelineBuilder();
//         Rr_EnableVertexStage(Builder, &Uber3DVERT);
//         Rr_EnableFragmentStage(Builder, &Uber3DFRAG);
//         Rr_EnableColorAttachment(Builder, false);
//         Rr_EnableDepthTest(Builder);
//         Rr_EnableRasterizer(Builder, RR_POLYGON_MODE_FILL);
//         GenericPipeline = Rr_BuildGenericPipeline(App, Builder,
//         sizeof(SGlobals), sizeof(SMaterial), sizeof(SPerDraw));
//     }
// };
//
// struct SCamera
// {
//     float Pitch{};
//     float Yaw{};
//     Rr_Vec3 Position{};
//     Rr_Mat4 ViewMatrix = Rr_M4D(1.0f);
//
//     [[nodiscard]] Rr_Vec3 GetForwardVector() const
//     {
//         return Rr_Norm(Rr_InvGeneral(ViewMatrix).Columns[2].XYZ);
//     }
//
//     [[nodiscard]] Rr_Vec3 GetRightVector() const
//     {
//         return Rr_Norm(Rr_InvGeneral(ViewMatrix).Columns[0].XYZ);
//     }
//
//     [[nodiscard]] Rr_Mat4 GetViewMatrix()
//     {
//         float CosPitch = cos(Pitch * Rr_DegToRad);
//         float SinPitch = sin(Pitch * Rr_DegToRad);
//         float CosYaw = cos(Yaw * Rr_DegToRad);
//         float SinYaw = sin(Yaw * Rr_DegToRad);
//
//         Rr_Vec3 XAxis{ CosYaw, 0.0f, -SinYaw };
//         Rr_Vec3 YAxis{ SinYaw * SinPitch, CosPitch, CosYaw * SinPitch };
//         Rr_Vec3 ZAxis{ SinYaw * CosPitch, -SinPitch, CosPitch * CosYaw };
//
//         ViewMatrix = {
//             XAxis.X,
//             YAxis.X,
//             ZAxis.X,
//             0.0f,
//             XAxis.Y,
//             YAxis.Y,
//             ZAxis.Y,
//             0.0f,
//             XAxis.Z,
//             YAxis.Z,
//             ZAxis.Z,
//             0.0f,
//             -Rr_Dot(XAxis, Position),
//             -Rr_Dot(YAxis, Position),
//             -Rr_Dot(ZAxis, Position),
//             1.0f,
//         };
//
//         return ViewMatrix;
//     }
// };
//
// struct SGame
// {
// private:
//     Rr_App *App{};
//
//     Rr_Arena *PermanentArena;
//
//     std::array<Rr_InputMapping, EIA_COUNT> InputMappings{};
//
//     SUber3DGlobals ShaderGlobals{};
//
//     SUber3DPipeline Uber3DPipeline;
//     SUnlitPipeline UnlitPipeline;
//     SShadowPassPipeline ShadowPipeline;
//
//     // Rr_Image SceneDepthImage;
//     // Rr_Image SceneColorImage;
//
//     Rr_StaticMesh *ArrowMesh{};
//
//     Rr_StaticMesh *CottageMesh{};
//     Rr_Image *CottageDiffuse{};
//     Rr_Image *CottageNormal{};
//     Rr_Material *CottageMaterial{};
//
//     Rr_StaticMesh *PocMesh{};
//     Rr_Image *PocDiffuseImage{};
//
//     Rr_StaticMesh *MarbleMesh{};
//
//     Rr_StaticMesh *AvocadoMesh{};
//
//     Rr_LoadingContext *LoadingContext{};
//
//     Rr_DrawTarget *ShadowMap;
//     Rr_DrawTarget *TestTarget;
//
//     Rr_GLTFLoader UnlitGLTFLoader;
//     Rr_GLTFLoader Uber3DGLTFLoader;
//
//     bool IsLoaded = false;
//
//     Rr_String TestString;
//     Rr_String DebugString;
//     Rr_String LoadingString;
//
// public:
//     void InitInputMappings()
//     {
//         InputMappings[EIA_UP].Primary = RR_SCANCODE_W;
//         InputMappings[EIA_DOWN].Primary = RR_SCANCODE_S;
//         InputMappings[EIA_LEFT].Primary = RR_SCANCODE_A;
//         InputMappings[EIA_RIGHT].Primary = RR_SCANCODE_D;
//         InputMappings[EIA_FULLSCREEN].Primary = RR_SCANCODE_F11;
//         InputMappings[EIA_DEBUGOVERLAY].Primary = RR_SCANCODE_F1;
//         InputMappings[EIA_TEST].Primary = RR_SCANCODE_F2;
//
//         Rr_InputConfig InputConfig = {
//             .Mappings = InputMappings.data(),
//             .Count = InputMappings.size(),
//         };
//
//         Rr_SetInputConfig(App, &InputConfig);
//     }
//
//     void InitGlobals()
//     {
//         ShaderGlobals.AmbientLightColor = { 0.01f, 0.01f, 0.01f, 1.0f };
//         ShaderGlobals.DirectionalLightColor = { 1.0f, 0.95f, 0.93f, 1.0f };
//         ShaderGlobals.DirectionalLightIntensity = { 1.0f, 1.0f, 1.0f, 1.0f };
//
//         ShaderGlobals.Proj = Rr_Perspective_LH_ZO(0.7643276f,
//         Rr_GetAspectRatio(App), 0.5f, 50.0f);
//     }
//
//     void OnLoadingComplete()
//     {
//         std::array CottageTextures = { CottageDiffuse, CottageNormal };
//         CottageMaterial =
//             Rr_CreateMaterial(App, Uber3DPipeline.GenericPipeline,
//             CottageTextures.data(), CottageTextures.size());
//
//         IsLoaded = true;
//
//         LoadingContext = nullptr;
//     }
//
//     static void OnLoadingComplete(Rr_App *App, void *UserData)
//     {
//         auto *Game = static_cast<SGame *>(UserData);
//         Game->OnLoadingComplete();
//     }
//
//     void DrawScene(Rr_GraphNode *Node)
//     {
//         const auto Time = static_cast<float>((Rr_GetTimeSeconds(App) * 2.0));
//
//         SUber3DPerDraw CottageDraw = {};
//         CottageDraw.Model = Rr_Scale({ 1.f, 1.f, 1.f });
//         CottageDraw.Model[3][1] = 0.1f;
//         Rr_DrawStaticMeshOverrideMaterials(App, Node, &CottageMaterial, 1,
//         CottageMesh, RR_MAKE_DATA(CottageDraw));
//
//         SUber3DPerDraw AvocadoDraw = {};
//         AvocadoDraw.Model = Rr_Scale({ 0.75f, 0.75f, 0.75f }) *
//                             Rr_Rotate_LH(fmodf(Time, RR_PI32 * 2.0f), {
//                             0.0f, 1.0f, 0.0f }) * Rr_Translate({ 3.5f,
//                             0.5f, 3.5f });
//         AvocadoDraw.Model[3][0] = 3.5f;
//         AvocadoDraw.Model[3][1] = 0.5f;
//         AvocadoDraw.Model[3][2] = 3.5f;
//         Rr_DrawStaticMesh(App, Node, AvocadoMesh, RR_MAKE_DATA(AvocadoDraw));
//
//         SUber3DPerDraw MarbleDraw = {};
//         MarbleDraw.Model = Rr_Translate({ 0.0f, 0.1f, 0.0f });
//         Rr_DrawStaticMesh(App, Node, MarbleMesh, RR_MAKE_DATA(MarbleDraw));
//     }
//
//     void Iterate()
//     {
//         auto [Keys, MousePosition, MousePositionDelta, MouseState] =
//         Rr_GetInputState(App);
//
//         auto DeltaTime = static_cast<float>(Rr_GetDeltaSeconds(App));
//
//         auto SwapchainSize = Rr_GetSwapchainSize(App);
//
//         if(Rr_GetKeyState(Keys, EIA_FULLSCREEN) == RR_KEYSTATE_PRESSED)
//         {
//             Rr_ToggleFullscreen(App);
//         }
//
//         // ImGui::Begin("nest1", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
//         // ImGui::LabelText("sdfsdf", "Test!!");
//         // ImGui::Begin("nest2", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
//         // ImGui::LabelText("sdfsdf", "Test!!");
//         // ImGui::End();
//         // ImGui::End();
//
//         Rr_BeginWindow("IMGUI");
//         Rr_Label("Test...\n\nA couple of new lines...");
//         Rr_Label("Label 1");
//         Rr_BeginHorizontal();
//         Rr_Label("Label 2");
//         Rr_Label("Label 3");
//         Rr_EndHorizontal();
//         Rr_EndWindow();
//
//         ImGui::Begin("RotTest", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
//
//         static SCamera Camera = {
//             .Pitch = 90.0f - 63.5593f,
//             .Yaw = -46.6919f,
//             .Position = { -7.35889f, 4.0f, -6.92579f },
//         };
//
//         // static Rr_Vec3 LightDirEuler = { 90.0f
//         - 37.261f, 99.6702f, 3.16371f
//         // };
//         static Rr_Vec3 LightDirEuler = { -100.0f, 18.0f, 3.16371f };
//         ImGui::LabelText("Camera", "Camera");
//         ImGui::SliderFloat2("CameraPitchYaw", &Camera.Pitch, -360.0f, 360.0f,
//         "%f", ImGuiSliderFlags_None); ImGui::SliderFloat3("CameraPos",
//         Camera.Position.Elements, -100.0f, 100.0f, "%.3f",
//         ImGuiSliderFlags_None); ImGui::SliderFloat3("LightDir",
//         LightDirEuler.Elements, -100.0f, 100.0f, "%f",
//         ImGuiSliderFlags_None); ImGui::Separator(); static Rr_Vec3 ShadowEye
//         = { 3, 3, 3 }; static Rr_Vec3 ShadowCenter = { 0, 0, 0 };
//         ImGui::SliderFloat3("ShadowEye", ShadowEye.Elements, -10.0f, 10.0f,
//         "%.3f", ImGuiSliderFlags_None); ImGui::SliderFloat3("ShadowCenter",
//         ShadowCenter.Elements, -10.0f, 10.0f, "%f", ImGuiSliderFlags_None);
//
//         Rr_Vec3 LightRotation = LightDirEuler * Rr_DegToRad;
//         ShaderGlobals.DirectionalLightDirection =
//         Rr_EulerXYZ(LightRotation).Columns[2];
//
//         Rr_Vec3 CameraForward = Camera.GetForwardVector();
//         Rr_Vec3 CameraLeft = Camera.GetRightVector();
//         constexpr float CameraSpeed = 0.1f;
//         if(Rr_GetKeyState(Keys, EIA_UP) == RR_KEYSTATE_HELD)
//         {
//             Camera.Position -= CameraForward * DeltaTime * CameraSpeed;
//         }
//         if(Rr_GetKeyState(Keys, EIA_LEFT) == RR_KEYSTATE_HELD)
//         {
//             Camera.Position -= CameraLeft * DeltaTime * CameraSpeed;
//         }
//         if(Rr_GetKeyState(Keys, EIA_DOWN) == RR_KEYSTATE_HELD)
//         {
//             Camera.Position += CameraForward * DeltaTime * CameraSpeed;
//         }
//         if(Rr_GetKeyState(Keys, EIA_RIGHT) == RR_KEYSTATE_HELD)
//         {
//             Camera.Position += CameraLeft * DeltaTime * CameraSpeed;
//         }
//
//         if(MouseState & RR_MOUSE_BUTTON_RIGHT_MASK)
//         {
//             Rr_SetRelativeMouseMode(App, true);
//             constexpr float Sensitivity = 0.2f;
//             Camera.Yaw = Rr_WrapMax(Camera.Yaw - (MousePositionDelta.X *
//             Sensitivity), 360.0f); Camera.Pitch = Rr_WrapMinMax(Camera.Pitch
//             - (MousePositionDelta.Y * Sensitivity), -90.0f, 90.0f);
//         }
//         else
//         {
//             Rr_SetRelativeMouseMode(App, false);
//         }
//
//         ShaderGlobals.View = Rr_VulkanMatrix() * Camera.GetViewMatrix();
//
//         ImGui::End();
//
//         static bool ShowDebugOverlay = false;
//         if(Rr_GetKeyState(Keys, EIA_DEBUGOVERLAY) == RR_KEYSTATE_PRESSED)
//         {
//             ShowDebugOverlay = !ShowDebugOverlay;
//         }
//         if(ShowDebugOverlay)
//         {
//             Rr_DebugOverlay(App);
//         }
//
//         Rr_PerspectiveResize(Rr_GetAspectRatio(App), &ShaderGlobals.Proj);
//
//         /* Create Nodes */
//
//         Rr_GraphicsNodeInfo TestNodeInfo = {
//             .DrawTarget = TestTarget,
//             .InitialColor = nullptr,
//             .InitialDepth = nullptr,
//             .Viewport = { 0, 0, 1024, 1024 },
//             .BasePipeline = Uber3DPipeline.GenericPipeline,
//             .OverridePipeline = nullptr,
//         };
//         SUber3DGlobals TestGlobals = ShaderGlobals;
//         TestGlobals.View = Rr_LookAt_LH(ShadowEye, ShadowCenter, { 0.0, 1.0f,
//         0.0f }), TestGlobals.Proj =
//             Rr_Orthographic_LH_ZO(-512 * 0.05f, 512 * 0.05f, -512 * 0.05f,
//             512 * 0.05f, -1000.0f, 1000.0f);
//         Rr_GraphNode *TestNode =
//             Rr_AddGraphicsNode(App, "test_pass", &TestNodeInfo,
//             reinterpret_cast<char *>(&TestGlobals), nullptr, 0);
//
//         Rr_GraphicsNodeInfo ShadowNodeInfo = {
//             .DrawTarget = ShadowMap,
//             .InitialColor = nullptr,
//             .InitialDepth = nullptr,
//             .Viewport = { 0, 0, 1024, 1024 },
//             .BasePipeline = ShadowPipeline.GenericPipeline,
//             .OverridePipeline = ShadowPipeline.GenericPipeline,
//         };
//         SShadowPassPipeline::SGlobals ShadowGlobals = {
//             .View = Rr_LookAt_LH(ShadowEye, ShadowCenter, { 0.0, 1.0f, 0.0f
//             }), .Proj = Rr_Orthographic_LH_ZO(-512 * 0.05f, 512 * 0.05f, -512
//             * 0.05f, 512 * 0.05f, -1000.0f, 1000.0f),
//         };
//         Rr_GraphNode *ShadowNode = Rr_AddGraphicsNode(
//             App,
//             "shadow_pass",
//             &ShadowNodeInfo,
//             reinterpret_cast<char *>(&ShadowGlobals),
//             nullptr,
//             0);
//
//         std::array NodeDependencies = { TestNode, ShadowNode };
//         // std::array<Rr_GraphNode *, 0> NodeDependencies{};
//
//         Rr_GraphicsNodeInfo NodeInfo = {
//             .DrawTarget = nullptr,
//             .InitialColor = nullptr,
//             .InitialDepth = nullptr,
//             .Viewport = {},
//             .BasePipeline = Uber3DPipeline.GenericPipeline,
//             .OverridePipeline = nullptr,
//         };
//         Rr_GraphNode *Node = Rr_AddGraphicsNode(
//             App,
//             "pbr_pass",
//             &NodeInfo,
//             reinterpret_cast<char *>(&ShaderGlobals),
//             NodeDependencies.data(),
//             NodeDependencies.size());
//
//         Rr_BlitNodeInfo BlitInfo = {
//             .SrcImage = Rr_GetDrawTargetColorImage(App, TestTarget),
//             .DstImage = Rr_GetDrawTargetColorImage(App,
//             Rr_GetMainDrawTarget(App)), .SrcRect = { 0, 0, 1024, 1024 },
//             .DstRect = { SwapchainSize.Width - 512, 0, 512, 512 },
//             .Mode = RR_BLIT_MODE_COLOR,
//         };
//         Rr_GraphNode *BlitNode = Rr_AddBlitNode(App, "blit_test", &BlitInfo,
//         &Node, 1);
//
//         Rr_GraphNode *BuiltinNode = Rr_AddBuiltinNode(App, "builtin",
//         &BlitNode, 1);
//         // Rr_GraphNode *BuiltinNode = Rr_AddBuiltinNode(App, "builtin",
//         &Node,
//         // 1);
//
//         Rr_PresentNodeInfo PresentInfo = {
//             .Mode = RR_PRESENT_MODE_STRETCH,
//             .DrawTarget = nullptr,
//         };
//
//         Rr_AddPresentNode(App, "present", &PresentInfo, &BuiltinNode, 1);
//
//         if(IsLoaded)
//         {
//             DrawScene(ShadowNode);
//             DrawScene(TestNode);
//
//             SUnlitPipeline::SPerDraw ArrowDraw = {};
//             ArrowDraw.Model = Rr_EulerXYZ(LightRotation);
//             ArrowDraw.Model[3][1] = 5.0f;
//             Rr_DrawStaticMesh(App, Node, ArrowMesh, RR_MAKE_DATA(ArrowDraw));
//
//             DrawScene(Node);
//
//             Rr_DrawDefaultText(App, BuiltinNode, &TestString, { 50.0f, 50.0f
//             });
//
//             Rr_DrawCustomText(
//                 App,
//                 BuiltinNode,
//                 nullptr,
//                 &DebugString,
//                 { 450.0f, 54.0f },
//                 28.0f,
//                 RR_DRAW_TEXT_FLAGS_NONE_BIT);
//         }
//         else
//         {
//             if(LoadingContext != nullptr)
//             {
//                 uint32_t Current, Total;
//                 Rr_GetLoadProgress(LoadingContext, &Current, &Total);
//
//                 std::string Formatted = std::format("Загружайу: {}/{}\n",
//                 Current, Total); Rr_UpdateString(&LoadingString, 128,
//                 Formatted.data(), Formatted.length());
//             }
//
//             Rr_DrawCustomText(
//                 App,
//                 BuiltinNode,
//                 nullptr,
//                 &LoadingString,
//                 { 25.0f, 540.0f - 25 - 32.0f },
//                 32.0f,
//                 RR_DRAW_TEXT_FLAGS_ANIMATION_BIT);
//         }
//     }
//
//     explicit SGame(Rr_App *InApp)
//         : App(InApp)
//         , PermanentArena(Rr_CreateDefaultArena())
//         , Uber3DPipeline(App)
//         , UnlitPipeline(App)
//         , ShadowPipeline(App)
//         , ShadowMap(Rr_CreateDrawTargetDepthOnly(App, 1024, 1024))
//         , TestTarget(Rr_CreateDrawTarget(App, 1024, 1024))
//         , UnlitGLTFLoader({
//               .GenericPipeline = UnlitPipeline.GenericPipeline,
//               .BaseTexture = 0,
//               .NormalTexture = 1,
//               .SpecularTexture = 2,
//           })
//         , Uber3DGLTFLoader({
//               .GenericPipeline = Uber3DPipeline.GenericPipeline,
//               .BaseTexture = 0,
//               .NormalTexture = 1,
//               .SpecularTexture = 2,
//           })
//     {
//         InitInputMappings();
//
//         InitGlobals();
//
//         std::array LoadTasks = {
//             Rr_LoadColorImageFromPNG(DEMO_ASSET_COTTAGEDIFFUSE_PNG,
//             &CottageDiffuse),
//             Rr_LoadColorImageFromPNG(DEMO_ASSET_COTTAGENORMAL_PNG,
//             &CottageNormal),
//             Rr_LoadStaticMeshFromGLTF(DEMO_ASSET_AVOCADO_GLB,
//             &Uber3DGLTFLoader, 0, &AvocadoMesh),
//             Rr_LoadStaticMeshFromGLTF(DEMO_ASSET_MARBLE_GLB,
//             &Uber3DGLTFLoader, 0, &MarbleMesh),
//             Rr_LoadStaticMeshFromGLTF(DEMO_ASSET_ARROW_GLB, &UnlitGLTFLoader,
//             0, &ArrowMesh), Rr_LoadStaticMeshFromOBJ(DEMO_ASSET_COTTAGE_OBJ,
//             &CottageMesh),
//         };
//         LoadingContext = Rr_LoadAsync(App, LoadTasks.data(),
//         LoadTasks.size(), OnLoadingComplete, this);
//
//         TestString = RR_STRING("Test...\n\nA couple of new lines...",
//         PermanentArena);
//         // TestString = RR_STRING(
//         //     "A quick brown fox @#$ \nNew line "
//         //     "test...\n\nA couple of new lines...",
//         //     PermanentArena);
//         DebugString = RR_STRING("$c3Colored $c1text$c2", PermanentArena);
//         LoadingString = Rr_CreateEmptyString(128, PermanentArena);
//     }
//
//     ~SGame()
//     {
//         Rr_DestroyMaterial(App, CottageMaterial);
//
//         // Rr_DestroyImage(App, &SceneDepthImage);
//         // Rr_DestroyImage(App, &SceneColorImage);
//         Rr_DestroyImage(App, CottageDiffuse);
//         Rr_DestroyImage(App, CottageNormal);
//         Rr_DestroyImage(App, PocDiffuseImage);
//
//         Rr_DestroyStaticMesh(App, CottageMesh);
//         Rr_DestroyStaticMesh(App, PocMesh);
//         Rr_DestroyStaticMesh(App, MarbleMesh);
//         Rr_DestroyStaticMesh(App, AvocadoMesh);
//         Rr_DestroyStaticMesh(App, ArrowMesh);
//
//         Rr_DestroyDrawTarget(App, ShadowMap);
//         Rr_DestroyDrawTarget(App, TestTarget);
//
//         Rr_DestroyArena(PermanentArena);
//     }
// };

static Rr_App *App;

static Rr_GLTFContext *GLTFContext;
static Rr_GLTFAsset *GLTFAsset;
static Rr_PipelineLayout *GLTFPipelineLayout;
static Rr_GraphicsPipeline *GLTFPipeline;

static Rr_Image *ColorAttachmentA;
static Rr_Image *ColorAttachmentB;
static Rr_Image *TexturePNG;
static Rr_PipelineLayout *PipelineLayout;
static Rr_GraphicsPipeline *GraphicsPipeline;
static Rr_Buffer *VertexBuffer;
static Rr_Buffer *IndexBuffer;
static Rr_Buffer *UniformBuffer;
static Rr_Sampler *LinearSampler;
static Rr_LoadThread *LoadThread;
static bool LoadComplete;

std::random_device RandomDevice;
std::mt19937 Generator(RandomDevice());
std::uniform_real_distribution<float> Distribution(0.0f, 1.0f);

struct STest
{
    Rr_Mat4 Padding1;
    Rr_Mat4 Padding2;
    Rr_Mat4 Padding3;
    Rr_Mat4 Padding4;
    Rr_Vec3 Offset;
};

static void InitGLTF()
{
    Rr_PipelineBinding BindingsA[] = {
        {
            .Slot = 0,
            .Count = 1,
            .Type = RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER,
        },
        {
            .Slot = 1,
            .Count = 1,
            .Type = RR_PIPELINE_BINDING_TYPE_COMBINED_SAMPLER,
        },

    };
    Rr_PipelineBinding BindingB = {
        .Slot = 3,
        .Count = 1,
        .Type = RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER,
    };
    Rr_PipelineBindingSet BindingSets[] = {
        {
            .BindingCount = RR_ARRAY_COUNT(BindingsA),
            .Bindings = BindingsA,
            .Stages = RR_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .BindingCount = 1,
            .Bindings = &BindingB,
            .Stages = RR_SHADER_STAGE_VERTEX_BIT,
        },
    };
    GLTFPipelineLayout = Rr_CreatePipelineLayout(App, 2, BindingSets);

    Rr_VertexInputAttribute VertexAttributes[] = {
        { .Format = RR_FORMAT_VEC3, .Location = 0 },
        { .Format = RR_FORMAT_VEC2, .Location = 1 },
        { .Format = RR_FORMAT_VEC3, .Location = 2 },
        { .Format = RR_FORMAT_VEC3, .Location = 3 },
    };

    Rr_VertexInputBinding VertexInputBinding = {
        .Rate = RR_VERTEX_INPUT_RATE_VERTEX,
        .AttributeCount = RR_ARRAY_COUNT(VertexAttributes),
        .Attributes = VertexAttributes,
    };

    Rr_ColorTargetInfo ColorTargets[1] = {};
    ColorTargets[0].Format = Rr_GetSwapchainFormat(App);

    Rr_PipelineInfo PipelineInfo = {};
    PipelineInfo.Layout = GLTFPipelineLayout;
    PipelineInfo.VertexShaderSPV = Rr_LoadAsset(DEMO_ASSET_TEST_VERT_SPV);
    PipelineInfo.FragmentShaderSPV = Rr_LoadAsset(DEMO_ASSET_TEST_FRAG_SPV);
    PipelineInfo.VertexInputBindingCount = 1;
    PipelineInfo.VertexInputBindings = &VertexInputBinding;
    PipelineInfo.ColorTargetCount = 1;
    PipelineInfo.ColorTargets = ColorTargets;

    GLTFPipeline = Rr_CreateGraphicsPipeline(App, &PipelineInfo);

    Rr_GLTFAttributeType GLTFAttributes[] = {
        RR_GLTF_ATTRIBUTE_TYPE_POSITION,
        RR_GLTF_ATTRIBUTE_TYPE_TEXCOORD0,
        RR_GLTF_ATTRIBUTE_TYPE_NORMAL,
        RR_GLTF_ATTRIBUTE_TYPE_TANGENT,
    };
    Rr_GLTFVertexInputBinding GLTFVertexInputBinding = {
        .AttributeTypeCount = RR_ARRAY_COUNT(GLTFAttributes),
        .AttributeTypes = GLTFAttributes,
    };
    GLTFContext = Rr_CreateGLTFContext(
        App,
        1,
        &VertexInputBinding,
        &GLTFVertexInputBinding);
}

static void OnLoadComplete(Rr_App *App, void *UserData)
{
    LoadComplete = true;
}

static void Init(Rr_App *App, void *UserData)
{
    ::App = App;
    LoadThread = Rr_CreateLoadThread(App);

    Rr_SamplerInfo SamplerInfo = {};
    SamplerInfo.MinFilter = RR_FILTER_LINEAR;
    SamplerInfo.MagFilter = RR_FILTER_LINEAR;
    LinearSampler = Rr_CreateSampler(App, &SamplerInfo);

    Rr_PipelineBinding BindingsA[] = {
        {
            .Slot = 0,
            .Count = 1,
            .Type = RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER,
        },
        {
            .Slot = 1,
            .Count = 1,
            .Type = RR_PIPELINE_BINDING_TYPE_COMBINED_SAMPLER,
        },

    };
    Rr_PipelineBinding BindingB = {
        .Slot = 3,
        .Count = 1,
        .Type = RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER,
    };
    Rr_PipelineBindingSet BindingSets[] = {
        {
            .BindingCount = RR_ARRAY_COUNT(BindingsA),
            .Bindings = BindingsA,
            .Stages = RR_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .BindingCount = 1,
            .Bindings = &BindingB,
            .Stages = RR_SHADER_STAGE_VERTEX_BIT,
        },
    };
    PipelineLayout = Rr_CreatePipelineLayout(App, 2, BindingSets);

    Rr_VertexInputAttribute VertexAttributes[] = {
        { .Format = RR_FORMAT_VEC4, .Location = 0 },
        { .Format = RR_FORMAT_VEC2, .Location = 1 },
        { .Format = RR_FORMAT_VEC4, .Location = 2 },
    };

    Rr_VertexInputBinding VertexInputBinding = {
        .Rate = RR_VERTEX_INPUT_RATE_VERTEX,
        .AttributeCount = RR_ARRAY_COUNT(VertexAttributes),
        .Attributes = VertexAttributes,
    };

    Rr_ColorTargetInfo ColorTargets[1] = {};
    ColorTargets[0].Format = Rr_GetSwapchainFormat(App);

    Rr_PipelineInfo PipelineInfo = {};
    PipelineInfo.Layout = PipelineLayout;
    PipelineInfo.VertexShaderSPV = Rr_LoadAsset(DEMO_ASSET_TEST_VERT_SPV);
    PipelineInfo.FragmentShaderSPV = Rr_LoadAsset(DEMO_ASSET_TEST_FRAG_SPV);
    PipelineInfo.VertexInputBindingCount = 1;
    PipelineInfo.VertexInputBindings = &VertexInputBinding;
    PipelineInfo.ColorTargetCount = 1;
    PipelineInfo.ColorTargets = ColorTargets;

    GraphicsPipeline = Rr_CreateGraphicsPipeline(App, &PipelineInfo);

    InitGLTF();

    Rr_LoadTask Tasks[] = {
        {
            .LoadType = RR_LOAD_TYPE_IMAGE_RGBA8_FROM_PNG,
            .AssetRef = DEMO_ASSET_COTTAGEDIFFUSE_PNG,
            .Options = {},
            .Out = { .Image = &TexturePNG },
        },
        {
            .LoadType = RR_LOAD_TYPE_GLTF_ASSET,
            .AssetRef = DEMO_ASSET_CUBE_GLB,
            .Options = {
                .GLTF = { .GLTFContext = GLTFContext, },
            },
            .Out = { .GLTFAsset = &GLTFAsset },
        },
    };
    Rr_LoadAsync(LoadThread, RR_ARRAY_COUNT(Tasks), Tasks, OnLoadComplete, App);

    float VertexData[] = {
        0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
    };

    VertexBuffer = Rr_CreateBuffer(
        App,
        sizeof(VertexData),
        RR_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        false);
    Rr_UploadToDeviceBufferImmediate(
        App,
        VertexBuffer,
        RR_MAKE_DATA_ARRAY(VertexData));

    uint32_t IndexData[] = {
        2,
        1,
        0,
    };

    IndexBuffer = Rr_CreateBuffer(
        App,
        sizeof(IndexData),
        RR_BUFFER_USAGE_INDEX_BUFFER_BIT,
        false);
    Rr_UploadToDeviceBufferImmediate(
        App,
        IndexBuffer,
        RR_MAKE_DATA_ARRAY(IndexData));

    ColorAttachmentA = Rr_CreateImage(
        App,
        { 1024, 1024, 1 },
        Rr_GetSwapchainFormat(App),
        RR_IMAGE_USAGE_COLOR_ATTACHMENT | RR_IMAGE_USAGE_TRANSFER |
            RR_IMAGE_USAGE_SAMPLED,
        false);

    ColorAttachmentB = Rr_CreateImage(
        App,
        { 512, 512, 1 },
        Rr_GetSwapchainFormat(App),
        RR_IMAGE_USAGE_COLOR_ATTACHMENT | RR_IMAGE_USAGE_TRANSFER |
            RR_IMAGE_USAGE_SAMPLED,
        false);

    UniformBuffer = Rr_CreateBuffer(
        App,
        RR_KILOBYTES(4),
        RR_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        true);
}

static Rr_Vec4 GetRandomVec4()
{
    return { Distribution(Generator),
             Distribution(Generator),
             Distribution(Generator),
             1.0f };
}

static STest GetRandomSTest()
{
    STest Test;
    Test.Offset = { Distribution(Generator) * 0.1f,
                    Distribution(Generator) * 0.1f,
                    Distribution(Generator) * 0.1f };
    return Test;
}

static void TestGraphicsNode(
    Rr_App *App,
    const char *Name,
    Rr_GraphImageHandle *AttachmentHandle,
    Rr_GraphBufferHandle *UniformBufferHandle,
    size_t OffsetA,
    size_t OffsetB,
    Rr_GraphBufferHandle *VertexBufferHandle,
    Rr_GraphBufferHandle *IndexBufferHandle,
    Rr_GraphImageHandle *SampledImageHandle,
    Rr_ColorClear ColorClear = { 0.2f, 0.9f, 0.9f, 1.0f })
{
    Rr_ColorTarget OffscreenTarget = {
        .Slot = 0,
        .LoadOp = RR_LOAD_OP_CLEAR,
        .StoreOp = RR_STORE_OP_STORE,
        .Clear = ColorClear,
    };
    Rr_GraphNode *OffscreenNode = Rr_AddGraphicsNode(
        App,
        Name,
        1,
        &OffscreenTarget,
        &AttachmentHandle,
        nullptr,
        nullptr);
    Rr_BindGraphicsPipeline(OffscreenNode, GraphicsPipeline);
    Rr_BindGraphicsUniformBuffer(
        OffscreenNode,
        UniformBufferHandle,
        0,
        0,
        OffsetA,
        sizeof(Rr_Vec4));
    Rr_BindGraphicsUniformBuffer(
        OffscreenNode,
        UniformBufferHandle,
        1,
        3,
        OffsetB,
        sizeof(STest));
    if(SampledImageHandle != nullptr)
    {
        Rr_BindCombinedImageSampler(
            OffscreenNode,
            SampledImageHandle,
            LinearSampler,
            0,
            1);
    }
    Rr_BindVertexBuffer(OffscreenNode, VertexBufferHandle, 0, 0);
    Rr_BindIndexBuffer(
        OffscreenNode,
        IndexBufferHandle,
        0,
        0,
        RR_INDEX_TYPE_UINT32);
    Rr_DrawIndexed(OffscreenNode, 3, 1, 0, 0, 0);
}

static void Iterate(Rr_App *App, void *UserData)
{
    /* Register Graph Resources */

    Rr_GraphImageHandle ColorAttachmentAHandle =
        Rr_RegisterGraphImage(App, ColorAttachmentA);
    Rr_GraphImageHandle ColorAttachmentBHandle =
        Rr_RegisterGraphImage(App, ColorAttachmentB);
    Rr_GraphBufferHandle UniformBufferHandle =
        Rr_RegisterGraphBuffer(App, UniformBuffer);
    Rr_GraphBufferHandle VertexBufferHandle =
        Rr_RegisterGraphBuffer(App, VertexBuffer);
    Rr_GraphBufferHandle IndexBufferHandle =
        Rr_RegisterGraphBuffer(App, IndexBuffer);

    /* Update Uniform Buffer */

    size_t UniformAlignment = Rr_GetUniformAlignment(App);

    Rr_Vec4 UniformValueA = GetRandomVec4();
    STest UniformValueB = GetRandomSTest();

    size_t OffsetA = 0;
    Rr_GraphNode *TransferNode =
        Rr_AddTransferNode(App, "transfer_a", &UniformBufferHandle);
    Rr_TransferBufferData(
        App,
        TransferNode,
        RR_MAKE_DATA_STRUCT(UniformValueA),
        OffsetA);
    size_t OffsetB = RR_ALIGN_POW2(sizeof(Rr_Vec4), UniformAlignment);
    Rr_TransferBufferData(
        App,
        TransferNode,
        RR_MAKE_DATA_STRUCT(UniformValueB),
        OffsetB);

    /* Draw Offscreen A */

    TestGraphicsNode(
        App,
        "offscreen_a",
        &ColorAttachmentAHandle,
        &UniformBufferHandle,
        OffsetA,
        OffsetB,
        &VertexBufferHandle,
        &IndexBufferHandle,
        &ColorAttachmentBHandle);

    /* Draw Offscreen B */

    if(LoadComplete)
    {
        Rr_GraphImageHandle TexturePNGHandle =
            Rr_RegisterGraphImage(App, TexturePNG);
        TestGraphicsNode(
            App,
            "offscreen_b",
            &ColorAttachmentBHandle,
            &UniformBufferHandle,
            OffsetA,
            OffsetB,
            &VertexBufferHandle,
            &IndexBufferHandle,
            &TexturePNGHandle,
            { 1.0f, 0.1f, 0.2f, 1.0f });
    }
    else
    {
        TestGraphicsNode(
            App,
            "offscreen_b",
            &ColorAttachmentBHandle,
            &UniformBufferHandle,
            OffsetA,
            OffsetB,
            &VertexBufferHandle,
            &IndexBufferHandle,
            &ColorAttachmentAHandle,
            { 1.0f, 0.1f, 0.2f, 1.0f });
    }

    /* Blit B to A (testing batching image reads with different layouts) */

    Rr_AddBlitNode(
        App,
        "blit",
        &ColorAttachmentBHandle,
        &ColorAttachmentAHandle,
        { 0, 0, 1024, 1024 },
        { 0, 0, 512, 512 },
        RR_BLIT_MODE_COLOR);

    /* Present */

    Rr_AddPresentNode(
        App,
        "present",
        &ColorAttachmentAHandle,
        LinearSampler,
        RR_PRESENT_MODE_FIT);
}

static void Cleanup(Rr_App *App, void *UserData)
{
    Rr_DestroyLoadThread(App, LoadThread);
    Rr_DestroyGLTFContext(App, GLTFContext);
    Rr_DestroyGraphicsPipeline(App, GLTFPipeline);
    Rr_DestroyPipelineLayout(App, GLTFPipelineLayout);
    Rr_DestroyImage(App, ColorAttachmentA);
    Rr_DestroyImage(App, ColorAttachmentB);
    if(LoadComplete)
    {
        Rr_DestroyImage(App, TexturePNG);
    }
    Rr_DestroyBuffer(App, VertexBuffer);
    Rr_DestroyBuffer(App, IndexBuffer);
    Rr_DestroyBuffer(App, UniformBuffer);
    Rr_DestroyGraphicsPipeline(App, GraphicsPipeline);
    Rr_DestroyPipelineLayout(App, PipelineLayout);
    Rr_DestroySampler(App, LinearSampler);
}

void RunGame()
{
    Rr_AppConfig Config = {
        .Title = "RrDemo",
        .Version = "0.0.1",
        .Package = "com.rr.demo",
        .InitFunc = Init,
        .CleanupFunc = Cleanup,
        .IterateFunc = Iterate,
        .FileDroppedFunc = nullptr,
        .UserData = nullptr,
    };
    Rr_Run(&Config);
}
