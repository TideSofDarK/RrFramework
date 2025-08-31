#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include <array>
#include <print>
#include <vector>

using UScancodes = std::array<bool, RR_SCANCODE_COUNT>;

static constexpr Rr_TextureFormat DEPTH_FORMAT = RR_TEXTURE_FORMAT_D32_SFLOAT;

struct SCamera
{
    float Near = 0.01f;
    float Far = 100.0f;
    float FOVDegrees = 90.0f;
    float Pitch{};
    float Yaw{};
    Rr_Vec3 Position;

    Rr_Mat4 ViewMatrix = Rr_M4D(1.0f);
    Rr_Mat4 ProjMatrix = Rr_M4D(1.0f);

    void UpdatePerspective(float Aspect)
    {
        ProjMatrix =
            Rr_Perspective_RH(RR_ANGLE_DEG(FOVDegrees), Aspect, Near, Far);
    }

    [[nodiscard]] Rr_Vec3 GetForwardVector() const
    {
        return Rr_Norm(Rr_InvGeneral(ViewMatrix).Columns[2].XYZ);
    }

    [[nodiscard]] Rr_Vec3 GetRightVector() const
    {
        return Rr_Norm(Rr_InvGeneral(ViewMatrix).Columns[0].XYZ);
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
            Yaw += MouseDelta.X * Sensitivity;
            Pitch += MouseDelta.Y * Sensitivity;
        }
        else
        {
            Rr_SetRelativeMouseMode(false);
        }

        Yaw = Rr_WrapMax(Yaw, 360.0f);
        Pitch = RR_CLAMP(-90.0f, Pitch, 90.0f);

        float CosPitch = cosf(Pitch * RR_DEG_TO_RAD);
        float SinPitch = sinf(Pitch * RR_DEG_TO_RAD);
        float CosYaw = cosf(Yaw * RR_DEG_TO_RAD);
        float SinYaw = sinf(Yaw * RR_DEG_TO_RAD);

        Rr_Vec3 XAxis{ CosYaw, 0.0f, -SinYaw };
        Rr_Vec3 YAxis{ SinYaw * SinPitch, CosPitch, CosYaw * SinPitch };
        Rr_Vec3 ZAxis{ SinYaw * CosPitch, -SinPitch, CosPitch * CosYaw };

        ViewMatrix.Columns[0] = { XAxis.X, YAxis.X, ZAxis.X, 0.0f };
        ViewMatrix.Columns[1] = { XAxis.Y, YAxis.Y, ZAxis.Y, 0.0f };
        ViewMatrix.Columns[2] = { XAxis.Z, YAxis.Z, ZAxis.Z, 0.0f };
        ViewMatrix.Columns[3] = { -Rr_Dot(XAxis, Position),
                                  -Rr_Dot(YAxis, Position),
                                  -Rr_Dot(ZAxis, Position),
                                  1.0f };
        ViewMatrix = Rr_VulkanMatrix() * ViewMatrix;
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

    Rr_PipelineLayout *PipelineLayout;
    Rr_GraphicsPipeline *GraphicsPipeline;

    Rr_Buffer *UniformBuffer;

    void InitPipeline()
    {
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

        GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);
    }

    void InitUniformBuffer()
    {
        UniformBuffer = Rr_CreateBuffer(
            sizeof(SGPUUniform),
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_MAPPED_BIT |
                RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT);
    }

    void Init()
    {
        InitPipeline();
        InitUniformBuffer();
    }

    void Draw(const SCamera &Camera, Rr_GraphNode *GraphicsNode)
    {
        SGPUUniform Uniform = {
            .View = Camera.ViewMatrix,
            .Projection = Camera.ProjMatrix,
            .Near = Camera.Near,
            .Far = Camera.Far,
            .GridSmall = 1.0f,
            .GridBig = 10.0f,
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
        Rr_Draw(GraphicsNode, 6, 1, 0, 0);
    }

    void Cleanup()
    {
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
        Rr_ReleasePipelineLayout(PipelineLayout);
        Rr_ReleaseBuffer(UniformBuffer);
    }
};

