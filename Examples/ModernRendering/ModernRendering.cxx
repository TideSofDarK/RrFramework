#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include <array>
#include <format>
#include <functional>
#include <print>
#include <utility>
#include <vector>

using UScancodes = std::array<bool, RR_SCANCODE_COUNT>;

constexpr Rr_Mat4 FLIP_Y_MATRIX = { 1.0f, 0.0f,  0.0f, 0.0f,
                                    0.0f, -1.0f, 0.0f, 0.0f, //
                                    0.0f, 0.0f,  1.0f, 0.0f, //
                                    0.0f, 0.0f,  0.0f, 1.0f };
constexpr Rr_TextureFormat DEPTH_FORMAT = RR_TEXTURE_FORMAT_D32_SFLOAT;
constexpr float NEAR_PLANE = 0.1f;
constexpr float FAR_PLANE = 100.0f;
constexpr std::size_t MAX_POINT_LIGHTS = 4;
constexpr std::size_t MAX_SPOT_LIGHTS = 4;

struct SCamera
{
    float FOVDegrees = 90.0f;
    float Pitch{};
    float Yaw{};
    Rr_Vec3 Position{};

    Rr_Mat4 Transform = Rr_M4D(1.0f);
    Rr_Mat4 ProjMatrix = Rr_M4D(1.0f);

    void UpdatePerspective(float Aspect)
    {
        ProjMatrix = Rr_Perspective_RH(
                         RR_ANGLE_DEG(FOVDegrees),
                         Aspect,
                         NEAR_PLANE,
                         FAR_PLANE) *
                     FLIP_Y_MATRIX;
    }

    [[nodiscard]] Rr_Mat4 GetViewMatrix() const
    {
        return Rr_InvGeneral(Transform);
    }

    [[nodiscard]] Rr_Vec3 GetForwardVector() const
    {
        return Rr_Norm(Transform.Columns[2].XYZ);
    }

    [[nodiscard]] Rr_Vec3 GetRightVector() const
    {
        return Rr_Norm(Transform.Columns[0].XYZ);
    }

    void Update(const UScancodes &Scancodes)
    {
        float DeltaTime = Rr_GetDeltaSeconds();

        Rr_Vec2 MouseDelta = Rr_GetMousePositionDelta();

        if (Rr_GetMouseState() & RR_MOUSE_BUTTON_RIGHT_BIT)
        {
            Rr_SetRelativeMouseMode(true);

            constexpr float CameraSpeed = 5.0f;
            Rr_Vec3 CameraForward = GetForwardVector();
            Rr_Vec3 CameraLeft = GetRightVector();
            if (Scancodes[RR_SCANCODE_W])
            {
                Position -= CameraForward * CameraSpeed * DeltaTime;
            }
            if (Scancodes[RR_SCANCODE_A])
            {
                Position -= CameraLeft * CameraSpeed * DeltaTime;
            }
            if (Scancodes[RR_SCANCODE_S])
            {
                Position += CameraForward * CameraSpeed * DeltaTime;
            }
            if (Scancodes[RR_SCANCODE_D])
            {
                Position += CameraLeft * CameraSpeed * DeltaTime;
            }

            constexpr float Sensitivity = 0.2f;
            Yaw -= MouseDelta.X * Sensitivity;
            Pitch -= MouseDelta.Y * Sensitivity;
        }
        else
        {
            Rr_SetRelativeMouseMode(false);
        }

        Yaw = Rr_WrapMax(Yaw, 360.0f);
        Pitch = RR_CLAMP(-90.0f, Pitch, 90.0f);

        Transform = Rr_Translate(Position) *
                    Rr_Rotate_RH(RR_ANGLE_DEG(Yaw), Rr_V3(0.0f, 1.0f, 0.0f)) *
                    Rr_Rotate_RH(RR_ANGLE_DEG(Pitch), Rr_V3(1.0f, 0.0f, 0.0f));
    }
};

struct SFullscreenBlit
{
    Rr_GraphicsPipeline *GraphicsPipeline{};
    Rr_Sampler *Sampler{};

    void Blit(Rr_Graph *Graph, Rr_Image2D *SrcImage, Rr_Image2D *DstImage)
    {
        Rr_ColorTarget ColorTarget = {
            .LoadOp = RR_LOAD_OP_DONT_CARE,
            .StoreOp = RR_STORE_OP_STORE,
            .Image = DstImage,
        };
        Rr_GraphNode *GraphicsNode =
            Rr_AddGraphicsNode(Graph, 1, &ColorTarget, nullptr);
        Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        Rr_BindCombinedImage2DSampler(GraphicsNode, SrcImage, Sampler, 0, 0);
        Rr_Draw(GraphicsNode, 3, 1, 0, 0);
    }

    SFullscreenBlit(Rr_PipelineLayout *PipelineLayout, Rr_AssetRef FragSPV)
    {
        Rr_SamplerInfo Info = {};
        Sampler = Rr_CreateSampler(&Info);

        Rr_ColorTargetInfo ColorTarget = {};
        ColorTarget.Format = Rr_GetSwapchainFormat();

        Rr_GraphicsPipelineCreateInfo PipelineInfo = {};
        PipelineInfo.Layout = PipelineLayout;
        PipelineInfo.VertexShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_FULLSCREENTRIANGLE_VERT_SPV);
        PipelineInfo.FragmentShaderSPV = Rr_LoadAsset(FragSPV);
        PipelineInfo.ColorTargetCount = 1;
        PipelineInfo.ColorTargets = &ColorTarget;
        PipelineInfo.Rasterizer.CullMode = RR_CULL_MODE_NONE;

        GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);
    }

    ~SFullscreenBlit()
    {
        Rr_ReleaseSampler(Sampler);
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
    }
};

struct SSkybox
{
    struct SGPUUniform
    {
        Rr_Mat4 View;
        Rr_Mat4 Projection;
        float Time;
    };

    Rr_PipelineLayout *PipelineLayout{};
    Rr_GraphicsPipeline *GraphicsPipeline{};

    Rr_Buffer *UniformBuffer{};
    Rr_Buffer *StagingBuffer{};
    Rr_Sampler *Sampler{};
    Rr_GLTFContext *GLTFContext{};
    Rr_GLTFAsset *GLTFAsset{};

    static constexpr std::array VertexAttributes = {
        Rr_VertexInputAttribute{ .Location = 0, .Format = RR_FORMAT_VEC3 },
    };

    static constexpr std::array VertexInputBindings = {
        Rr_VertexInputBinding{
            .Rate = RR_VERTEX_INPUT_RATE_VERTEX,
            .AttributeCount = VertexAttributes.size(),
            .Attributes = VertexAttributes.data(),
        },
    };

    void RecreatePipeline(uint32_t MSAASampleCount)
    {
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);

        Rr_ColorTargetInfo ColorTarget = {};
        ColorTarget.Format = Rr_GetSwapchainFormat();
        ColorTarget.Blend = Rr_AlphaBlend();

