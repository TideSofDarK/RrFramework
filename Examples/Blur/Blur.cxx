#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include <array>
#include <print>

#include "../../Vendor/stb/stb_image.h"

using UScancodes = std::array<bool, RR_SCANCODE_COUNT>;

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

struct SGPUUniform
{
    Rr_Mat4 View;
    Rr_Mat4 Projection;
};

struct SPNGImage
{
    int32_t Channels;
    int32_t Width;
    int32_t Height;
    void *Data;

    SPNGImage(Rr_AssetRef AssetRef)
    {
        Rr_Asset Asset = Rr_LoadAsset(AssetRef);
        int32_t DesiredChannels = 4;
        Data = stbi_load_from_memory(
            (stbi_uc *)Asset.Pointer,
            (int32_t)Asset.Size,
            (int32_t *)&Width,
            (int32_t *)&Height,
            &Channels,
            DesiredChannels);
    }

    ~SPNGImage()
    {
        stbi_image_free(Data);
    }

    SPNGImage(const SPNGImage &) = delete;
    SPNGImage(SPNGImage &&) = delete;
    SPNGImage &operator=(const SPNGImage &) = delete;
    SPNGImage &operator=(SPNGImage &&) = delete;
};

struct SBlurCube
{
    Rr_PipelineLayout *PipelineLayout;
    Rr_ComputePipeline *BlurCubeXPipeline{};
    Rr_ComputePipeline *BlurCubeYPipeline{};
    Rr_ImageCube *IntermediateImage;

    Rr_TextureFormat Format;
    std::uint32_t ImageSize;
    std::uint32_t WorkgroupSize;

    void Blur(Rr_Graph *Graph, Rr_ImageCube *TargetImage, std::int32_t Passes)
    {
        Rr_SetNextNodeName(Graph, "BlurCube");
        Rr_GraphNode *Node = Rr_AddComputeNode(Graph);
        for (std::int32_t Index = 0; Index < Passes; ++Index)
        {
            Rr_BindComputePipeline(Node, BlurCubeXPipeline);
            Rr_BindStorageImage2DArray(Node, TargetImage, 0, 0);
            Rr_BindStorageImage2DArrayRW(Node, IntermediateImage, 0, 1);
            Rr_Dispatch(Node, 1, ImageSize / WorkgroupSize, 6);
            Rr_ComputeBarrier(Node);
            Rr_BindComputePipeline(Node, BlurCubeYPipeline);
            Rr_BindStorageImage2DArray(Node, IntermediateImage, 0, 0);
            Rr_BindStorageImage2DArrayRW(Node, TargetImage, 0, 1);
            Rr_Dispatch(Node, ImageSize / WorkgroupSize, 1, 6);
            Rr_ComputeBarrier(Node);
        }
    }

    Rr_ComputePipeline *CreateBlurPipeline(
        Rr_AssetRef ComputeSPV,
        std::uint32_t Radius)
    {
        std::array Specializations = {
            Rr_PipelineSpecialization{
                0,
                RR_MAKE_DATA_STRUCT(WorkgroupSize),
            },
            Rr_PipelineSpecialization{
                1,
                RR_MAKE_DATA_STRUCT(ImageSize),
            },
            Rr_PipelineSpecialization{
                2,
                RR_MAKE_DATA_STRUCT(Radius),
            },
        };

        Rr_ComputePipelineCreateInfo PipelineCreateInfo = {};
        PipelineCreateInfo.Layout = PipelineLayout;
        PipelineCreateInfo.ShaderSPV = Rr_LoadAsset(ComputeSPV);
        PipelineCreateInfo.SpecializationCount = Specializations.size();
        PipelineCreateInfo.Specializations = Specializations.data();

        return Rr_CreateComputePipeline(&PipelineCreateInfo);
    }

    void RecreatePipelines(std::uint32_t Radius)
    {
        Rr_ReleaseComputePipeline(BlurCubeXPipeline);
        BlurCubeXPipeline =
            CreateBlurPipeline(EXAMPLE_ASSET_BLURCUBEX_COMP_SPV, Radius);
        Rr_ReleaseComputePipeline(BlurCubeYPipeline);
        BlurCubeYPipeline =
            CreateBlurPipeline(EXAMPLE_ASSET_BLURCUBEY_COMP_SPV, Radius);
    }

    SBlurCube(
        Rr_TextureFormat Format,
        std::int32_t ImageSize,
        std::uint32_t Radius)
        : Format(Format)
        , ImageSize(ImageSize)
        , WorkgroupSize(std::sqrt(Rr_GetMaxComputeWorkgroupInvocations()))
    {
        std::array Bindings0 = {
            Rr_Binding{
                0,
                RR_BINDING_TYPE_STORAGE_IMAGE,
                RR_SHADER_STAGE_COMPUTE_BIT,
            },
            Rr_Binding{
                1,
                RR_BINDING_TYPE_STORAGE_IMAGE,
                RR_SHADER_STAGE_COMPUTE_BIT,
            },
        };
        std::array Sets = {
            Rr_BindingSet{ Bindings0.size(), Bindings0.data() },
        };
        PipelineLayout =
            Rr_CreatePipelineLayout((uint32_t)Sets.size(), Sets.data());

        RecreatePipelines(Radius);

        IntermediateImage = Rr_CreateImageCube(
            { ImageSize, ImageSize },
            Format,
            RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_STORAGE_BIT);
    }