struct SGPUPointLight
{
    Rr_Vec4 Position;
    Rr_Vec4 Ambient;
    Rr_Vec4 Diffuse;
    Rr_Vec4 Specular;
    float Constant;
    float Linear;
    float Quadratic;
    float Padding;
};

struct SLighting
{
    struct SGPUUniform
    {
        Rr_Vec4 LightPosition;
        float FarPlane;
    };

    struct SGPUStorage
    {
        Rr_Mat4 ViewProjection;
    };

    static constexpr std::int32_t MAP_WIDTH = 512;
    static constexpr std::int32_t MAP_HEIGHT = 512;

    Rr_PipelineLayout *PipelineLayout;
    Rr_GraphicsPipeline *GraphicsPipeline;
    Rr_Buffer *UniformBuffer;
    Rr_Buffer *LightsBuffer;
    std::vector<SGPUPointLight> PointLights;
    std::vector<Rr_ImageCube *> PointShadowMaps;

    void Init()
    {
        std::array Bindings = {
            Rr_Binding{
                0,
                RR_BINDING_TYPE_STORAGE_BUFFER,
                RR_SHADER_STAGE_FRAGMENT_BIT,
            },
            Rr_Binding{
                1,
                RR_BINDING_TYPE_STORAGE_BUFFER,
                RR_SHADER_STAGE_VERTEX_BIT,
            },
        };
        std::array Sets = {
            Rr_BindingSet{ Bindings.size(), Bindings.data() },
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
        PipelineInfo.DepthStencil.Format = DEPTH_FORMAT;

        GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

        UniformBuffer = Rr_CreateBuffer(
            RR_MEGABYTES(2),
            RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
                RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_UNIFORM_BIT);

        LightsBuffer = Rr_CreateBuffer(
            RR_MEGABYTES(2),
            RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
                RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_STORAGE_BIT);
    }

    void Cleanup()
    {
        Rr_ReleasePipelineLayout(PipelineLayout);
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
        Rr_ReleaseBuffer(LightsBuffer);
        Rr_ReleaseBuffer(UniformBuffer);
        for (auto &ShadowMap : PointShadowMaps)
        {
            Rr_ReleaseImage(ShadowMap);
        }
    }

    void AddPointLight()
    {
        SGPUPointLight PointLight = {
            .Position = Rr_V4(0.0f, 1.0f, 0.0f, 1.0f),
            .Ambient = Rr_V4(0.5f, 0.5f, 0.5f, 1.0f),
            .Diffuse = Rr_V4(0.5f, 0.5f, 0.5f, 1.0f),
            .Specular = Rr_V4(0.5f, 0.5f, 0.5f, 1.0f),
            .Constant = 1.0f,
            .Linear = 0.07f,
            .Quadratic = 0.017f,
        };
        PointLights.emplace_back(PointLight);

        Rr_ImageCube *ShadowMap = Rr_CreateImageCube(
            { MAP_WIDTH, MAP_HEIGHT },
            RR_TEXTURE_FORMAT_D32_SFLOAT,
            RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT |
                RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT);
        PointShadowMaps.emplace_back(ShadowMap);
    }

    void DrawShadowMaps(Rr_Graph *Graph)
    {
        Rr_DepthTarget DepthTarget = {
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
            .Clear = Rr_DepthClear(1.0f, 0),
        };
        for (std::size_t Index = 0; Index < PointLights.size(); ++Index)
        {
            SGPUPointLight &Point = PointLights[Index];
            Rr_ImageCube *PointShadowMap = PointShadowMaps[Index];

            Rr_GraphNode *GraphicsNode = Rr_AddGraphicsNode(
                Graph,
                "draw_shadow_maps",
                0,
                nullptr,
                nullptr,
                &DepthTarget,
                PointShadowMap);
            Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        }
    }

    void UpdateLightBuffer()
    {
        std::memcpy(
            Rr_GetMappedBufferData(LightsBuffer),
            PointLights.data(),
            sizeof(SGPUPointLight) * PointLights.size());
    }

    void BindLights(
        Rr_GraphNode *GraphicsNode,
        std::uint32_t Set,
        std::uint32_t Binding)
    {
        Rr_BindStorageBuffer(
            GraphicsNode,
            LightsBuffer,
            Set,
            Binding,
            0,
            sizeof(SGPUPointLight) * PointLights.size());
    }

    void UIPointLight(SGPUPointLight &PointLight)
    {
        Rr_UISliderFloat("Constant", &PointLight.Constant, 0.0f, 4.0f);
        Rr_UISliderFloat("Linear", &PointLight.Linear, 0.0f, 4.0f);
        Rr_UISliderFloat("Quadratic", &PointLight.Quadratic, 0.0f, 4.0f);
    }

    void UI()
    {
        for (auto &PointLight : PointLights)
        {
            UIPointLight(PointLight);
        }
    }
};

struct SModernRenderingApp
{
    struct SGPUUniform
    {
        Rr_Mat4 View;
        Rr_Mat4 Projection;
        float Time;
    };