        Rr_GraphicsPipelineCreateInfo PipelineInfo = {};
        PipelineInfo.Layout = PipelineLayout;
        PipelineInfo.VertexShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_SKYBOX_VERT_SPV);
        PipelineInfo.FragmentShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_SKYBOX_FRAG_SPV);
        PipelineInfo.ColorTargetCount = 1;
        PipelineInfo.ColorTargets = &ColorTarget;
        PipelineInfo.Rasterizer.CullMode = RR_CULL_MODE_NONE;
        PipelineInfo.VertexInputBindingCount = VertexInputBindings.size();
        PipelineInfo.VertexInputBindings = VertexInputBindings.data();
        PipelineInfo.Multisampling.SampleCount = MSAASampleCount;

        GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);
    }

    void InitUniformBuffer()
    {
        UniformBuffer = Rr_CreateBuffer(
            sizeof(SGPUUniform),
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_MAPPED_BIT |
                RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT);
    }

    void InitSampler()
    {
        Rr_SamplerInfo Info = {};
        Info.MinFilter = RR_FILTER_LINEAR;
        Info.MagFilter = RR_FILTER_LINEAR;
        Sampler = Rr_CreateSampler(&Info);
    }

    void InitSkyboxMesh()
    {
        std::array GLTFAttributeTypes = {
            RR_GLTF_ATTRIBUTE_TYPE_POSITION,
        };

        Rr_GLTFVertexInputBinding GLTFVertexInputBinding = {
            .AttributeTypeCount = RR_ARRAY_COUNT(GLTFAttributeTypes),
            .AttributeTypes = GLTFAttributeTypes.data(),
        };
        GLTFContext = Rr_CreateGLTFContext(
            VertexInputBindings.size(),
            VertexInputBindings.data(),
            &GLTFVertexInputBinding,
            0,
            NULL);

        Rr_Asset LoadedAsset = Rr_LoadAsset(EXAMPLE_ASSET_SKYBOX_GLB);
        GLTFAsset = Rr_CreateGLTFAsset(
            GLTFContext,
            Rr_GetGraph(),
            LoadedAsset.Size,
            LoadedAsset.Pointer);
    }

    void Draw(
        Rr_Graph *Graph,
        Rr_Image2D *ColorImage,
        const SCamera &Camera,
        Rr_ImageCube *ImageCube)
    {
        SGPUUniform Uniform = {
            .View = Camera.GetViewMatrix(),
            .Projection = Camera.ProjMatrix,
        };
        std::memcpy(
            Rr_GetMappedBufferData(UniformBuffer),
            &Uniform,
            sizeof(SGPUUniform));

        Rr_ColorTarget ColorTarget = {
            .Slot = 0,
            .LoadOp = RR_LOAD_OP_DONT_CARE,
            .StoreOp = RR_STORE_OP_STORE,
            .Image = ColorImage,
        };
        Rr_GraphNode *GraphicsNode =
            Rr_AddGraphicsNode(Graph, 1, &ColorTarget, nullptr);

        Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        Rr_BindVertexBuffer(
            GraphicsNode,
            GLTFAsset->Buffer,
            0,
            GLTFAsset->VertexBufferOffset);
        Rr_BindIndexBuffer(
            GraphicsNode,
            GLTFAsset->Buffer,
            0,
            GLTFAsset->IndexBufferOffset,
            GLTFAsset->IndexType);
        Rr_BindUniformBuffer(
            GraphicsNode,
            UniformBuffer,
            0,
            0,
            0,
            sizeof(SGPUUniform));
        Rr_BindCombinedImageCubeSampler(GraphicsNode, ImageCube, Sampler, 0, 1);
        Rr_GLTFPrimitive *GLTFPrimitive = GLTFAsset->Meshes->Primitives;
        Rr_DrawIndexed(GraphicsNode, GLTFPrimitive->IndexCount, 1, 0, 0, 0);
    }

    SSkybox(uint32_t MSAASampleCount)
    {
        std::array Bindings = {
            Rr_Binding{
                0,
                RR_BINDING_TYPE_UNIFORM_BUFFER,
                RR_SHADER_STAGE_VERTEX_BIT | RR_SHADER_STAGE_FRAGMENT_BIT,
            },
            Rr_Binding{
                1,
                RR_BINDING_TYPE_COMBINED_IMAGE_SAMPLER,
                RR_SHADER_STAGE_VERTEX_BIT | RR_SHADER_STAGE_FRAGMENT_BIT,
            },
        };
        std::array Sets = {
            Rr_BindingSet{ Bindings.size(), Bindings.data() },
        };
        PipelineLayout =
            Rr_CreatePipelineLayout((uint32_t)Sets.size(), Sets.data());

        RecreatePipeline(MSAASampleCount);
        InitUniformBuffer();
        InitSampler();
        InitSkyboxMesh();
    }

    ~SSkybox()
    {
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
        Rr_ReleasePipelineLayout(PipelineLayout);
        Rr_ReleaseBuffer(UniformBuffer);
        Rr_ReleaseBuffer(StagingBuffer);
        Rr_ReleaseSampler(Sampler);
        Rr_ReleaseGLTFContext(GLTFContext);
    }
};

struct SGrid
{
    struct SGPUUniform
    {
        Rr_Mat4 View;
        Rr_Mat4 Projection;
        float Near;
        float Far;
        float GridSmall;
        float GridBig;
    };

    Rr_PipelineLayout *PipelineLayout{};
    Rr_GraphicsPipeline *GraphicsPipeline{};

    Rr_Buffer *UniformBuffer{};

    void RecreatePipeline(uint32_t SampleCount = 1)
    {
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);

        Rr_ColorTargetInfo ColorTarget = {};
        ColorTarget.Format = Rr_GetSwapchainFormat();
        ColorTarget.Blend = Rr_AlphaBlend();