    ~SBlurCube()
    {
        Rr_ReleasePipelineLayout(PipelineLayout);
        Rr_ReleaseComputePipeline(BlurCubeXPipeline);
        Rr_ReleaseComputePipeline(BlurCubeYPipeline);
        Rr_ReleaseImage(IntermediateImage);
    }
};

struct SBlurApp
{
    Rr_PipelineLayout *PipelineLayout;
    Rr_GraphicsPipeline *GraphicsPipeline;

    Rr_Buffer *UniformBuffer;
    Rr_Buffer *StagingBuffer;
    Rr_Sampler *Sampler;
    Rr_GLTFContext *GLTFContext;
    Rr_GLTFAsset *GLTFAsset;

    SCamera Camera;

    Rr_ImageCube *OriginalImageCube;
    Rr_ImageCube *BlurredImageCube;
    std::int32_t BlurCubeRadius = 2;
    std::int32_t BlurCubePasses = 2;
    SBlurCube BlurCube;

    UScancodes Scancodes{};

    void InitPipeline()
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

        std::array VertexAttributes = {
            Rr_VertexInputAttribute{ .Location = 0, .Format = RR_FORMAT_VEC3 },
        };

        std::array VertexInputBindings = {
            Rr_VertexInputBinding{
                .Rate = RR_VERTEX_INPUT_RATE_VERTEX,
                .AttributeCount = VertexAttributes.size(),
                .Attributes = VertexAttributes.data(),
            },
        };

        Rr_ColorTargetInfo ColorTarget = {};
        ColorTarget.Format = Rr_GetSwapchainFormat();
        ColorTarget.Blend = Rr_AlphaBlend();

