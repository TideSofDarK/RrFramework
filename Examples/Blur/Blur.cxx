#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include <array>

#define STB_IMAGE_IMPLEMENTATION
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

struct SBoxBlur2D
{
    Rr_PipelineLayout *PipelineLayout;
    Rr_ComputePipeline *Blur2DXPipeline{};
    Rr_ComputePipeline *Blur2DYPipeline{};
    Rr_Image2D *IntermediateImageA;
    Rr_Image2D *IntermediateImageB;

    Rr_ImageFormat Format;
    std::uint32_t ImageSize;
    std::uint32_t WorkgroupSize;

    void Blur(
        Rr_Graph *Graph,
        Rr_Image2D *OriginalImage,
        Rr_Image2D *TargetImage,
        std::int32_t Passes)
    {
        Rr_BeginDebugLabel(Graph, "BoxBlur2D");

        Rr_CopyImage2D(
            Graph,
            OriginalImage,
            Rr_IntVec2{},
            IntermediateImageA,
            Rr_IntVec2{},
            Rr_IntVec2{ (std::int32_t)ImageSize, (std::int32_t)ImageSize },
            0);

        Rr_SetNextNodeName(Graph, "Blur2D");
        Rr_GraphNode *Node = Rr_AddComputeNode(Graph);
        Rr_BindComputePipeline(Node, Blur2DXPipeline);
        for (std::int32_t Index = 0; Index < Passes; ++Index)
        {
            Rr_BindStorageImage2D(Node, IntermediateImageA, 0, 0);
            Rr_BindStorageImage2DRW(Node, IntermediateImageB, 0, 1);
            Rr_Dispatch(Node, ImageSize / WorkgroupSize, 1, 1);
            Rr_ComputeBarrier(Node);
            Rr_BindComputePipeline(Node, Blur2DYPipeline);
            Rr_BindStorageImage2D(Node, IntermediateImageB, 0, 0);
            Rr_BindStorageImage2DRW(Node, IntermediateImageA, 0, 1);
            Rr_Dispatch(Node, ImageSize / WorkgroupSize, 1, 1);
            Rr_ComputeBarrier(Node);
        }

        Rr_CopyImage2D(
            Graph,
            IntermediateImageA,
            Rr_IntVec2{},
            TargetImage,
            Rr_IntVec2{},
            Rr_IntVec2{ (std::int32_t)ImageSize, (std::int32_t)ImageSize },
            0);

        Rr_EndDebugLabel(Graph, "BoxBlur2D");
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
            CreateBlurPipeline(EXAMPLE_ASSET_BOX2DX_COMP_SPV, KernelSize);
        Rr_ReleaseComputePipeline(Blur2DYPipeline);
        Blur2DYPipeline =
            CreateBlurPipeline(EXAMPLE_ASSET_BOX2DY_COMP_SPV, KernelSize);
    }

    SBoxBlur2D(
        Rr_ImageFormat Format,
        std::int32_t ImageSize,
        std::uint32_t KernelSize)
        : Format(Format)
        , ImageSize(ImageSize)
        , WorkgroupSize(std::sqrt(Rr_GetMaxComputeWorkgroupInvocations()))
    {
        std::array Bindings0 = {
            Rr_Binding{
                .Index = 0,
                .Type = RR_BINDING_TYPE_STORAGE_IMAGE,
                .Stages = RR_SHADER_STAGE_COMPUTE_BIT,
            },
            Rr_Binding{
                .Index = 1,
                .Type = RR_BINDING_TYPE_STORAGE_IMAGE,
                .Stages = RR_SHADER_STAGE_COMPUTE_BIT,
            },
        };
        std::array Sets = {
            Rr_BindingSet{ Bindings0.size(), Bindings0.data() },
        };
        PipelineLayout =
            Rr_CreatePipelineLayout((uint32_t)Sets.size(), Sets.data());

        RecreatePipelines(KernelSize);

        IntermediateImageA = Rr_CreateImage2D(
            { ImageSize, ImageSize },
            Format,
            RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_STORAGE_BIT);
        IntermediateImageB = Rr_CreateImage2D(
            { ImageSize, ImageSize },
            Format,
            RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_STORAGE_BIT);
    }