        Rr_GraphicsPipelineCreateInfo PipelineInfo = {};
        PipelineInfo.Layout = PipelineLayout;
        PipelineInfo.VertexShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_GRID_VERT_SPV);
        PipelineInfo.FragmentShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_GRID_FRAG_SPV);
        PipelineInfo.ColorTargetCount = 1;
        PipelineInfo.ColorTargets = &ColorTarget;
        PipelineInfo.DepthStencil.EnableDepthTest = true;
        PipelineInfo.DepthStencil.EnableDepthWrite = true;
        PipelineInfo.DepthStencil.CompareOp = RR_COMPARE_OP_LESS;
        PipelineInfo.DepthStencil.Format = DEPTH_FORMAT;
        PipelineInfo.Multisampling.SampleCount = SampleCount;

        GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);
    }

    void Draw(
        const SCamera &Camera,
        Rr_Image2D *ColorImage,
        Rr_Image2D *DepthImage)
    {
        SGPUUniform Uniform = {
            .View = Camera.GetViewMatrix(),
            .Projection = Camera.ProjMatrix,
            .Near = NEAR_PLANE,
            .Far = FAR_PLANE,
            .GridSmall = 1.0f,
            .GridBig = 10.0f,
        };
        std::memcpy(
            Rr_GetMappedBufferData(UniformBuffer),
            &Uniform,
            sizeof(SGPUUniform));

        Rr_ColorTarget ColorTarget = {
            .LoadOp = RR_LOAD_OP_LOAD,
            .StoreOp = RR_STORE_OP_STORE,
            .Image = ColorImage,
        };
        Rr_DepthTarget DepthTarget = {
            .LoadOp = RR_LOAD_OP_LOAD,
            .StoreOp = RR_STORE_OP_STORE,
            .Image = DepthImage,
        };
        Rr_GraphNode *GraphicsNode =
            Rr_AddGraphicsNode(Rr_GetGraph(), 1, &ColorTarget, &DepthTarget);
        Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        Rr_BindUniformBuffer(
            GraphicsNode,
            UniformBuffer,
            0,
            0,
            0,
            sizeof(SGPUUniform));
        Rr_Draw(GraphicsNode, 6, 1, 0, 0);
    }

    SGrid(uint32_t MSAASampleCount)
    {
        UniformBuffer = Rr_CreateBuffer(
            sizeof(SGPUUniform),
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_MAPPED_BIT |
                RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT);

        std::array Bindings = {
            Rr_Binding{
                0,
                RR_BINDING_TYPE_UNIFORM_BUFFER,
                RR_SHADER_STAGE_VERTEX_BIT | RR_SHADER_STAGE_FRAGMENT_BIT,
            },
        };
        std::array Sets = {
            Rr_BindingSet{ Bindings.size(), Bindings.data() },
        };
        PipelineLayout =
            Rr_CreatePipelineLayout((uint32_t)Sets.size(), Sets.data());

        RecreatePipeline(MSAASampleCount);
    }

    ~SGrid()
    {
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
        Rr_ReleasePipelineLayout(PipelineLayout);
        Rr_ReleaseBuffer(UniformBuffer);
    }
};

struct SLighting
{
    static constexpr Rr_TextureFormat SHADOW_MAP_DEPTH_FORMAT =
        RR_TEXTURE_FORMAT_D32_SFLOAT;
    static constexpr std::int32_t POINT_SHADOW_MAP_SIZE = 2048;
    static constexpr std::int32_t SPOT_SHADOW_MAP_SIZE = 4096;

    struct SGPUPointLight
    {
        Rr_Vec3 Position;
        float Energy;
        Rr_Vec3 Color;
        float Specular;
        float Radius;
        float Intensity;
        float Falloff;
        float ConstantBias;
        float SlopeBias;
        float NormalBias;
        float LightSize;
        float TexelSize;
        float NearPlane;
        float FarPlane;
        Rr_Vec2 Padding;
    };

    struct SGPUSpotLight
    {
        Rr_Mat4 Transform;
        Rr_Mat4 ViewProjection;
        Rr_Vec3 Color;
        float Energy;
        Rr_Vec3 Padding;
        float Specular;
        float Intensity;
        float InnerCone;
        float OuterCone;
        float ConstantBias;
        float SlopeBias;
        float NormalBias;
        float LightSize;
        float TexelSize;
    };

    struct SGPUUniform
    {
        Rr_Mat4 ViewProjection;
        Rr_Vec3 LightPosition;
        float FarPlane;
    };

    Rr_PipelineLayout *PipelineLayout{};
    Rr_GraphicsPipeline *ShadowPipeline{};
    Rr_Sampler *ShadowSampler{};
    Rr_Sampler *RegularSampler{};
    Rr_Buffer *UniformBuffer{};

    Rr_Buffer *PointLightsBuffer{};
    std::vector<SGPUPointLight> PointLights{};
    std::vector<Rr_ImageCube *> PointShadowMaps{};

    Rr_Buffer *SpotLightsBuffer{};
    std::vector<SGPUSpotLight> SpotLights{};
    std::vector<Rr_Image2D *> SpotShadowMaps{};

    Rr_ImageCube *VisualizePointShadowMap{};

    void AddPointLight()
    {
        SGPUPointLight PointLight = {
            .Position = Rr_V3(0.0f, 1.0f, 0.0f),
            .Energy = 1.0f,
            .Color = Rr_V3(1.0f, 1.0f, 1.0f),
            .Specular = 0.5f,
            .Radius = 2.5f,
            .Intensity = 3.8f,
            .Falloff = 1.4f,
            .ConstantBias = 0.000f,
            .SlopeBias = 0.000f,
            .NormalBias = 0.000f,
            .LightSize = 0.0037f,
            .TexelSize = 1.0f / (float)POINT_SHADOW_MAP_SIZE,
            .NearPlane = NEAR_PLANE,
            .FarPlane = FAR_PLANE,
        };
        PointLights.emplace_back(PointLight);

        Rr_SetNextObjectName("PointShadowMap");
        PointShadowMaps.emplace_back(Rr_CreateImageCube(
            { POINT_SHADOW_MAP_SIZE, POINT_SHADOW_MAP_SIZE },
            SHADOW_MAP_DEPTH_FORMAT,
            RR_IMAGE_FLAGS_SAMPLED_BIT |
                RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT));
    }

    void AddSpotLight()
    {
        SGPUSpotLight SpotLight = {
            .Color = Rr_V3(1.0f, 1.0f, 1.0f),
            .Energy = 1.0f,
            .Specular = 0.5f,
            .Intensity = 1.0f,
            .InnerCone = 30.0f,
            .OuterCone = 75.0f,
            .ConstantBias = 0.455f,
            .SlopeBias = 5.0f,
            .NormalBias = 0.0091f,
            .LightSize = 0.0037f,
            .TexelSize = 1.0f / (float)SPOT_SHADOW_MAP_SIZE,
        };
        SpotLights.emplace_back(SpotLight);

        Rr_SetNextObjectName("SpotShadowMap");
        SpotShadowMaps.emplace_back(Rr_CreateImage2D(
            { SPOT_SHADOW_MAP_SIZE, SPOT_SHADOW_MAP_SIZE },
            SHADOW_MAP_DEPTH_FORMAT,
            RR_IMAGE_FLAGS_SAMPLED_BIT |
                RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT));
    }

    Rr_Mat4 GetCubeView(Rr_ImageCubeFace Face, Rr_Vec3 Position)
    {
        switch (Face)
        {
            case RR_IMAGE_CUBE_FACE_FRONT:
                return Rr_LookAt_RH(
                    Position,
                    Position + Rr_V3(1.0f, 0.0f, 0.0f),
                    Rr_V3(0.0f, 1.0f, 0.0f));
            case RR_IMAGE_CUBE_FACE_BACK:
                return Rr_LookAt_RH(
                    Position,
                    Position + Rr_V3(-1.0f, 0.0f, 0.0f),
                    Rr_V3(0.0f, 1.0f, 0.0f));
            case RR_IMAGE_CUBE_FACE_UP:
                return Rr_LookAt_RH(
                    Position,
                    Position + Rr_V3(0.0f, 1.0f, 0.0f),
                    Rr_V3(0.0f, 0.0f, -1.0f));
            case RR_IMAGE_CUBE_FACE_DOWN:
                return Rr_LookAt_RH(
                    Position,
                    Position + Rr_V3(0.0f, -1.0f, 0.0f),
                    Rr_V3(0.0f, 0.0f, 1.0f));
            case RR_IMAGE_CUBE_FACE_RIGHT:
                return Rr_LookAt_RH(
                    Position,
                    Position + Rr_V3(0.0f, 0.0f, 1.0f),
                    Rr_V3(0.0f, 1.0f, 0.0f));
            case RR_IMAGE_CUBE_FACE_LEFT:
                return Rr_LookAt_RH(
                    Position,
                    Position + Rr_V3(0.0f, 0.0f, -1.0f),
                    Rr_V3(0.0f, 1.0f, 0.0f));
            default:
                std::unreachable();
        }
    }

