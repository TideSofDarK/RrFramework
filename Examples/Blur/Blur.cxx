#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include <array>
#include <print>

#include "../../Vendor/stb/stb_image.h"

using UScancodes = std::array<bool, RR_SCANCODE_COUNT>;

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
        ProjMatrix =
            Rr_Perspective_RH(RR_ANGLE_DEG(FOVDegrees), Aspect, 0.1f, 100.0f);
        ProjMatrix.Elements[1][1] *= -1.0f;
    }

    [[nodiscard]] Rr_Mat4 GetViewMatrix() const
    {
        return Rr_InvGeneral(Transform);
    }

    void Update(const UScancodes &Scancodes)
    {
        float DeltaTime = Rr_GetDeltaSeconds();

        Rr_Vec2 MouseDelta = Rr_GetMousePositionDelta();

        if (Rr_GetMouseState() & RR_MOUSE_BUTTON_RIGHT_BIT)
        {
            Rr_SetRelativeMouseMode(true);
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

struct SBlur2D
{
    Rr_PipelineLayout *PipelineLayout;
    Rr_ComputePipeline *Blur2DXPipeline{};
    Rr_ComputePipeline *Blur2DYPipeline{};
    Rr_Image2D *IntermediateImage;

    Rr_TextureFormat Format;
    std::uint32_t ImageSize;
    std::uint32_t WorkgroupSize;

    void Blur(Rr_Graph *Graph, Rr_Image2D *TargetImage, std::int32_t Passes)
    {
        Rr_SetNextNodeName(Graph, "Blur2D");
        Rr_GraphNode *Node = Rr_AddComputeNode(Graph);
        for (std::int32_t Index = 0; Index < Passes; ++Index)
        {
            Rr_BindComputePipeline(Node, Blur2DXPipeline);
            Rr_BindStorageImage2D(Node, TargetImage, 0, 0);
            Rr_BindStorageImage2DRW(Node, IntermediateImage, 0, 1);
            Rr_Dispatch(Node, ImageSize / WorkgroupSize, 1, 1);
            Rr_ComputeBarrier(Node);
            Rr_BindComputePipeline(Node, Blur2DYPipeline);
            Rr_BindStorageImage2D(Node, IntermediateImage, 0, 0);
            Rr_BindStorageImage2DRW(Node, TargetImage, 0, 1);
            Rr_Dispatch(Node, ImageSize / WorkgroupSize, 1, 1);
            Rr_ComputeBarrier(Node);
        }
    }

    Rr_ComputePipeline *CreateBlurPipeline(
        Rr_AssetRef ComputeSPV,
        std::uint32_t KernelSize)
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
                RR_MAKE_DATA_STRUCT(KernelSize),
            },
        };

        Rr_ComputePipelineCreateInfo PipelineCreateInfo = {};
        PipelineCreateInfo.Layout = PipelineLayout;
        PipelineCreateInfo.ShaderSPV = Rr_LoadAsset(ComputeSPV);
        PipelineCreateInfo.SpecializationCount = Specializations.size();
        PipelineCreateInfo.Specializations = Specializations.data();

        return Rr_CreateComputePipeline(&PipelineCreateInfo);
    }

    void RecreatePipelines(std::uint32_t KernelSize)
    {
        Rr_ReleaseComputePipeline(Blur2DXPipeline);
        Blur2DXPipeline =
            CreateBlurPipeline(EXAMPLE_ASSET_BLUR2DX_COMP_SPV, KernelSize);
        Rr_ReleaseComputePipeline(Blur2DYPipeline);
        Blur2DYPipeline =
            CreateBlurPipeline(EXAMPLE_ASSET_BLUR2DY_COMP_SPV, KernelSize);
    }

    SBlur2D(
        Rr_TextureFormat Format,
        std::int32_t ImageSize,
        std::uint32_t KernelSize)
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

        RecreatePipelines(KernelSize);

        IntermediateImage = Rr_CreateImage2D(
            { ImageSize, ImageSize },
            Format,
            RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_STORAGE_BIT);
    }

    ~SBlur2D()
    {
        Rr_ReleasePipelineLayout(PipelineLayout);
        Rr_ReleaseComputePipeline(Blur2DXPipeline);
        Rr_ReleaseComputePipeline(Blur2DYPipeline);
        Rr_ReleaseImage(IntermediateImage);
    }
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
    Rr_Buffer *UniformBuffer;
    Rr_Sampler *Sampler;
    Rr_GLTFContext *GLTFContext;
    Rr_GLTFAsset *GLTFAsset;

    SCamera Camera;

    Rr_PipelineLayout *QuadPipelineLayout;
    Rr_GraphicsPipeline *QuadGraphicsPipeline;
    Rr_Image2D *OriginalImage2D;
    Rr_Image2D *BlurredImage2D;
    std::int32_t Blur2DKernelSize = 32;
    std::int32_t Blur2DPasses = 2;
    SBlur2D Blur2D;

    Rr_PipelineLayout *CubePipelineLayout;
    Rr_GraphicsPipeline *CubeGraphicsPipeline;
    Rr_ImageCube *OriginalImageCube;
    Rr_ImageCube *BlurredImageCube;
    std::int32_t BlurCubeRadius = 4;
    std::int32_t BlurCubePasses = 4;
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
        CubePipelineLayout =
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
        PipelineInfo.Layout = CubePipelineLayout;
        PipelineInfo.VertexShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_BLUR_VERT_SPV);
        PipelineInfo.FragmentShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_BLUR_FRAG_SPV);
        PipelineInfo.ColorTargetCount = 1;
        PipelineInfo.ColorTargets = &ColorTarget;
        PipelineInfo.Rasterizer.CullMode = RR_CULL_MODE_NONE;
        PipelineInfo.VertexInputBindingCount = VertexInputBindings.size();
        PipelineInfo.VertexInputBindings = VertexInputBindings.data();

        CubeGraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

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

    void InitImageCube()
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

        Rr_Buffer *StagingBuffer = Rr_CreateBuffer(
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

        Rr_ReleaseBuffer(StagingBuffer);
    }

    void InitQuadPipeline()
    {
        std::array Bindings = {
            Rr_Binding{
                0,
                RR_BINDING_TYPE_COMBINED_IMAGE_SAMPLER,
                RR_SHADER_STAGE_FRAGMENT_BIT,
            },
        };
        std::array Sets = {
            Rr_BindingSet{ Bindings.size(), Bindings.data() },
        };
        QuadPipelineLayout =
            Rr_CreatePipelineLayout((uint32_t)Sets.size(), Sets.data());

        Rr_ColorTargetInfo ColorTarget = {};
        ColorTarget.Format = Rr_GetSwapchainFormat();

        Rr_GraphicsPipelineCreateInfo PipelineInfo = {};
        PipelineInfo.Layout = QuadPipelineLayout;
        PipelineInfo.VertexShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_QUAD_VERT_SPV);
        PipelineInfo.FragmentShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_QUAD_FRAG_SPV);
        PipelineInfo.ColorTargetCount = 1;
        PipelineInfo.ColorTargets = &ColorTarget;

        QuadGraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);
    }

    void InitImage2D()
    {
        SPNGImage PNGImage{ EXAMPLE_ASSET_BACK_PNG };

        int32_t Width = PNGImage.Width;
        int32_t Height = PNGImage.Height;

        int32_t LayerSize = Width * Height * 4;

        OriginalImage2D = Rr_CreateImage2D(
            { Width, Height },
            RR_TEXTURE_FORMAT_R8G8B8A8_UNORM,
            RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT);

        Rr_Buffer *StagingBuffer = Rr_CreateBuffer(
            Width * Height * 4,
            RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_STAGING_BIT);

        char *StagingData = (char *)Rr_GetMappedBufferData(StagingBuffer);
        std::memcpy(StagingData, PNGImage.Data, LayerSize);

        Rr_CopyBufferToImage2D(
            Rr_GetGraph(),
            StagingBuffer,
            0,
            { Width, Height },
            OriginalImage2D,
            0);

        BlurredImage2D = Rr_CreateImage2D(
            { Width, Height },
            RR_TEXTURE_FORMAT_R8G8B8A8_UNORM,
            RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT |
                RR_IMAGE_FLAGS_STORAGE_BIT);

        Rr_CopyImage2D(
            Rr_GetGraph(),
            OriginalImage2D,
            Rr_IntVec2{},
            BlurredImage2D,
            Rr_IntVec2{},
            Rr_IntVec2{ 512, 512 },
            0);
        Blur2D.Blur(Rr_GetGraph(), BlurredImage2D, Blur2DPasses);

        Rr_ReleaseBuffer(StagingBuffer);
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
        Rr_Graph *Graph = Rr_GetGraph();

        Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize();
        Rr_Image2D *SwapchainImage = Rr_GetSwapchainImage();

        Rr_UIBeginWindow("Blur.cxx", NULL, RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT);
        Rr_UILabel(
            "This example demonstrates blur compute shaders.\nBlurring "
            "cubemaps requires different algorithm to avoid seams between cube "
            "faces.");
        Rr_UISeparator();
        const char *Modes[2] = { "Image2D", "ImageCube" };
        static std::uint32_t SelectedModeIndex = 0;
        Rr_UICombobox("Mode", 2, Modes, &SelectedModeIndex);
        if (SelectedModeIndex == 0)
        {
            if (Rr_UISliderInt("Kernel Size", &Blur2DKernelSize, 8, 64))
            {
                Blur2D.RecreatePipelines(Blur2DKernelSize);
                Rr_CopyImage2D(
                    Rr_GetGraph(),
                    OriginalImage2D,
                    Rr_IntVec2{},
                    BlurredImage2D,
                    Rr_IntVec2{},
                    Rr_IntVec2{ 512, 512 },
                    0);
                Blur2D.Blur(Rr_GetGraph(), BlurredImage2D, Blur2DPasses);
            }
            if (Rr_UISliderInt("Passes", &Blur2DPasses, 0, 4))
            {
                Rr_CopyImage2D(
                    Rr_GetGraph(),
                    OriginalImage2D,
                    Rr_IntVec2{},
                    BlurredImage2D,
                    Rr_IntVec2{},
                    Rr_IntVec2{ 512, 512 },
                    0);
                Blur2D.Blur(Rr_GetGraph(), BlurredImage2D, Blur2DPasses);
            }

            Rr_ColorTarget ColorTarget = {
                .Slot = 0,
                .LoadOp = RR_LOAD_OP_DONT_CARE,
                .StoreOp = RR_STORE_OP_STORE,
                .Image = SwapchainImage,
            };
            Rr_GraphNode *GraphicsNode =
                Rr_AddGraphicsNode(Graph, 1, &ColorTarget, NULL);
            Rr_BindGraphicsPipeline(GraphicsNode, QuadGraphicsPipeline);
            Rr_BindCombinedImage2DSampler(
                GraphicsNode,
                BlurredImage2D,
                Sampler,
                0,
                0);
            Rr_Draw(GraphicsNode, 6, 1, 0, 0);
        }
        else
        {
            if (Rr_UISliderInt("Radius", &BlurCubeRadius, 2, 16))
            {
                BlurCube.RecreatePipelines(BlurCubeRadius);
                Rr_CopyImageCube(
                    Rr_GetGraph(),
                    OriginalImageCube,
                    BlurredImageCube,
                    0);
                BlurCube.Blur(Rr_GetGraph(), BlurredImageCube, BlurCubePasses);
            }
            if (Rr_UISliderInt("Passes", &BlurCubePasses, 0, 16))
            {
                Rr_CopyImageCube(
                    Rr_GetGraph(),
                    OriginalImageCube,
                    BlurredImageCube,
                    0);
                BlurCube.Blur(Rr_GetGraph(), BlurredImageCube, BlurCubePasses);
            }

            Camera.Update(Scancodes);

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
                .Image = SwapchainImage,
            };
            Rr_GraphNode *GraphicsNode =
                Rr_AddGraphicsNode(Graph, 1, &ColorTarget, NULL);
            Rr_BindGraphicsPipeline(GraphicsNode, CubeGraphicsPipeline);
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
        Rr_UIEndWindow();
    }

    SBlurApp()
        : Blur2D(RR_TEXTURE_FORMAT_R8G8B8A8_UNORM, 512, Blur2DKernelSize)
        , BlurCube(RR_TEXTURE_FORMAT_R8G8B8A8_UNORM, 512, BlurCubeRadius)
    {
        InitQuadPipeline();
        InitPipeline();
        InitUniformBuffer();
        InitSampler();
        InitImage2D();
        InitImageCube();
        InitSkyboxMesh();
    }

    ~SBlurApp()
    {
        Rr_ReleaseBuffer(UniformBuffer);
        Rr_ReleaseSampler(Sampler);
        Rr_ReleaseGraphicsPipeline(QuadGraphicsPipeline);
        Rr_ReleasePipelineLayout(QuadPipelineLayout);
        Rr_ReleaseImage(OriginalImage2D);
        Rr_ReleaseImage(BlurredImage2D);
        Rr_ReleaseGraphicsPipeline(CubeGraphicsPipeline);
        Rr_ReleasePipelineLayout(CubePipelineLayout);
        Rr_ReleaseImage(OriginalImageCube);
        Rr_ReleaseImage(BlurredImageCube);
        Rr_ReleaseGLTFContext(GLTFContext);
    }
};

int main()
{
    static SBlurApp *App{};

    Rr_AppConfig Config = {};
    Config.Title = "Blur";
    Config.WindowFlags |= RR_WINDOW_FLAGS_RESIZE_BIT;
    Config.InitFunc = []() { App = new SBlurApp(); };
    Config.EventFunc = [](Rr_Event *Event) { App->Event(Event); };
    Config.IterateFunc = []() { App->Iterate(); };
    Config.CleanupFunc = []() { delete App; };
    Rr_Run(&Config);
}