        Rr_GraphicsPipelineCreateInfo PipelineInfo = {};
        PipelineInfo.Layout = PipelineLayout;
        PipelineInfo.VertexShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_BLUR_VERT_SPV);
        PipelineInfo.FragmentShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_BLUR_FRAG_SPV);
        PipelineInfo.ColorTargetCount = 1;
        PipelineInfo.ColorTargets = &ColorTarget;
        PipelineInfo.Rasterizer.CullMode = RR_CULL_MODE_NONE;
        PipelineInfo.VertexInputBindingCount = VertexInputBindings.size();
        PipelineInfo.VertexInputBindings = VertexInputBindings.data();

        GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

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
        Sampler = Rr_CreateSampler(&Info);
    }

    void InitCubemapImage()
    {
        SPNGImage Right{ EXAMPLE_ASSET_RIGHT_PNG };
        SPNGImage Left{ EXAMPLE_ASSET_LEFT_PNG };
        SPNGImage Up{ EXAMPLE_ASSET_UP_PNG };
        SPNGImage Down{ EXAMPLE_ASSET_DOWN_PNG };
        SPNGImage Front{ EXAMPLE_ASSET_FRONT_PNG };
        SPNGImage Back{ EXAMPLE_ASSET_BACK_PNG };

        int32_t Width = Up.Width;
        int32_t Height = Up.Height;

        int32_t LayerSize = Width * Height * 4;

        OriginalImageCube = Rr_CreateImageCube(
            { Width, Height },
            RR_TEXTURE_FORMAT_R8G8B8A8_UNORM,
            RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT |
                RR_IMAGE_FLAGS_STORAGE_BIT);

        StagingBuffer = Rr_CreateBuffer(
            Width * Height * 4 * 6,
            RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_STAGING_BIT);

        char *StagingData = (char *)Rr_GetMappedBufferData(StagingBuffer);
        std::memcpy(StagingData + (LayerSize * 0), Right.Data, LayerSize);
        std::memcpy(StagingData + (LayerSize * 1), Left.Data, LayerSize);
        std::memcpy(StagingData + (LayerSize * 2), Up.Data, LayerSize);
        std::memcpy(StagingData + (LayerSize * 3), Down.Data, LayerSize);
        std::memcpy(StagingData + (LayerSize * 4), Front.Data, LayerSize);
        std::memcpy(StagingData + (LayerSize * 5), Back.Data, LayerSize);

        Rr_CopyBufferToImageCubeEx(
            Rr_GetGraph(),
            StagingBuffer,
            (LayerSize * 0),
            { Width, Height },
            OriginalImageCube,
            RR_IMAGE_CUBE_FACE_FIRST,
            RR_IMAGE_CUBE_FACE_LAST,
            0);

        BlurredImageCube = Rr_CreateImageCube(
            { Width, Height },
            RR_TEXTURE_FORMAT_R8G8B8A8_UNORM,
            RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT |
                RR_IMAGE_FLAGS_STORAGE_BIT);

        Rr_CopyImageCube(Rr_GetGraph(), OriginalImageCube, BlurredImageCube, 0);

        BlurCube.Blur(Rr_GetGraph(), BlurredImageCube, BlurCubePasses);
    }

    void InitSkyboxMesh()
    {
        Rr_Asset LoadedAsset = Rr_LoadAsset(EXAMPLE_ASSET_CUBE_GLB);
        GLTFAsset = Rr_CreateGLTFAsset(
            GLTFContext,
            Rr_GetGraph(),
            LoadedAsset.Size,
            LoadedAsset.Pointer);
    }

    void InitCamera()
    {
        Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize();
        float Aspect = (float)SwapchainSize.Width / SwapchainSize.Height;
        Camera.UpdatePerspective(Aspect);
    }

    void Event(Rr_Event *Event)
    {
        switch (Event->Type)
        {
            case RR_EVENT_TYPE_SWAPCHAIN_CREATED:
            {
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

    void Iterate()
    {
        // Rr_UIDebugOverlay();

        Rr_UIBeginWindow("Blur.cxx", NULL, RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT);
        Rr_UILabel(
            "This example demonstrates bluring of Rr_Image2D and Rr_ImageCube "
            "textures.");
        if (Rr_UISliderInt("Blur Radius", &BlurCubeRadius, 2, 16))
        {
            BlurCube.RecreatePipelines(BlurCubeRadius);
            Rr_CopyImageCube(
                Rr_GetGraph(),
                OriginalImageCube,
                BlurredImageCube,
                0);
            BlurCube.Blur(Rr_GetGraph(), BlurredImageCube, BlurCubePasses);
        }
        if (Rr_UISliderInt("Blur Passes", &BlurCubePasses, 0, 16))
        {
            Rr_CopyImageCube(
                Rr_GetGraph(),
                OriginalImageCube,
                BlurredImageCube,
                0);
            BlurCube.Blur(Rr_GetGraph(), BlurredImageCube, BlurCubePasses);
        }
        // Rr_UIInputInt
        // Rr_UICheckbox("Use Image3D", &UseImage3D);
        Rr_UIEndWindow();

        Rr_Graph *Graph = Rr_GetGraph();

        Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize();

        Camera.Update(Scancodes);

        SGPUUniform Uniform = {
            .View = Camera.ViewMatrix,
            .Projection = Camera.ProjMatrix,
        };
        std::memcpy(
            Rr_GetMappedBufferData(UniformBuffer),
            &Uniform,
            sizeof(SGPUUniform));

        Rr_Image2D *SwapchainImage = Rr_GetSwapchainImage();

        Rr_ColorClear ColorClear = {};
        ColorClear.Vec4 = { 13.0f / 255.0f,
                            14.0f / 255.0f,
                            28.0f / 255.0f,
                            1.0f };
        Rr_ColorTarget ColorTarget = {
            .Slot = 0,
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
            .Clear = ColorClear,
            .Image = SwapchainImage,
        };
        Rr_GraphNode *GraphicsNode =
            Rr_AddGraphicsNode(Graph, 1, &ColorTarget, NULL);
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
        Rr_BindCombinedImageCubeSampler(
            GraphicsNode,
            BlurredImageCube,
            Sampler,
            0,
            1);
        Rr_GLTFPrimitive *GLTFPrimitive = GLTFAsset->Meshes->Primitives;
        Rr_DrawIndexed(GraphicsNode, GLTFPrimitive->IndexCount, 1, 0, 0, 0);
    }

    SBlurApp()
        : BlurCube(RR_TEXTURE_FORMAT_R8G8B8A8_UNORM, 512, BlurCubeRadius)
    {
        InitPipeline();
        InitUniformBuffer();
        InitSampler();
        InitCubemapImage();
        InitSkyboxMesh();
    }

    ~SBlurApp()
    {
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
        Rr_ReleasePipelineLayout(PipelineLayout);
        Rr_ReleaseBuffer(UniformBuffer);
        Rr_ReleaseBuffer(StagingBuffer);
        Rr_ReleaseSampler(Sampler);
        Rr_ReleaseImage(OriginalImageCube);
        Rr_ReleaseImage(BlurredImageCube);
        Rr_ReleaseGLTFContext(GLTFContext);
    }
};

int main()
{
    static SBlurApp *App{};

    Rr_AppConfig Config = {};
    Config.Title = "Skybox";
    Config.WindowFlags |= RR_WINDOW_FLAGS_RESIZE_BIT;
    Config.InitFunc = []() { App = new SBlurApp(); };
    Config.EventFunc = [](Rr_Event *Event) { App->Event(Event); };
    Config.IterateFunc = []() { App->Iterate(); };
    Config.CleanupFunc = []() { delete App; };
    Rr_Run(&Config);
}