    void UpdateLightBuffers()
    {
        for (auto &PointLight : PointLights)
        {
            // PointLight.DepthParams = Rr_V2(
            //     (NEAR_PLANE - FAR_PLANE) / (NEAR_PLANE * FAR_PLANE),
            //     1.0 / NEAR_PLANE);
        }
        std::memcpy(
            Rr_GetMappedBufferData(PointLightsBuffer),
            PointLights.data(),
            sizeof(SGPUPointLight) * PointLights.size());

        for (auto &SpotLight : SpotLights)
        {
            SpotLight.ViewProjection = Rr_Perspective_RH(
                                           RR_ANGLE_DEG(SpotLight.OuterCone),
                                           1.0f,
                                           0.5f,
                                           FAR_PLANE) *
                                       FLIP_Y_MATRIX *
                                       Rr_InvGeneral(SpotLight.Transform);
            // SpotLight.Width = 1.0f;
            // SpotLight.WidthUV = 0.8f;
            // SpotLight.WidthUV =
            //     SpotLight.Width /
            //     (2.0f * std::tan(RR_ANGLE_DEG(SpotLight.OuterCone) / 2.0f) *
            //      0.1f);
        }
        std::memcpy(
            Rr_GetMappedBufferData(SpotLightsBuffer),
            SpotLights.data(),
            sizeof(SGPUSpotLight) * SpotLights.size());
    }

    void Iterate(
        const SCamera &Camera,
        Rr_Graph *Graph,
        const std::function<void(Rr_GraphNode *Node)> &Callback)
    {
        UpdateLightBuffers();

        char *UniformData = (char *)Rr_GetMappedBufferData(UniformBuffer);
        std::size_t UniformOffset = 0;

        for (std::size_t Index = 0; Index < PointLights.size(); ++Index)
        {
            SGPUPointLight &Point = PointLights[Index];
            Rr_ImageCube *PointShadowMap = PointShadowMaps[Index];

            const Rr_Mat4 CubeFacePerspective = Rr_Perspective_RH(
                RR_ANGLE_DEG(90.0f),
                1.0f,
                Point.NearPlane,
                Point.FarPlane);

            for (std::uint32_t Face = 0; Face < RR_IMAGE_CUBE_FACE_COUNT;
                 ++Face)
            {
                constexpr Rr_Mat4 FLIP_XY_MATRIX = {
                    -1.0f, 0.0f,  0.0f, 0.0f, //
                    0.0f,  -1.0f, 0.0f, 0.0f, //
                    0.0f,  0.0f,  1.0f, 0.0f, //
                    0.0f,  0.0f,  0.0f, 1.0f, //
                };
                SGPUUniform Uniform = {
                    .ViewProjection =
                        CubeFacePerspective * FLIP_XY_MATRIX *
                        GetCubeView((Rr_ImageCubeFace)Face, Point.Position),
                    .LightPosition = Point.Position,
                    .FarPlane = FAR_PLANE,
                };
                std::memcpy(
                    UniformData + UniformOffset,
                    &Uniform,
                    sizeof(Uniform));

                Rr_DepthTarget DepthTarget = {
                    .LoadOp = RR_LOAD_OP_CLEAR,
                    .StoreOp = RR_STORE_OP_STORE,
                    .Clear = Rr_DepthClear(1.0f, 0),
                    .Image = PointShadowMap,
                    .ImageLayerIndex = Face,
                };
                Rr_SetNextNodeName(
                    Graph,
                    std::format("PointShadowMap#{}", Index).c_str());
                Rr_GraphNode *GraphicsNode =
                    Rr_AddGraphicsNode(Graph, 0, nullptr, &DepthTarget);
                Rr_BindGraphicsPipeline(GraphicsNode, ShadowPipeline);
                Rr_BindUniformBuffer(
                    GraphicsNode,
                    UniformBuffer,
                    0,
                    0,
                    UniformOffset,
                    sizeof(Uniform));
                Callback(GraphicsNode);

                UniformOffset +=
                    RR_ALIGN_POW2(sizeof(Uniform), Rr_GetUniformAlignment());
            }
        }

        for (std::size_t Index = 0; Index < SpotLights.size(); ++Index)
        {
            SGPUSpotLight &Spot = SpotLights[Index];
            Rr_Image2D *SpotShadowMap = SpotShadowMaps[Index];

            SGPUUniform Uniform = {
                .ViewProjection = Spot.ViewProjection,
                .LightPosition = Spot.Transform.Columns[3].XYZ,
                .FarPlane = FAR_PLANE,
            };
            std::memcpy(UniformData + UniformOffset, &Uniform, sizeof(Uniform));

            Rr_DepthTarget DepthTarget = {
                .LoadOp = RR_LOAD_OP_CLEAR,
                .StoreOp = RR_STORE_OP_STORE,
                .Clear = Rr_DepthClear(1.0f, 0),
                .Image = SpotShadowMap,
            };
            Rr_SetNextNodeName(
                Graph,
                std::format("SpotShadowMap#{}", Index).c_str());
            Rr_GraphNode *GraphicsNode =
                Rr_AddGraphicsNode(Graph, 0, nullptr, &DepthTarget);
            Rr_BindGraphicsPipeline(GraphicsNode, ShadowPipeline);
            Rr_BindUniformBuffer(
                GraphicsNode,
                UniformBuffer,
                0,
                0,
                UniformOffset,
                sizeof(Uniform));
            Callback(GraphicsNode);

            UniformOffset +=
                RR_ALIGN_POW2(sizeof(Uniform), Rr_GetUniformAlignment());
        }
    }

    void BindLights(Rr_GraphNode *GraphicsNode, std::uint32_t Set)
    {
        Rr_BindStorageBuffer(
            GraphicsNode,
            PointLightsBuffer,
            Set,
            0,
            0,
            sizeof(SGPUPointLight) * PointLights.size());
        for (std::uint32_t Index = 0; Index < MAX_POINT_LIGHTS; ++Index)
        {
            std::uint32_t ImageIndex = Index;
            if (ImageIndex >= PointLights.size())
            {
                ImageIndex = PointLights.size() - 1;
            }
            Rr_BindSampledImageCubeAt(
                GraphicsNode,
                PointShadowMaps[ImageIndex],
                Set,
                1,
                Index);
        }

        Rr_BindStorageBuffer(
            GraphicsNode,
            SpotLightsBuffer,
            Set,
            2,
            0,
            sizeof(SGPUSpotLight) * SpotLights.size());
        for (std::uint32_t Index = 0; Index < MAX_SPOT_LIGHTS; ++Index)
        {
            std::uint32_t ImageIndex = Index;
            if (ImageIndex >= SpotLights.size())
            {
                ImageIndex = SpotLights.size() - 1;
            }
            Rr_BindSampledImage2DAt(
                GraphicsNode,
                SpotShadowMaps[ImageIndex],
                Set,
                3,
                Index);
        }
        Rr_BindSampler(GraphicsNode, RegularSampler, 1, 4);
        Rr_BindSampler(GraphicsNode, ShadowSampler, 1, 5);
    }