    struct SGPUStorage
    {
        Rr_Mat4 Model;
    };

    Rr_PipelineLayout *PipelineLayout;
    Rr_GraphicsPipeline *GraphicsPipeline;
    Rr_Image2D *DepthImage;
    Rr_Buffer *UniformBuffer;
    Rr_Buffer *ModelBuffer;
    Rr_GLTFContext *GLTFContext;
    Rr_GLTFAsset *GLTFAsset;

    SLighting Lighting;
    SCamera Camera;
    SGrid Grid;

    UScancodes Scancodes{};

    void InitPipeline()
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

        Rr_ColorTargetInfo ColorTargets[1] = { 0 };
        ColorTargets[0].Format = Rr_GetSwapchainFormat();

        Rr_GraphicsPipelineCreateInfo PipelineInfo = { 0 };
        PipelineInfo.Layout = PipelineLayout;
        PipelineInfo.VertexShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_MODERNRENDERING_VERT_SPV);
        PipelineInfo.FragmentShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_MODERNRENDERING_FRAG_SPV);
        PipelineInfo.VertexInputBindingCount = VertexInputBindings.size();
        PipelineInfo.VertexInputBindings = VertexInputBindings.data();
        PipelineInfo.ColorTargetCount = 1;
        PipelineInfo.ColorTargets = ColorTargets;
        PipelineInfo.DepthStencil.EnableDepthTest = true;
        PipelineInfo.DepthStencil.EnableDepthWrite = true;
        PipelineInfo.DepthStencil.CompareOp = RR_COMPARE_OP_LESS;
        PipelineInfo.DepthStencil.Format = DEPTH_FORMAT;
        PipelineInfo.Rasterizer.FrontFace = RR_FRONT_FACE_CLOCKWISE;
        PipelineInfo.Rasterizer.CullMode = RR_CULL_MODE_BACK;

        GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

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
    }

    void InitDepthImage()
    {
        Rr_ReleaseImage(DepthImage);
        Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize();
        DepthImage = Rr_CreateImage2D(
            { SwapchainSize.X, SwapchainSize.Y },
            DEPTH_FORMAT,
            RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT);
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

        Rr_TransferNode *TransferNode =
            Rr_AddTransferNode(Rr_GetGraph(), "upload_storage_buffer");
        Rr_TransferBufferData(
            TransferNode,
            StagingData - StagingDataStart,
            StagingBuffer,
            0,
            ModelBuffer,
            0);

        Rr_ReleaseBuffer(StagingBuffer);
    }

    void Init()
    {
        Grid.Init();
        InitDepthImage();
        InitPipeline();
        InitGLTFAsset();
        InitUniform();
        InitCamera();
        Camera.Position = Rr_V3(0.0f, 1.0f, 0.0f);
        Lighting.Init();
        Lighting.AddPointLight();
    }

    void Event(Rr_Event *Event)
    {
        switch (Event->Type)
        {
            case RR_EVENT_TYPE_SWAPCHAIN_CREATED:
            {
                InitDepthImage();
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
        Rr_GLTFNode *Node)
    {
        if (Node->Mesh)
        {
            Rr_BindStorageBuffer(
                GraphicsNode,
                ModelBuffer,
                2,
                0,
                StorageIndex * sizeof(SGPUStorage),
                sizeof(SGPUStorage));
            DrawGLTFMesh(GraphicsNode, Node->Mesh);
            StorageIndex++;
        }
        for (std::uint32_t Index = 0; Index < Node->ChildrenCount; ++Index)
        {
            DrawGLTFNode(GraphicsNode, StorageIndex, Node->Children[Index]);
        }
    }

    void DrawGLTFAsset(Rr_GraphNode *GraphicsNode)
    {
        if (GLTFAsset->SceneCount == 0)
        {
            return;
        }

        SGPUUniform Uniform = {
            .View = Camera.ViewMatrix,
            .Projection = Camera.ProjMatrix,
            .Time = (float)Rr_GetTimeSeconds(),
        };
        std::memcpy(
            Rr_GetMappedBufferData(UniformBuffer),
            &Uniform,
            sizeof(SGPUUniform));

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
        Lighting.BindLights(GraphicsNode, 1, 0);

        Rr_GLTFScene *Scene = &GLTFAsset->Scenes[0];

        std::size_t StorageIndex = 0;
        for (std::uint32_t NodeIndex = 0; NodeIndex < Scene->NodeCount;
             ++NodeIndex)
        {
            DrawGLTFNode(GraphicsNode, StorageIndex, Scene->Nodes[NodeIndex]);
        }
    }

    void Iterate()
    {
        // Rr_UIDebugOverlay();

        Rr_UIBeginWindow("ModernRendering.cxx", NULL, 0);
        Lighting.UI();
        Rr_UIEndWindow();

        Lighting.UpdateLightBuffer();

        Rr_Graph *Graph = Rr_GetGraph();

        Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize();

        Camera.Update(Scancodes);

        Rr_Image2D *SwapchainImage = Rr_GetSwapchainImage();

        Rr_ColorClear ColorClear = {};
        ColorClear.Vec4 = {
            0.005f,
            0.007f,
            0.015f,
            1.0f,
        };
        Rr_ColorTarget ColorTarget = {
            .Slot = 0,
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
            .Clear = ColorClear,
        };
        Rr_DepthTarget DepthTarget = {
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
            .Clear = Rr_DepthClear(1.0f, 0),
        };
        Rr_GraphNode *GraphicsNode = Rr_AddGraphicsNode(
            Graph,
            "grid",
            1,
            &ColorTarget,
            &SwapchainImage,
            &DepthTarget,
            DepthImage);

        DrawGLTFAsset(GraphicsNode);

        Grid.Draw(Camera, GraphicsNode);
    }

    void Cleanup()
    {
        Rr_ReleaseBuffer(UniformBuffer);
        Rr_ReleaseBuffer(ModelBuffer);
        Rr_ReleaseGLTFContext(GLTFContext);
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
        Rr_ReleasePipelineLayout(PipelineLayout);
        Rr_ReleaseImage(DepthImage);
        Grid.Cleanup();
        Lighting.Cleanup();
    }
};

int main()
{
    static SModernRenderingApp App;

    Rr_AppConfig Config = {};
    Config.Title = "ModernRendering";
    Config.WindowFlags |= RR_WINDOW_FLAGS_RESIZE_BIT;
    Config.InitFunc = []() { App.Init(); };
    Config.EventFunc = [](Rr_Event *Event) { App.Event(Event); };
    Config.IterateFunc = []() { App.Iterate(); };
    Config.CleanupFunc = []() { App.Cleanup(); };
    Rr_Run(&Config);
}