    ~SBoxBlur2D()
    {
        Rr_ReleasePipelineLayout(PipelineLayout);
        Rr_ReleaseComputePipeline(Blur2DXPipeline);
        Rr_ReleaseComputePipeline(Blur2DYPipeline);
        Rr_ReleaseImage(IntermediateImageA);
        Rr_ReleaseImage(IntermediateImageB);
    }
};

struct SDualKawaseBlur2D
{
    Rr_PipelineLayout *PipelineLayout;
    Rr_ComputePipeline *DownPipeline{};
    Rr_ComputePipeline *UpPipeline{};
    Rr_Image2D *IntermediateImageA;
    Rr_Image2D *IntermediateImageB;

    Rr_ImageFormat Format;
    std::uint32_t ImageSize;
    std::uint32_t WorkgroupSize;

    Rr_Sampler *Sampler;
    Rr_Buffer *UniformBuffer;

    void Blur(
        Rr_Graph *Graph,
        Rr_Image2D *OriginalImage,
        Rr_Image2D *TargetImage,
        std::int32_t Levels,
        float SamplerPosMultiplier)
    {
        if (Levels == 0)
        {
            Rr_CopyImage2D(
                Graph,
                OriginalImage,
                Rr_IntVec2{},
                TargetImage,
                Rr_IntVec2{},
                Rr_IntVec2{ (std::int32_t)ImageSize, (std::int32_t)ImageSize },
                0);

            return;
        }

        Rr_BeginDebugLabel(Graph, "DualKawase2D");

        Rr_CopyImage2D(
            Graph,
            OriginalImage,
            Rr_IntVec2{},
            IntermediateImageA,
            Rr_IntVec2{},
            Rr_IntVec2{ (std::int32_t)ImageSize, (std::int32_t)ImageSize },
            0);

        char *UniformData = (char *)Rr_GetMappedBufferData(UniformBuffer);
        std::size_t UniformOffset = 0;
        std::size_t UniformAlignment = Rr_GetUniformAlignment();

        Rr_BeginDebugLabel(Graph, "DualKawase2DDown");

        struct
        {
            std::uint32_t SrcWidth;
            std::uint32_t SrcHeight;
            Rr_Vec2 TexelSizeUV;
            float SamplerPosMultiplier;
        } GPUUniform;

        GPUUniform.SrcWidth = ImageSize;
        GPUUniform.SrcHeight = ImageSize;
        GPUUniform.TexelSizeUV = Rr_V2F(1.0f) / Rr_V2(ImageSize, ImageSize);
        GPUUniform.SamplerPosMultiplier = SamplerPosMultiplier;

        for (auto Level = 0; Level < Levels; ++Level)
        {
            Rr_GraphNode *Node = Rr_AddComputeNode(Graph);
            Rr_BindComputePipeline(Node, DownPipeline);

            std::memcpy(
                UniformData + UniformOffset,
                &GPUUniform,
                sizeof(GPUUniform));

            Rr_BindCombinedImage2DSampler(
                Node,
                IntermediateImageA,
                Sampler,
                0,
                0);
            Rr_BindStorageImage2DRW(Node, IntermediateImageB, 0, 1);
            Rr_BindUniformBuffer(
                Node,
                UniformBuffer,
                0,
                2,
                UniformOffset,
                sizeof(GPUUniform));
            Rr_Dispatch(
                Node,
                GPUUniform.SrcWidth / WorkgroupSize / 2,
                GPUUniform.SrcHeight / WorkgroupSize / 2,
                1);

            UniformOffset +=
                RR_ALIGN_POW2(sizeof(GPUUniform), UniformAlignment);

            GPUUniform.SrcWidth = GPUUniform.SrcWidth >> 1;
            GPUUniform.SrcHeight = GPUUniform.SrcHeight >> 1;

            std::swap(IntermediateImageA, IntermediateImageB);
        }

        Rr_EndDebugLabel(Graph, "DualKawase2DDown");

        Rr_BeginDebugLabel(Graph, "DualKawase2DUp");

        for (auto Level = 0; Level < Levels; ++Level)
        {
            Rr_GraphNode *Node = Rr_AddComputeNode(Graph);
            Rr_BindComputePipeline(Node, UpPipeline);

            std::memcpy(
                UniformData + UniformOffset,
                &GPUUniform,
                sizeof(GPUUniform));

            Rr_BindCombinedImage2DSampler(
                Node,
                IntermediateImageA,
                Sampler,
                0,
                0);
            Rr_BindStorageImage2DRW(Node, IntermediateImageB, 0, 1);
            Rr_BindUniformBuffer(
                Node,
                UniformBuffer,
                0,
                2,
                UniformOffset,
                sizeof(GPUUniform));
            Rr_Dispatch(
                Node,
                GPUUniform.SrcWidth * 2 / WorkgroupSize,
                GPUUniform.SrcHeight * 2 / WorkgroupSize,
                1);

            UniformOffset +=
                RR_ALIGN_POW2(sizeof(GPUUniform), UniformAlignment);

            GPUUniform.SrcWidth = GPUUniform.SrcWidth << 1;
            GPUUniform.SrcHeight = GPUUniform.SrcHeight << 1;

            std::swap(IntermediateImageA, IntermediateImageB);
        }

        Rr_EndDebugLabel(Graph, "DualKawase2DUp");

        Rr_CopyImage2D(
            Graph,
            IntermediateImageA,
            Rr_IntVec2{},
            TargetImage,
            Rr_IntVec2{},
            Rr_IntVec2{ (std::int32_t)ImageSize, (std::int32_t)ImageSize },
            0);

        Rr_EndDebugLabel(Graph, "DualKawase2D");
    }