    void UI()
    {
        // if (Rr_UIFold("Point Lights"))
        // {
        //     for (std::uint32_t Index = 0; Index < PointLights.size();
        //     ++Index)
        //     {
        //         Rr_UISeparator();
        //         auto &PointLight = PointLights[Index];
        //         bool Visualize =
        //             VisualizePointShadowMap == PointShadowMaps[Index];
        //         bool OldVisualize = Visualize;
        //         if (Rr_UICheckbox("Visualize Shadow Map", &Visualize))
        //         {
        //             VisualizePointShadowMap =
        //                 OldVisualize ? nullptr : PointShadowMaps[Index];
        //         }
        //         Rr_UISliderFloat("Radius", &PointLight.Radius, 0.0f, 8.0f);
        //         Rr_UISliderFloat(
        //             "Intensity",
        //             &PointLight.Intensity,
        //             0.0f,
        //             8.0f);
        //         Rr_UISliderFloat("Falloff", &PointLight.Falloff, 0.0f, 8.0f);
        //         Rr_UISliderFloat("Bias", &PointLight.Bias, 0.0001f, 0.1f);
        //     }
        // }

        if (Rr_UIFold("Spot Lights"))
        {
            for (std::uint32_t Index = 0; Index < SpotLights.size(); ++Index)
            {
                // Rr_UISeparator();
                auto &SpotLight = SpotLights[Index];
                Rr_UIInputFloat3(
                    "Position",
                    SpotLight.Transform.Columns[3].Elements);
                Rr_UISliderFloat(
                    "Inner Cone",
                    &SpotLight.InnerCone,
                    0.0f,
                    90.0f);
                Rr_UISliderFloat(
                    "Outer Cone",
                    &SpotLight.OuterCone,
                    0.0f,
                    90.0f);
                Rr_UISliderFloat("Intensity", &SpotLight.Intensity, 0.0f, 8.0f);
                // Rr_UISliderFloat("Falloff", &SpotLight.Falloff, 0.0f, 8.0f);
                Rr_UISliderFloat(
                    "LightSize",
                    &SpotLight.LightSize,
                    0.0001f,
                    0.5f);
                Rr_UISliderFloat(
                    "ConstantBias",
                    &SpotLight.ConstantBias,
                    0.0f,
                    1.0f);
                Rr_UISliderFloat(
                    "SlopeBias",
                    &SpotLight.SlopeBias,
                    0.0f,
                    15.0f);
                Rr_UISliderFloat(
                    "NormalBias",
                    &SpotLight.NormalBias,
                    0.0f,
                    1.0f);
            }
        }
    }

    SLighting()
    {
        Rr_SamplerInfo SamplerInfo = {};
        SamplerInfo.MinFilter = RR_FILTER_NEAREST;
        SamplerInfo.MagFilter = RR_FILTER_NEAREST;
        RegularSampler = Rr_CreateSampler(&SamplerInfo);

        SamplerInfo.CompareEnable = true;
        SamplerInfo.CompareOp = RR_COMPARE_OP_LESS_OR_EQUAL;
        SamplerInfo.MinFilter = RR_FILTER_LINEAR;
        SamplerInfo.MagFilter = RR_FILTER_LINEAR;
        ShadowSampler = Rr_CreateSampler(&SamplerInfo);

        std::array Bindings0 = {
            Rr_Binding{
                0,
                RR_BINDING_TYPE_UNIFORM_BUFFER,
                RR_SHADER_STAGE_FRAGMENT_BIT | RR_SHADER_STAGE_VERTEX_BIT,
            },
        };
        std::array Bindings1 = {
            Rr_Binding{
                0,
                RR_BINDING_TYPE_STORAGE_BUFFER,
                RR_SHADER_STAGE_VERTEX_BIT,
            },
        };
        std::array Sets = {
            Rr_BindingSet{ Bindings0.size(), Bindings0.data() },
            Rr_BindingSet{ Bindings1.size(), Bindings1.data() },
        };
        PipelineLayout =
            Rr_CreatePipelineLayout((uint32_t)Sets.size(), Sets.data());

        std::array VertexAttributes = {
            Rr_VertexInputAttribute{ .Location = 0, .Format = RR_FORMAT_VEC3 },
            Rr_VertexInputAttribute{ .Location = 1, .Format = RR_FORMAT_VEC2 },
            Rr_VertexInputAttribute{ .Location = 2, .Format = RR_FORMAT_VEC3 },
        };

        std::array VertexInputBindings = {
            Rr_VertexInputBinding{
                .Rate = RR_VERTEX_INPUT_RATE_VERTEX,
                .AttributeCount = VertexAttributes.size(),
                .Attributes = VertexAttributes.data(),
            },
        };

        Rr_GraphicsPipelineCreateInfo PipelineInfo = {};
        PipelineInfo.Layout = PipelineLayout;
        PipelineInfo.VertexShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_SHADOWMAP_VERT_SPV);
        PipelineInfo.FragmentShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_SHADOWMAP_FRAG_SPV);
        PipelineInfo.VertexInputBindingCount = VertexInputBindings.size();
        PipelineInfo.VertexInputBindings = VertexInputBindings.data();
        PipelineInfo.DepthStencil.EnableDepthTest = true;
        PipelineInfo.DepthStencil.EnableDepthWrite = true;
        PipelineInfo.DepthStencil.CompareOp = RR_COMPARE_OP_LESS;
        PipelineInfo.DepthStencil.Format = SHADOW_MAP_DEPTH_FORMAT;
        PipelineInfo.Rasterizer.FrontFace = RR_FRONT_FACE_COUNTER_CLOCKWISE;
        PipelineInfo.Rasterizer.CullMode = RR_CULL_MODE_BACK;

        ShadowPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

        UniformBuffer = Rr_CreateBuffer(
            RR_MEGABYTES(2),
            RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
                RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_UNIFORM_BIT);

        PointLightsBuffer = Rr_CreateBuffer(
            RR_MEGABYTES(2),
            RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
                RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_STORAGE_BIT);

        SpotLightsBuffer = Rr_CreateBuffer(
            RR_MEGABYTES(2),
            RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
                RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_STORAGE_BIT);
    }

    ~SLighting()
    {
        Rr_ReleaseSampler(RegularSampler);
        Rr_ReleaseSampler(ShadowSampler);
        Rr_ReleasePipelineLayout(PipelineLayout);
        Rr_ReleaseGraphicsPipeline(ShadowPipeline);
        Rr_ReleaseBuffer(PointLightsBuffer);
        Rr_ReleaseBuffer(SpotLightsBuffer);
        Rr_ReleaseBuffer(UniformBuffer);
        for (auto &ShadowMap : PointShadowMaps)
        {
            Rr_ReleaseImage(ShadowMap);
        }
        for (auto &ShadowMap : SpotShadowMaps)
        {
            Rr_ReleaseImage(ShadowMap);
        }
    }
};

struct SModernRenderingApp
{
    struct SGPUUniform
    {
        Rr_Mat4 View;
        Rr_Mat4 Projection;
        Rr_Vec3 CameraPosition;
        float Time;
    };

    struct SGPUStorage
    {
        Rr_Mat4 Model;
    };

    static constexpr std::array VertexAttributes = {
        Rr_VertexInputAttribute{ .Location = 0, .Format = RR_FORMAT_VEC3 },
        Rr_VertexInputAttribute{ .Location = 1, .Format = RR_FORMAT_VEC2 },
        Rr_VertexInputAttribute{ .Location = 2, .Format = RR_FORMAT_VEC3 },
    };

    static constexpr std::array VertexInputBindings = {
        Rr_VertexInputBinding{
            .Rate = RR_VERTEX_INPUT_RATE_VERTEX,
            .AttributeCount = VertexAttributes.size(),
            .Attributes = VertexAttributes.data(),
        },
    };

    Rr_PipelineLayout *PipelineLayout{};
    Rr_GraphicsPipeline *GraphicsPipeline{};
    Rr_Buffer *UniformBuffer{};
    Rr_Buffer *ModelBuffer{};
    Rr_GLTFContext *GLTFContext{};
    Rr_GLTFAsset *GLTFAsset{};
    Rr_Image2D *ColorImage{};
    Rr_Image2D *ColorImageResolved{};
    Rr_Image2D *DepthImage{};

    static constexpr std::array<const char *, 4> MSAA_OPTIONS = {
        "Disabled",
        "2 Samples",
        "4 Samples",
        "8 Samples",
    };
    std::uint32_t MSAAOptionIndex = 0;

    UScancodes Scancodes{};

    Rr_PipelineLayout *BlitLayout;
    SFullscreenBlit FullscreenBlit;

    SCamera Camera;
    SLighting Lighting;
    SSkybox Skybox;
    SGrid Grid;
    bool DrawGrid = true;

    uint32_t GetMSAASampleCount() const
    {
        return 1 << MSAAOptionIndex;
    }

    void InitPipelineLayout()
    {
        std::array Bindings0 = {
            Rr_Binding{
                .Index = 0,
                .Type = RR_BINDING_TYPE_UNIFORM_BUFFER,
                .Stages =
                    RR_SHADER_STAGE_FRAGMENT_BIT | RR_SHADER_STAGE_VERTEX_BIT,
            },
        };
        std::array Bindings1 = {
            Rr_Binding{
                .Index = 0,
                .Type = RR_BINDING_TYPE_STORAGE_BUFFER,
                .Stages = RR_SHADER_STAGE_FRAGMENT_BIT,
            },
            Rr_Binding{
                .Index = 1,
                .Type = RR_BINDING_TYPE_SAMPLED_IMAGE,
                .Stages = RR_SHADER_STAGE_FRAGMENT_BIT,
                .Count = MAX_POINT_LIGHTS,
            },
            Rr_Binding{
                .Index = 2,
                .Type = RR_BINDING_TYPE_STORAGE_BUFFER,
                .Stages = RR_SHADER_STAGE_FRAGMENT_BIT,
            },
            Rr_Binding{
                .Index = 3,
                .Type = RR_BINDING_TYPE_SAMPLED_IMAGE,
                .Stages = RR_SHADER_STAGE_FRAGMENT_BIT,
                .Count = MAX_POINT_LIGHTS,
            },
            Rr_Binding{
                .Index = 4,
                .Type = RR_BINDING_TYPE_SAMPLER,
                .Stages = RR_SHADER_STAGE_FRAGMENT_BIT,
            },
            Rr_Binding{
                .Index = 5,
                .Type = RR_BINDING_TYPE_SAMPLER,
                .Stages = RR_SHADER_STAGE_FRAGMENT_BIT,
            },
        };
        std::array Bindings2 = {
            Rr_Binding{
                .Index = 0,
                .Type = RR_BINDING_TYPE_STORAGE_BUFFER,
                .Stages = RR_SHADER_STAGE_VERTEX_BIT,
            },
        };
        std::array BindingSets = {
            Rr_BindingSet{
                .BindingCount = Bindings0.size(),
                .Bindings = Bindings0.data(),
            },
            Rr_BindingSet{
                .BindingCount = Bindings1.size(),
                .Bindings = Bindings1.data(),
            },
            Rr_BindingSet{
                .BindingCount = Bindings2.size(),
                .Bindings = Bindings2.data(),
            },
        };
        PipelineLayout =
            Rr_CreatePipelineLayout(BindingSets.size(), BindingSets.data());
    }

    void InitPipeline()
    {
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);

        std::array ColorTargets = {
            Rr_ColorTargetInfo{
                .Format = Rr_GetSwapchainFormat(),
                .Resolve = MSAAOptionIndex > 0,
            },
        };