    Rr_ComputePipeline *CreateBlurPipeline(Rr_AssetRef ComputeSPV)
    {
        std::array Specializations = {
            Rr_PipelineSpecialization{
                0,
                RR_MAKE_DATA_STRUCT(WorkgroupSize),
            },
        };

        Rr_ComputePipelineCreateInfo PipelineCreateInfo = {};
        PipelineCreateInfo.Layout = PipelineLayout;
        PipelineCreateInfo.ShaderSPV = Rr_LoadAsset(ComputeSPV);
        PipelineCreateInfo.SpecializationCount = Specializations.size();
        PipelineCreateInfo.Specializations = Specializations.data();

        return Rr_CreateComputePipeline(&PipelineCreateInfo);
    }

    SDualKawaseBlur2D(Rr_ImageFormat Format, std::int32_t ImageSize)
        : Format(Format)
        , ImageSize(ImageSize)
        , WorkgroupSize(std::sqrt(Rr_GetMaxComputeWorkgroupInvocations()))
    {
        std::array Bindings0 = {
            Rr_Binding{
                .Index = 0,
                .Type = RR_BINDING_TYPE_COMBINED_IMAGE_SAMPLER,
                .Stages = RR_SHADER_STAGE_COMPUTE_BIT,
            },
            Rr_Binding{
                .Index = 1,
                .Type = RR_BINDING_TYPE_STORAGE_IMAGE,
                .Stages = RR_SHADER_STAGE_COMPUTE_BIT,
            },
            Rr_Binding{
                .Index = 2,
                .Type = RR_BINDING_TYPE_UNIFORM_BUFFER,
                .Stages = RR_SHADER_STAGE_COMPUTE_BIT,
            },
        };
        std::array Sets = {
            Rr_BindingSet{ Bindings0.size(), Bindings0.data() },
        };
        PipelineLayout =
            Rr_CreatePipelineLayout((uint32_t)Sets.size(), Sets.data());

        DownPipeline =
            CreateBlurPipeline(EXAMPLE_ASSET_DUALKAWASE2DDOWN_COMP_SPV);
        UpPipeline = CreateBlurPipeline(EXAMPLE_ASSET_DUALKAWASE2DUP_COMP_SPV);

        IntermediateImageA = Rr_CreateImage2D(
            { ImageSize, ImageSize },
            Format,
            RR_IMAGE_FLAGS_SAMPLED_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT |
                RR_IMAGE_FLAGS_STORAGE_BIT);
        IntermediateImageB = Rr_CreateImage2D(
            { ImageSize, ImageSize },
            Format,
            RR_IMAGE_FLAGS_SAMPLED_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT |
                RR_IMAGE_FLAGS_STORAGE_BIT);

        Rr_SamplerInfo SamplerInfo = {};
        SamplerInfo.MinFilter = RR_FILTER_LINEAR;
        SamplerInfo.MagFilter = RR_FILTER_LINEAR;
        SamplerInfo.AddressModeU = RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        SamplerInfo.AddressModeV = RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        // SamplerInfo.AddressModeU = RR_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        // SamplerInfo.AddressModeV = RR_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        Sampler = Rr_CreateSampler(&SamplerInfo);

        UniformBuffer = Rr_CreateBuffer(
            RR_MEGABYTES(1),
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
                RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_STAGING_BIT);
    }