        Rr_GraphicsPipelineCreateInfo PipelineInfo = { 0 };
        PipelineInfo.Layout = PipelineLayout;
        PipelineInfo.VertexShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_MODERNRENDERING_VERT_SPV);
        PipelineInfo.FragmentShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_MODERNRENDERING_FRAG_SPV);
        PipelineInfo.VertexInputBindingCount = VertexInputBindings.size();
        PipelineInfo.VertexInputBindings = VertexInputBindings.data();
        PipelineInfo.ColorTargetCount = ColorTargets.size();
        PipelineInfo.ColorTargets = ColorTargets.data();
        PipelineInfo.DepthStencil.EnableDepthTest = true;
        PipelineInfo.DepthStencil.EnableDepthWrite = true;
        PipelineInfo.DepthStencil.CompareOp = RR_COMPARE_OP_LESS;
        PipelineInfo.DepthStencil.Format = DEPTH_FORMAT;
        PipelineInfo.Rasterizer.FrontFace = RR_FRONT_FACE_COUNTER_CLOCKWISE;
        PipelineInfo.Rasterizer.CullMode = RR_CULL_MODE_BACK;
        PipelineInfo.Multisampling.SampleCount = GetMSAASampleCount();

        GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);
    }

    void InitAttachments()
    {
        Rr_ImageFlags SampleCountFlag = RR_IMAGE_FLAGS_SAMPLE_COUNT_1
                                        << MSAAOptionIndex;
        Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize();
        Rr_TextureFormat SwapchainFormat = Rr_GetSwapchainFormat();

        Rr_ReleaseImage(DepthImage);
        DepthImage = Rr_CreateImage2D(
            { SwapchainSize.X, SwapchainSize.Y },
            DEPTH_FORMAT,
            RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT | SampleCountFlag);

        Rr_ReleaseImage(ColorImage);
        ColorImage = Rr_CreateImage2D(
            { SwapchainSize.X, SwapchainSize.Y },
            Rr_GetSwapchainFormat(),
            RR_IMAGE_FLAGS_SAMPLED_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT |
                RR_IMAGE_FLAGS_COLOR_ATTACHMENT_BIT | SampleCountFlag);

        Rr_ReleaseImage(ColorImageResolved);
        ColorImageResolved = nullptr;
        if (GetMSAASampleCount() == 1)
        {
            return;
        }
        ColorImageResolved = Rr_CreateImage2D(
            { SwapchainSize.X, SwapchainSize.Y },
            Rr_GetSwapchainFormat(),
            RR_IMAGE_FLAGS_SAMPLED_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT);
    }

    void InitCamera()
    {
        Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize();
        float Aspect = (float)SwapchainSize.Width / SwapchainSize.Height;
        Camera.UpdatePerspective(Aspect);
    }

    void InitUniform()
    {
        UniformBuffer = Rr_CreateBuffer(
            sizeof(SGPUUniform),
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_MAPPED_BIT |
                RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT);
    }

    void UploadNodeStorage(
        char *&StagingData,
        Rr_GLTFNode *Node,
        Rr_Mat4 Transform)
    {
        if (Node->Mesh)
        {
            Transform = Transform * Node->Transform;
            std::memcpy(StagingData, &Transform, sizeof(SGPUStorage));
            StagingData += sizeof(SGPUStorage);
        }
        for (std::uint32_t Index = 0; Index < Node->ChildrenCount; ++Index)
        {
            UploadNodeStorage(StagingData, Node->Children[Index], Transform);
        }
    }

    void InitGLTFAsset()
    {
        std::array<Rr_GLTFAttributeType, 3> GLTFAttributeTypes = {
            RR_GLTF_ATTRIBUTE_TYPE_POSITION,
            RR_GLTF_ATTRIBUTE_TYPE_TEXCOORD0,
            RR_GLTF_ATTRIBUTE_TYPE_NORMAL,
        };
        Rr_GLTFVertexInputBinding GLTFVertexInputBinding = {
            .AttributeTypeCount = GLTFAttributeTypes.size(),
            .AttributeTypes = GLTFAttributeTypes.data(),
        };
        std::array GLTFTextureMappings = {
            Rr_GLTFTextureMapping{
                .Set = 0,
                .Binding = 1,
                .TextureType = RR_GLTF_TEXTURE_TYPE_COLOR,
            },
        };
        GLTFContext = Rr_CreateGLTFContext(
            VertexInputBindings.size(),
            VertexInputBindings.data(),
            &GLTFVertexInputBinding,
            GLTFTextureMappings.size(),
            GLTFTextureMappings.data());

        Rr_Asset LoadedAsset = Rr_LoadAsset(EXAMPLE_ASSET_CABIN_GLB);
        GLTFAsset = Rr_CreateGLTFAsset(
            GLTFContext,
            Rr_GetGraph(),
            LoadedAsset.Size,
            LoadedAsset.Pointer);

        ModelBuffer =
            Rr_CreateBuffer(RR_MEGABYTES(4), RR_BUFFER_FLAGS_STORAGE_BIT);

        Rr_Buffer *StagingBuffer = Rr_CreateBuffer(
            RR_MEGABYTES(4),
            RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT);
        char *StagingData = (char *)Rr_GetMappedBufferData(StagingBuffer);
        char *StagingDataStart = StagingData;

        Rr_GLTFScene *Scene = GLTFAsset->Scenes;
        for (std::uint32_t NodeIndex = 0; NodeIndex < Scene->NodeCount;
             ++NodeIndex)
        {
            UploadNodeStorage(
                StagingData,
                Scene->Nodes[NodeIndex],
                Rr_M4D(1.0f));
        }

        Rr_TransferNode *TransferNode = Rr_AddTransferNode(Rr_GetGraph());
        Rr_TransferBufferData(
            TransferNode,
            StagingData - StagingDataStart,
            StagingBuffer,
            0,
            ModelBuffer,
            0);

        Rr_ReleaseBuffer(StagingBuffer);
    }

    void Event(Rr_Event *Event)
    {
        switch (Event->Type)
        {
            case RR_EVENT_TYPE_SWAPCHAIN_CREATED:
            {
                InitAttachments();
                InitCamera();
                return;
            }
            case RR_EVENT_TYPE_KEY_DOWN:
            case RR_EVENT_TYPE_KEY_UP:
            {
                Scancodes[Event->Key.Scancode] = Event->Key.Down;
                return;
            }
            default:
                return;
        }
    }

    void DrawGLTFMesh(Rr_GraphNode *GraphicsNode, Rr_GLTFMesh *Mesh)
    {
        for (std::uint32_t PrimitiveIndex = 0;
             PrimitiveIndex < Mesh->PrimitiveCount;
             ++PrimitiveIndex)
        {
            Rr_GLTFPrimitive *Primitive = &Mesh->Primitives[PrimitiveIndex];
            // Rr_BindSampler(GraphicsNode, NearestSampler, 0, 1);
            // Rr_BindSampledImage2D(GraphicsNode, GLTFAsset->Images[0], 0, 2);
            Rr_DrawIndexed(
                GraphicsNode,
                Primitive->IndexCount,
                1,
                Primitive->FirstIndex,
                Primitive->VertexOffset,
                0);
        }
    }

    void DrawGLTFNode(
        Rr_GraphNode *GraphicsNode,
        std::size_t &StorageIndex,
        Rr_GLTFNode *Node,
        uint32_t ModelSet,
        uint32_t ModelBinding)
    {
        if (Node->Mesh)
        {
            Rr_BindStorageBuffer(
                GraphicsNode,
                ModelBuffer,
                ModelSet,
                ModelBinding,
                StorageIndex * sizeof(SGPUStorage),
                sizeof(SGPUStorage));
            DrawGLTFMesh(GraphicsNode, Node->Mesh);
            StorageIndex++;
        }
        for (std::uint32_t Index = 0; Index < Node->ChildrenCount; ++Index)
        {
            DrawGLTFNode(
                GraphicsNode,
                StorageIndex,
                Node->Children[Index],
                ModelSet,
                ModelBinding);
        }
    }

    void DrawGLTFAsset(
        Rr_GraphNode *GraphicsNode,
        uint32_t ModelSet,
        uint32_t ModelBinding)
    {
        if (GLTFAsset->SceneCount == 0)
        {
            return;
        }

        Rr_BindVertexBuffer(
            GraphicsNode,
            GLTFAsset->Buffer,
            0,
            GLTFAsset->VertexBufferOffset);
        Rr_BindIndexBuffer(
            GraphicsNode,
            GLTFAsset->Buffer,
            0,
            GLTFAsset->IndexBufferOffset,
            GLTFAsset->IndexType);

        Rr_GLTFScene *Scene = &GLTFAsset->Scenes[0];

        std::size_t StorageIndex = 0;
        for (std::uint32_t NodeIndex = 0; NodeIndex < Scene->NodeCount;
             ++NodeIndex)
        {
            DrawGLTFNode(
                GraphicsNode,
                StorageIndex,
                Scene->Nodes[NodeIndex],
                ModelSet,
                ModelBinding);
        }
    }

    void UI()
    {
        Rr_UIBeginWindow("ModernRendering.cxx", NULL, 0);
        Rr_UIBeginTabs("Tabs");
        if (Rr_UITab("General"))
        {
            Rr_UIInputFloat3("Camera Position", Camera.Position.Elements);
            Rr_Vec3 CameraForward = Camera.GetForwardVector();
            Rr_UIInputFloat3("Camera Forward", CameraForward.Elements);
            Rr_UISeparator();
            Rr_UICheckbox("Draw Grid", &DrawGrid);
            if (Rr_UICombobox(
                    "MSAA",
                    MSAA_OPTIONS.size(),
                    MSAA_OPTIONS.data(),
                    &MSAAOptionIndex))
            {
                Skybox.RecreatePipeline(GetMSAASampleCount());
                Grid.RecreatePipeline(GetMSAASampleCount());
                InitAttachments();
                InitPipeline();
            }
        }
        if (Rr_UITab("Lighting"))
        {
            Lighting.UI();
        }
        Rr_UIEndTabs();
        Rr_UIEndWindow();
    }

    void Iterate()
    {
        // Rr_UIDebugOverlay();

        UI();

        Camera.Update(Scancodes);

        Rr_Graph *Graph = Rr_GetGraph();

        if (!Scancodes[RR_SCANCODE_SPACE])
        {
            Lighting.PointLights[0].Position.X =
                std::cosf(Rr_GetTimeSeconds()) / 2.0f;
            Lighting.PointLights[0].Position.Z =
                std::sinf(Rr_GetTimeSeconds()) / 2.0f;

            Lighting.SpotLights[0].Transform =
                Rr_Rotate_RH(
                    // Rr_GetTimeSeconds() / 2.0f,
                    RR_ANGLE_DEG(115.0f),
                    Rr_V3(0.0f, 1.0f, 0.0f)) *
                Rr_Translate(Rr_V3(
                    // std::cosf(Rr_GetTimeSeconds()) / 2.0f,
                    0.0f,
                    1.0f,
                    0.0f
                    // std::sinf(Rr_GetTimeSeconds()) / 2.0f)
                    ));
        }

        Lighting.Iterate(Camera, Graph, [&](Rr_GraphNode *Node) {
            DrawGLTFAsset(Node, 1, 0);
        });

        Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize();
        Rr_Image2D *SwapchainImage = Rr_GetSwapchainImage();

        /// MAIN PASS
        Rr_ClearColorImage2D(
            Graph,
            { Rr_V4(0.005f, 0.007f, 0.015f, 1.0f) },
            ColorImage);

        if (Lighting.VisualizePointShadowMap)
        {
            Skybox.Draw(
                Graph,
                ColorImage,
                Camera,
                Lighting.VisualizePointShadowMap);
        }

        Rr_ColorTarget ColorTarget = {
            .Slot = 0,
            .LoadOp = RR_LOAD_OP_LOAD,
            .StoreOp = RR_STORE_OP_STORE,
            .Image = ColorImage,
        };
        Rr_DepthTarget DepthTarget = {
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
            .Clear = Rr_DepthClear(1.0f, 0),
            .Image = DepthImage,
        };
        Rr_GraphNode *GraphicsNode =
            Rr_AddGraphicsNode(Graph, 1, &ColorTarget, &DepthTarget);

        SGPUUniform Uniform = {
            .View = Camera.GetViewMatrix(),
            .Projection = Camera.ProjMatrix,
            .CameraPosition = Camera.Position,
            .Time = (float)Rr_GetTimeSeconds(),
        };
        std::memcpy(
            Rr_GetMappedBufferData(UniformBuffer),
            &Uniform,
            sizeof(SGPUUniform));

        Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        Rr_BindUniformBuffer(
            GraphicsNode,
            UniformBuffer,
            0,
            0,
            0,
            sizeof(SGPUUniform));
        Lighting.BindLights(GraphicsNode, 1);
        DrawGLTFAsset(GraphicsNode, 2, 0);

        if (DrawGrid)
        {
            Grid.Draw(Camera, ColorImage, DepthImage);
        }

        if (GetMSAASampleCount() > 1)
        {
            Rr_ResolveImage2D(
                Graph,
                ColorImage,
                0,
                ColorImageResolved,
                0,
                RR_IMAGE_ASPECT_COLOR_BIT);
            FullscreenBlit.Blit(Graph, ColorImageResolved, SwapchainImage);
        }
        else
        {
            FullscreenBlit.Blit(Graph, ColorImage, SwapchainImage);
        }
    }

    Rr_PipelineLayout *CreateBlitLayout()
    {
        std::array Bindings0 = {
            Rr_Binding{
                .Type = RR_BINDING_TYPE_COMBINED_IMAGE_SAMPLER,
                .Stages = RR_SHADER_STAGE_FRAGMENT_BIT,
            },
        };
        std::array BindingSets = {
            Rr_BindingSet{
                .BindingCount = Bindings0.size(),
                .Bindings = Bindings0.data(),
            },
        };
        return Rr_CreatePipelineLayout(BindingSets.size(), BindingSets.data());
    }

    SModernRenderingApp()
        : BlitLayout(CreateBlitLayout())
        , FullscreenBlit(BlitLayout, EXAMPLE_ASSET_FULLSCREENTRIANGLE_FRAG_SPV)
        , Grid(GetMSAASampleCount())
        , Skybox(GetMSAASampleCount())
    {
        Lighting.AddPointLight();
        Lighting.AddSpotLight();
        InitAttachments();
        InitPipelineLayout();
        InitPipeline();
        InitGLTFAsset();
        InitUniform();
        InitCamera();
        Camera.Position = Rr_V3(0.0f, 1.0f, 0.0f);
    }

    ~SModernRenderingApp()
    {
        Rr_ReleasePipelineLayout(BlitLayout);
        Rr_ReleaseBuffer(UniformBuffer);
        Rr_ReleaseBuffer(ModelBuffer);
        Rr_ReleaseGLTFContext(GLTFContext);
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
        Rr_ReleasePipelineLayout(PipelineLayout);
        Rr_ReleaseImage(DepthImage);
        Rr_ReleaseImage(ColorImage);
        Rr_ReleaseImage(ColorImageResolved);
    }
};

int main()
{
    static SModernRenderingApp *App{};

    Rr_AppConfig Config = {};
    Config.Title = "ModernRendering";
    Config.WindowFlags |= RR_WINDOW_FLAGS_RESIZE_BIT;
    Config.InitFunc = []() { App = new SModernRenderingApp(); };
    Config.EventFunc = [](Rr_Event *Event) { App->Event(Event); };
    Config.IterateFunc = []() { App->Iterate(); };
    Config.CleanupFunc = []() { delete App; };
    Rr_Run(&Config);
}