    ~SDualKawaseBlur2D()
    {
        Rr_ReleasePipelineLayout(PipelineLayout);
        Rr_ReleaseComputePipeline(DownPipeline);
        Rr_ReleaseComputePipeline(UpPipeline);
        Rr_ReleaseImage(IntermediateImageA);
        Rr_ReleaseImage(IntermediateImageB);
        Rr_ReleaseSampler(Sampler);
        Rr_ReleaseBuffer(UniformBuffer);
    }
};

struct SBoxBlurCube
{
    Rr_PipelineLayout *PipelineLayout;
    Rr_ComputePipeline *BlurCubeXPipeline{};
    Rr_ComputePipeline *BlurCubeYPipeline{};
    Rr_ImageCube *IntermediateImageA;
    Rr_ImageCube *IntermediateImageB;

    Rr_ImageFormat Format;
    std::uint32_t ImageSize;
    std::uint32_t WorkgroupSize;

    void Blur(
        Rr_Graph *Graph,
        Rr_ImageCube *OriginalImage,
        Rr_ImageCube *BlurredImage,
        std::int32_t Passes)
    {
        Rr_BeginDebugLabel(Graph, "BoxBlurCube");

        Rr_CopyImageCube(Rr_GetGraph(), OriginalImage, IntermediateImageA, 0);

        Rr_SetNextNodeName(Graph, "BlurCube");
        Rr_GraphNode *Node = Rr_AddComputeNode(Graph);
        for (std::int32_t Index = 0; Index < Passes; ++Index)
        {
            Rr_BindComputePipeline(Node, BlurCubeXPipeline);
            Rr_BindStorageImage2DArray(Node, IntermediateImageA, 0, 0);
            Rr_BindStorageImage2DArrayRW(Node, IntermediateImageB, 0, 1);
            Rr_Dispatch(Node, 1, ImageSize / WorkgroupSize, 6);
            Rr_ComputeBarrier(Node);
            Rr_BindComputePipeline(Node, BlurCubeYPipeline);
            Rr_BindStorageImage2DArray(Node, IntermediateImageB, 0, 0);
            Rr_BindStorageImage2DArrayRW(Node, IntermediateImageA, 0, 1);
            Rr_Dispatch(Node, ImageSize / WorkgroupSize, 1, 6);
            Rr_ComputeBarrier(Node);
        }

        Rr_CopyImageCube(Rr_GetGraph(), IntermediateImageA, BlurredImage, 0);

        Rr_EndDebugLabel(Graph, "BoxBlurCube");
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
            CreateBlurPipeline(EXAMPLE_ASSET_BOXCUBEX_COMP_SPV, Radius);
        Rr_ReleaseComputePipeline(BlurCubeYPipeline);
        BlurCubeYPipeline =
            CreateBlurPipeline(EXAMPLE_ASSET_BOXCUBEY_COMP_SPV, Radius);
    }

    SBoxBlurCube(
        Rr_ImageFormat Format,
        std::int32_t ImageSize,
        std::uint32_t Radius)
        : Format(Format)
        , ImageSize(ImageSize)
        , WorkgroupSize(std::sqrt(Rr_GetMaxComputeWorkgroupInvocations()))
    {
        std::array Bindings0 = {
            Rr_Binding{
                .Index = 0,
                .Type = RR_BINDING_TYPE_STORAGE_IMAGE,
                .Stages = RR_SHADER_STAGE_COMPUTE_BIT,
            },
            Rr_Binding{
                .Index = 1,
                .Type = RR_BINDING_TYPE_STORAGE_IMAGE,
                .Stages = RR_SHADER_STAGE_COMPUTE_BIT,
            },
        };
        std::array Sets = {
            Rr_BindingSet{ Bindings0.size(), Bindings0.data() },
        };
        PipelineLayout =
            Rr_CreatePipelineLayout((uint32_t)Sets.size(), Sets.data());

        RecreatePipelines(Radius);

        IntermediateImageA = Rr_CreateImageCube(
            { ImageSize, ImageSize },
            Format,
            RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_STORAGE_BIT);
        IntermediateImageB = Rr_CreateImageCube(
            { ImageSize, ImageSize },
            Format,
            RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_STORAGE_BIT);
    }

    ~SBoxBlurCube()
    {
        Rr_ReleasePipelineLayout(PipelineLayout);
        Rr_ReleaseComputePipeline(BlurCubeXPipeline);
        Rr_ReleaseComputePipeline(BlurCubeYPipeline);
        Rr_ReleaseImage(IntermediateImageA);
        Rr_ReleaseImage(IntermediateImageB);
    }
};

enum class EBlurType : std::uint32_t
{
    BOX_2D,
    DUAL_KAWASE_2D,
    BOX_CUBE,
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

    std::int32_t Blur2DKernelSize = 5;
    std::int32_t Blur2DPasses = 2;
    SBoxBlur2D BoxBlur2D;

    std::int32_t DualKawaseBlur2DLevels = 1;
    float DualKawaseBlur2DMultiplier = 1.0f;
    SDualKawaseBlur2D DualKawaseBlur2D;

    Rr_PipelineLayout *CubePipelineLayout;
    Rr_GraphicsPipeline *CubeGraphicsPipeline;
    Rr_ImageCube *OriginalImageCube;
    Rr_ImageCube *BlurredImageCube;
    std::int32_t BlurCubeRadius = 4;
    std::int32_t BlurCubePasses = 4;
    SBoxBlurCube BoxBlurCube;

    UScancodes Scancodes{};
    EBlurType Type = EBlurType::DUAL_KAWASE_2D;

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

    void InitQuadPipeline()
    {
        std::array Bindings = {
            Rr_Binding{
                .Index = 0,
                .Type = RR_BINDING_TYPE_COMBINED_IMAGE_SAMPLER,
                .Stages = RR_SHADER_STAGE_FRAGMENT_BIT,
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

        int32_t ImageSize = Width * Height * 4;

        OriginalImage2D = Rr_CreateImage2D(
            { Width, Height },
            RR_IMAGE_FORMAT_R8G8B8A8_SRGB,
            RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT);

        Rr_Buffer *StagingBuffer = Rr_CreateBuffer(
            Width * Height * 4,
            RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_STAGING_BIT);

        char *StagingData = (char *)Rr_GetMappedBufferData(StagingBuffer);
        std::memcpy(StagingData, PNGImage.Data, ImageSize);

        Rr_CopyBufferToImage2D(
            Rr_GetGraph(),
            StagingBuffer,
            0,
            { Width, Height },
            OriginalImage2D,
            0);

        BlurredImage2D = Rr_CreateImage2D(
            { Width, Height },
            RR_IMAGE_FORMAT_R8G8B8A8_SRGB,
            RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT);

        Rr_ReleaseBuffer(StagingBuffer);
    }

    void InitCubePipeline()
    {
        std::array Bindings = {
            Rr_Binding{
                .Index = 0,
                .Type = RR_BINDING_TYPE_UNIFORM_BUFFER,
                .Stages =
                    RR_SHADER_STAGE_VERTEX_BIT | RR_SHADER_STAGE_FRAGMENT_BIT,
            },
            Rr_Binding{
                .Index = 1,
                .Type = RR_BINDING_TYPE_COMBINED_IMAGE_SAMPLER,
                .Stages =
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
            RR_IMAGE_FORMAT_R8G8B8A8_SRGB,
            RR_IMAGE_FLAGS_TRANSFER_BIT);

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
            RR_IMAGE_FORMAT_R8G8B8A8_SRGB,
            RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT);
        BoxBlurCube.Blur(
            Rr_GetGraph(),
            OriginalImageCube,
            BlurredImageCube,
            BlurCubePasses);

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

    void Reblur(Rr_Graph *Graph, bool RecreatePipeline = false)
    {
        switch (Type)
        {
            case EBlurType::BOX_2D:
            {
                if (RecreatePipeline)
                {
                    BoxBlur2D.RecreatePipelines(Blur2DKernelSize);
                }
                BoxBlur2D
                    .Blur(Graph, OriginalImage2D, BlurredImage2D, Blur2DPasses);
            }
            break;
            case EBlurType::DUAL_KAWASE_2D:
            {
                DualKawaseBlur2D.Blur(
                    Graph,
                    OriginalImage2D,
                    BlurredImage2D,
                    DualKawaseBlur2DLevels,
                    DualKawaseBlur2DMultiplier);
            }
            case EBlurType::BOX_CUBE:
            {
                if (RecreatePipeline)
                {
                    BoxBlurCube.RecreatePipelines(BlurCubeRadius);
                }
                BoxBlurCube.Blur(
                    Rr_GetGraph(),
                    OriginalImageCube,
                    BlurredImageCube,
                    BlurCubePasses);
            }
            break;
        }
    }

    void Draw2D(Rr_Graph *Graph)
    {
        Rr_BeginDebugLabel(Graph, "DrawBlur2D");

        Rr_ColorTarget ColorTarget = {
            .Image = Rr_GetSwapchainImage(),
            .LoadOp = RR_LOAD_OP_DONT_CARE,
            .StoreOp = RR_STORE_OP_STORE,
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

        Rr_EndDebugLabel(Graph, "DrawBlur2D");
    }

    void DrawCube(Rr_Graph *Graph)
    {
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
            .Image = Rr_GetSwapchainImage(),
            .LoadOp = RR_LOAD_OP_DONT_CARE,
            .StoreOp = RR_STORE_OP_STORE,
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

    void Iterate()
    {
        Rr_Graph *Graph = Rr_GetGraph();

        Rr_UIBeginWindow("Blur.cxx", NULL, RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT);
        Rr_UILabel("This example demonstrates various blur algorithms.");
        Rr_UISeparator();
        const char *BlurTypes[3] = { "Box 2D", "Dual Kawase 2D", "Box Cube" };
        if (Rr_UICombobox("Mode", 3, BlurTypes, (std::uint32_t *)&Type))
        {
            Reblur(Graph);
        }
        switch (Type)
        {
            case EBlurType::BOX_2D:
            {
                bool KernelChanged =
                    Rr_UISliderInt("Kernel Size", &Blur2DKernelSize, 3, 9);
                bool PassesChanged =
                    Rr_UISliderInt("Passes", &Blur2DPasses, 0, 4);
                if (KernelChanged || PassesChanged)
                {
                    Reblur(Graph, KernelChanged);
                }

                Draw2D(Graph);
            }
            break;
            case EBlurType::DUAL_KAWASE_2D:
            {
                bool Changed = Rr_UISliderInt(
                    "Downsample Levels",
                    &DualKawaseBlur2DLevels,
                    0,
                    4);
                Changed |= Rr_UISliderFloat(
                    "Sampler Position Multiplier",
                    &DualKawaseBlur2DMultiplier,
                    0.1f,
                    25.0f);
                if (Changed)
                {
                    Reblur(Graph);
                }

                Draw2D(Graph);
            }
            break;
            case EBlurType::BOX_CUBE:
            {
                if (Rr_UISliderInt("Radius", &BlurCubeRadius, 2, 16))
                {
                    Reblur(Graph, true);
                }
                if (Rr_UISliderInt("Passes", &BlurCubePasses, 0, 16))
                {
                    Reblur(Graph);
                }

                DrawCube(Graph);
            }

            break;
        }

        Rr_UIEndWindow();
    }

    SBlurApp()
        : BoxBlur2D(RR_IMAGE_FORMAT_R8G8B8A8_UNORM, 512, Blur2DKernelSize)
        , DualKawaseBlur2D(RR_IMAGE_FORMAT_R8G8B8A8_UNORM, 512)
        , BoxBlurCube(RR_IMAGE_FORMAT_R8G8B8A8_UNORM, 512, BlurCubeRadius)
    {
        InitQuadPipeline();
        InitCubePipeline();
        InitUniformBuffer();
        InitSampler();
        InitImage2D();
        InitImageCube();
        InitSkyboxMesh();
        Reblur(Rr_GetGraph());
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
