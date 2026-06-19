#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include <array>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include "../../Vendor/stb/stb_image.h"

static constexpr std::int32_t MAX_IMAGE_SIZE = 4096;

class CCamera
{
    float FieldOfView{ RR_ANGLE_DEG(90.0f) };
    float Pitch{};
    float Yaw{};

    Rr_Mat4 Transform = Rr_M4D(1.0f);
    Rr_Mat4 ProjMatrix = Rr_M4D(1.0f);

public:
    Rr_Mat4 GetProjectionMatrix() const
    {
        return ProjMatrix;
    }

    Rr_Mat4 GetViewMatrix() const
    {
        return Rr_InvGeneral(Transform);
    }

    void Update(float Aspect)
    {
        auto DeltaTime = Rr_GetDeltaSeconds();
        auto MouseDelta = Rr_GetMousePositionDelta();

        if (Rr_GetMouseState() & RR_MOUSE_BUTTON_RIGHT_BIT)
        {
            Rr_SetRelativeMouseMode(true);

            auto constexpr SENSITIVITY = 0.005f;
            Yaw -= MouseDelta.X * SENSITIVITY;
            Pitch -= MouseDelta.Y * SENSITIVITY;
        }
        else
        {
            Rr_SetRelativeMouseMode(false);
        }

        Yaw = Rr_WrapMax(Yaw, RR_PI32 * 2.0f);
        Pitch = RR_CLAMP(RR_PI32 * -0.5f, Pitch, RR_PI32 * 0.5f);

        Transform = Rr_Rotate_RH(Yaw, Rr_V3(0.0f, 1.0f, 0.0f)) * Rr_Rotate_RH(Pitch, Rr_V3(1.0f, 0.0f, 0.0f));
        ProjMatrix = Rr_Perspective_RH(FieldOfView, Aspect, 0.1f, 100.0f);
        ProjMatrix.Elements[1][1] *= -1.0f;
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
        auto Asset = Rr_LoadAsset(AssetRef);
        auto DesiredChannels = 4;
        Data = stbi_load_from_memory(
            (stbi_uc *)Asset.Data,
            (int32_t)Asset.Size,
            (int32_t *)&Width,
            (int32_t *)&Height,
            &Channels,
            DesiredChannels);
    }

    SPNGImage(const char *Path)
    {
        auto DesiredChannels = 4;
        Data = stbi_load(Path, (int32_t *)&Width, (int32_t *)&Height, &Channels, DesiredChannels);
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
    Rr_ComputePipeline *Blur2DXPipeline{};
    Rr_ComputePipeline *Blur2DYPipeline{};
    Rr_Buffer *UniformBuffer{};

    std::uint32_t LocalSizeX{};

    void Blur(
        Rr_Graph *Graph,
        Rr_Image2D *OriginalImage,
        Rr_Image2D *IntermediateImageA,
        Rr_Image2D *IntermediateImageB,
        Rr_Image2D *TargetImage,
        std::int32_t Passes)
    {
        Rr_BeginGraphLabel(Graph, "BoxBlur2D");

        auto ImageSize = Rr_GetImage2DExtent(OriginalImage);

        Rr_CopyImage2D(Graph, OriginalImage, Rr_IntVec2{}, IntermediateImageA, Rr_IntVec2{}, ImageSize, 0);

        std::memcpy(Rr_GetMappedBufferData(UniformBuffer), &ImageSize, sizeof(ImageSize));

        auto Node = Rr_AddComputeNode(Graph);
        Rr_BindComputePipeline(Node, Blur2DXPipeline);
        Rr_BindUniformBuffer(Node, UniformBuffer, 0, 2, 0, sizeof(Rr_IntVec2));
        for (std::int32_t Index = 0; Index < Passes; ++Index)
        {
            Rr_BindComputePipeline(Node, Blur2DXPipeline);
            Rr_BindStorageImage2D(Node, IntermediateImageA, 0, 0);
            Rr_BindStorageImage2DRW(Node, IntermediateImageB, 0, 1);
            Rr_Dispatch(Node, ImageSize.Height / LocalSizeX + 1, 1, 1);
            Rr_ComputeBarrier(Node);
            Rr_BindComputePipeline(Node, Blur2DYPipeline);
            Rr_BindStorageImage2D(Node, IntermediateImageB, 0, 0);
            Rr_BindStorageImage2DRW(Node, IntermediateImageA, 0, 1);
            Rr_Dispatch(Node, ImageSize.Width / LocalSizeX + 1, 1, 1);
            Rr_ComputeBarrier(Node);
        }

        Rr_CopyImage2D(Graph, IntermediateImageA, Rr_IntVec2{}, TargetImage, Rr_IntVec2{}, ImageSize, 0);

        Rr_EndGraphLabel(Graph, "BoxBlur2D");
    }

    Rr_ComputePipeline *CreateBlurPipeline(Rr_AssetRef ComputeSPV, std::uint32_t KernelSize)
    {
        auto Specializations = std::array{
            Rr_PipelineSpecialization{
                .ConstantID = 0,
                .Size = sizeof(LocalSizeX),
                .Data = &LocalSizeX,
            },
            Rr_PipelineSpecialization{
                .ConstantID = 1,
                .Size = sizeof(KernelSize),
                .Data = &KernelSize,
            },
        };

        auto ComputeShader = Rr_LoadAsset(ComputeSPV);
        auto ShaderInfo = Rr_ShaderInfo{
            .SPVSize = ComputeShader.Size,
            .SPVData = ComputeShader.Data,
            .SpecializationCount = Specializations.size(),
            .Specializations = Specializations.data(),
        };

        return Rr_CreateComputePipeline(&ShaderInfo);
    }

    void RecreatePipelines(std::uint32_t KernelSize)
    {
        Rr_ReleaseComputePipeline(Blur2DXPipeline);
        Blur2DXPipeline = CreateBlurPipeline(EXAMPLE_ASSET_BOX2DX_COMP_SPV, KernelSize);
        Rr_ReleaseComputePipeline(Blur2DYPipeline);
        Blur2DYPipeline = CreateBlurPipeline(EXAMPLE_ASSET_BOX2DY_COMP_SPV, KernelSize);
    }

    SBoxBlur2D(std::uint32_t KernelSize)
        : LocalSizeX(Rr_GetMaxComputeWorkgroupInvocations())
    {
        RecreatePipelines(KernelSize);

        UniformBuffer = Rr_CreateBuffer(RR_KIBIBYTES(1), RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_DYNAMIC);
    }

    ~SBoxBlur2D()
    {
        Rr_ReleaseComputePipeline(Blur2DXPipeline);
        Rr_ReleaseComputePipeline(Blur2DYPipeline);
        Rr_ReleaseBuffer(UniformBuffer);
    }
};

struct SKawaseBlur2D
{
    struct SGPUUniform
    {
        Rr_IntVec2 SrcSize;
        Rr_Vec2 TexelSizeUV;
        float SamplerPosMultiplier;
    };

    Rr_ComputePipeline *Pipeline{};

    std::uint32_t LocalSize{};

    Rr_Sampler *Sampler{};
    Rr_Buffer *UniformBuffer{};

    void Blur(
        Rr_Graph *Graph,
        Rr_Image2D *OriginalImage,
        Rr_Image2D *IntermediateImageA,
        Rr_Image2D *IntermediateImageB,
        Rr_Image2D *TargetImage,
        std::int32_t Passes,
        float SamplerPosMultiplier)
    {
        auto OriginalSize = Rr_GetImage2DExtent(OriginalImage);

        if (Passes == 0)
        {
            Rr_CopyImage2D(Graph, OriginalImage, Rr_IntVec2{}, TargetImage, Rr_IntVec2{}, OriginalSize, 0);

            return;
        }

        Rr_BeginGraphLabel(Graph, "Kawase2D");

        Rr_CopyImage2D(Graph, OriginalImage, Rr_IntVec2{}, IntermediateImageA, Rr_IntVec2{}, OriginalSize, 0);

        auto UniformData = (std::byte *)Rr_GetMappedBufferData(UniformBuffer);
        auto UniformOffset = 0zu;
        auto UniformAlignment = Rr_GetUniformAlignment();

        SGPUUniform GPUUniform = {
            .SrcSize = OriginalSize,
            .TexelSizeUV = Rr_V2F(1.0f) / Rr_V2(MAX_IMAGE_SIZE, MAX_IMAGE_SIZE),
        };

        for (auto Pass = 0; Pass < Passes; ++Pass)
        {
            auto Node = Rr_AddComputeNode(Graph);
            Rr_BindComputePipeline(Node, Pipeline);

            GPUUniform.SamplerPosMultiplier = float(Pass + 1) * SamplerPosMultiplier;

            std::memcpy(UniformData + UniformOffset, &GPUUniform, sizeof(GPUUniform));

            Rr_BindCombinedImage2DSampler(Node, IntermediateImageA, Sampler, 0, 0);
            Rr_BindStorageImage2DRW(Node, IntermediateImageB, 0, 1);
            Rr_BindUniformBuffer(Node, UniformBuffer, 0, 2, UniformOffset, sizeof(GPUUniform));
            Rr_Dispatch(Node, OriginalSize.Width / LocalSize + 1, OriginalSize.Height / LocalSize + 1, 1);

            UniformOffset += RR_ALIGN_POW2(sizeof(GPUUniform), UniformAlignment);

            std::swap(IntermediateImageA, IntermediateImageB);
        }

        Rr_CopyImage2D(Graph, IntermediateImageA, Rr_IntVec2{}, TargetImage, Rr_IntVec2{}, OriginalSize, 0);

        Rr_EndGraphLabel(Graph, "Kawase2D");
    }

    SKawaseBlur2D()
        : LocalSize(std::sqrt(Rr_GetMaxComputeWorkgroupInvocations()))
    {
        auto Specializations = std::array{
            Rr_PipelineSpecialization{
                .ConstantID = 0,
                .Size = sizeof(LocalSize),
                .Data = &LocalSize,
            },
        };

        auto ComputeShader = Rr_LoadAsset(EXAMPLE_ASSET_KAWASE2D_COMP_SPV);
        auto ShaderInfo = Rr_ShaderInfo{
            .SPVSize = ComputeShader.Size,
            .SPVData = ComputeShader.Data,
            .SpecializationCount = Specializations.size(),
            .Specializations = Specializations.data(),
        };

        Pipeline = Rr_CreateComputePipeline(&ShaderInfo);

        auto SamplerInfo = Rr_SamplerInfo{
            .MagFilter = RR_FILTER_LINEAR,
            .MinFilter = RR_FILTER_LINEAR,
            .AddressModeU = RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .AddressModeV = RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        };
        Sampler = Rr_CreateSampler(&SamplerInfo);

        UniformBuffer = Rr_CreateBuffer(RR_KIBIBYTES(1), RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_DYNAMIC);
    }

    ~SKawaseBlur2D()
    {
        Rr_ReleaseComputePipeline(Pipeline);
        Rr_ReleaseSampler(Sampler);
        Rr_ReleaseBuffer(UniformBuffer);
    }
};

struct SDualKawaseBlur2D
{
    struct SGPUUniform
    {
        Rr_IntVec2 SrcSize;
        Rr_Vec2 TexelSizeUV;
        float SamplerPosMultiplier;
    };

    Rr_ComputePipeline *DownPipeline{};
    Rr_ComputePipeline *UpPipeline{};

    std::uint32_t LocalSize;

    Rr_Sampler *Sampler{};
    Rr_Buffer *UniformBuffer{};

    void Blur(
        Rr_Graph *Graph,
        Rr_Image2D *OriginalImage,
        Rr_Image2D *IntermediateImageA,
        Rr_Image2D *IntermediateImageB,
        Rr_Image2D *TargetImage,
        std::int32_t Levels,
        float SamplerPosMultiplier)
    {
        auto OriginalSize = Rr_GetImage2DExtent(OriginalImage);

        if (Levels == 0)
        {
            Rr_CopyImage2D(Graph, OriginalImage, Rr_IntVec2{}, TargetImage, Rr_IntVec2{}, OriginalSize, 0);

            return;
        }

        Rr_BeginGraphLabel(Graph, "DualKawase2D");

        Rr_CopyImage2D(Graph, OriginalImage, Rr_IntVec2{}, IntermediateImageA, Rr_IntVec2{}, OriginalSize, 0);

        auto UniformData = (std::byte *)Rr_GetMappedBufferData(UniformBuffer);
        auto UniformOffset = 0zu;
        auto UniformAlignment = Rr_GetUniformAlignment();

        Rr_BeginGraphLabel(Graph, "DualKawase2DDown");

        SGPUUniform GPUUniform = {
            .SrcSize = OriginalSize,
            .TexelSizeUV = Rr_V2F(1.0f) / Rr_V2(MAX_IMAGE_SIZE, MAX_IMAGE_SIZE),
            .SamplerPosMultiplier = SamplerPosMultiplier,
        };

        for (auto Level = 0; Level < Levels; ++Level)
        {
            auto Node = Rr_AddComputeNode(Graph);
            Rr_BindComputePipeline(Node, DownPipeline);

            std::memcpy(UniformData + UniformOffset, &GPUUniform, sizeof(GPUUniform));

            Rr_BindCombinedImage2DSampler(Node, IntermediateImageA, Sampler, 0, 0);
            Rr_BindStorageImage2DRW(Node, IntermediateImageB, 0, 1);
            Rr_BindUniformBuffer(Node, UniformBuffer, 0, 2, UniformOffset, sizeof(GPUUniform));
            Rr_Dispatch(
                Node,
                GPUUniform.SrcSize.Width / LocalSize / 2 + 1,
                GPUUniform.SrcSize.Height / LocalSize / 2 + 1,
                1);

            UniformOffset += RR_ALIGN_POW2(sizeof(GPUUniform), UniformAlignment);

            GPUUniform.SrcSize.Width >>= 1;
            GPUUniform.SrcSize.Height >>= 1;

            std::swap(IntermediateImageA, IntermediateImageB);
        }

        Rr_EndGraphLabel(Graph, "DualKawase2DDown");

        Rr_BeginGraphLabel(Graph, "DualKawase2DUp");

        for (auto Level = 0; Level < Levels; ++Level)
        {
            auto Node = Rr_AddComputeNode(Graph);
            Rr_BindComputePipeline(Node, UpPipeline);

            std::memcpy(UniformData + UniformOffset, &GPUUniform, sizeof(GPUUniform));

            Rr_BindCombinedImage2DSampler(Node, IntermediateImageA, Sampler, 0, 0);
            Rr_BindStorageImage2DRW(Node, IntermediateImageB, 0, 1);
            Rr_BindUniformBuffer(Node, UniformBuffer, 0, 2, UniformOffset, sizeof(GPUUniform));
            Rr_Dispatch(
                Node,
                GPUUniform.SrcSize.Width * 2 / LocalSize + 1,
                GPUUniform.SrcSize.Height * 2 / LocalSize + 1,
                1);

            UniformOffset += RR_ALIGN_POW2(sizeof(GPUUniform), UniformAlignment);

            GPUUniform.SrcSize.Width = RR_MIN(GPUUniform.SrcSize.Width << 1, OriginalSize.Width);
            GPUUniform.SrcSize.Height = RR_MIN(GPUUniform.SrcSize.Height << 1, OriginalSize.Width);

            std::swap(IntermediateImageA, IntermediateImageB);
        }

        Rr_EndGraphLabel(Graph, "DualKawase2DUp");

        Rr_CopyImage2D(Graph, IntermediateImageA, Rr_IntVec2{}, TargetImage, Rr_IntVec2{}, OriginalSize, 0);

        Rr_EndGraphLabel(Graph, "DualKawase2D");
    }

    Rr_ComputePipeline *CreateBlurPipeline(Rr_AssetRef ComputeSPV)
    {
        auto Specializations = std::array{
            Rr_PipelineSpecialization{
                .ConstantID = 0,
                .Size = sizeof(LocalSize),
                .Data = &LocalSize,
            },
        };

        auto ComputeShader = Rr_LoadAsset(ComputeSPV);
        auto ShaderInfo = Rr_ShaderInfo{
            .SPVSize = ComputeShader.Size,
            .SPVData = ComputeShader.Data,
            .SpecializationCount = Specializations.size(),
            .Specializations = Specializations.data(),
        };

        return Rr_CreateComputePipeline(&ShaderInfo);
    }

    SDualKawaseBlur2D()
        : LocalSize(std::sqrt(Rr_GetMaxComputeWorkgroupInvocations()))
    {
        DownPipeline = CreateBlurPipeline(EXAMPLE_ASSET_DUALKAWASE2DDOWN_COMP_SPV);
        UpPipeline = CreateBlurPipeline(EXAMPLE_ASSET_DUALKAWASE2DUP_COMP_SPV);

        auto SamplerInfo = Rr_SamplerInfo{
            .MagFilter = RR_FILTER_LINEAR,
            .MinFilter = RR_FILTER_LINEAR,
            .AddressModeU = RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .AddressModeV = RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,

        };
        Sampler = Rr_CreateSampler(&SamplerInfo);

        UniformBuffer = Rr_CreateBuffer(RR_MEBIBYTES(1), RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_DYNAMIC);
    }

    ~SDualKawaseBlur2D()
    {
        Rr_ReleaseComputePipeline(DownPipeline);
        Rr_ReleaseComputePipeline(UpPipeline);
        Rr_ReleaseSampler(Sampler);
        Rr_ReleaseBuffer(UniformBuffer);
    }
};

struct SBoxBlurCube
{
    Rr_ComputePipeline *BlurCubeXPipeline{};
    Rr_ComputePipeline *BlurCubeYPipeline{};
    Rr_ImageCube *IntermediateImageA{};
    Rr_ImageCube *IntermediateImageB{};

    Rr_ImageFormat Format;
    std::uint32_t ImageSize;
    std::uint32_t LocalSize;

    void Blur(Rr_Graph *Graph, Rr_ImageCube *OriginalImage, Rr_ImageCube *BlurredImage, std::int32_t Passes)
    {
        Rr_BeginGraphLabel(Graph, "BoxBlurCube");

        Rr_CopyImageCube(Rr_GetGraph(), OriginalImage, IntermediateImageA, 0);

        auto Node = Rr_AddComputeNode(Graph);
        for (std::int32_t Index = 0; Index < Passes; ++Index)
        {
            Rr_BindComputePipeline(Node, BlurCubeXPipeline);
            Rr_BindStorageImage2DArray(Node, IntermediateImageA, 0, 0);
            Rr_BindStorageImage2DArrayRW(Node, IntermediateImageB, 0, 1);
            Rr_Dispatch(Node, 1, ImageSize / LocalSize, 6);
            Rr_ComputeBarrier(Node);
            Rr_BindComputePipeline(Node, BlurCubeYPipeline);
            Rr_BindStorageImage2DArray(Node, IntermediateImageB, 0, 0);
            Rr_BindStorageImage2DArrayRW(Node, IntermediateImageA, 0, 1);
            Rr_Dispatch(Node, ImageSize / LocalSize, 1, 6);
            Rr_ComputeBarrier(Node);
        }

        Rr_CopyImageCube(Rr_GetGraph(), IntermediateImageA, BlurredImage, 0);

        Rr_EndGraphLabel(Graph, "BoxBlurCube");
    }

    Rr_ComputePipeline *CreateBlurPipeline(Rr_AssetRef ComputeSPV, std::uint32_t Radius)
    {
        auto Specializations = std::array{
            Rr_PipelineSpecialization{
                .ConstantID = 0,
                .Size = sizeof(LocalSize),
                .Data = &LocalSize,
            },
            Rr_PipelineSpecialization{
                .ConstantID = 1,
                .Size = sizeof(ImageSize),
                .Data = &ImageSize,
            },
            Rr_PipelineSpecialization{
                .ConstantID = 2,
                .Size = sizeof(Radius),
                .Data = &Radius,
            },
        };

        auto ComputeShader = Rr_LoadAsset(ComputeSPV);
        auto ShaderInfo = Rr_ShaderInfo{
            .SPVSize = ComputeShader.Size,
            .SPVData = ComputeShader.Data,
            .SpecializationCount = Specializations.size(),
            .Specializations = Specializations.data(),
        };

        return Rr_CreateComputePipeline(&ShaderInfo);
    }

    void RecreatePipelines(std::uint32_t Radius)
    {
        Rr_ReleaseComputePipeline(BlurCubeXPipeline);
        BlurCubeXPipeline = CreateBlurPipeline(EXAMPLE_ASSET_BOXCUBEX_COMP_SPV, Radius);
        Rr_ReleaseComputePipeline(BlurCubeYPipeline);
        BlurCubeYPipeline = CreateBlurPipeline(EXAMPLE_ASSET_BOXCUBEY_COMP_SPV, Radius);
    }

    SBoxBlurCube(Rr_ImageFormat Format, std::int32_t ImageSize, std::uint32_t Radius)
        : Format(Format)
        , ImageSize(ImageSize)
        , LocalSize(std::sqrt(Rr_GetMaxComputeWorkgroupInvocations()))
    {
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
        Rr_ReleaseComputePipeline(BlurCubeXPipeline);
        Rr_ReleaseComputePipeline(BlurCubeYPipeline);
        Rr_ReleaseImage(IntermediateImageA);
        Rr_ReleaseImage(IntermediateImageB);
    }
};

enum class EBlurType : std::uint32_t
{
    BOX_2D,
    KAWASE_2D,
    DUAL_KAWASE_2D,
    BOX_CUBE,
};

struct SBlurApp
{
    Rr_Buffer *UniformBuffer{};
    Rr_Sampler *Sampler{};

    CCamera Camera;

    Rr_Image2D *IntermediateImageA{};
    Rr_Image2D *IntermediateImageB{};

    Rr_GraphicsPipeline *QuadGraphicsPipeline{};
    Rr_Image2D *OriginalImage2D{};
    Rr_Image2D *BlurredImage2D{};

    std::int32_t Blur2DKernelSize{ 5 };
    std::int32_t Blur2DPasses{ 2 };
    SBoxBlur2D BoxBlur2D;

    std::int32_t KawaseBlur2DPasses{ 5 };
    float KawaseBlur2DMultiplier{ 1.0f };
    SKawaseBlur2D KawaseBlur2D;

    std::int32_t DualKawaseBlur2DLevels{ 1 };
    float DualKawaseBlur2DMultiplier{ 1.5f };
    SDualKawaseBlur2D DualKawaseBlur2D;

    Rr_GraphicsPipeline *CubeGraphicsPipeline{};
    Rr_ImageCube *OriginalImageCube{};
    Rr_ImageCube *BlurredImageCube{};
    std::int32_t BlurCubeRadius{ 4 };
    std::int32_t BlurCubePasses{ 4 };
    SBoxBlurCube BoxBlurCube;

    EBlurType Type = EBlurType::DUAL_KAWASE_2D;

    void InitUniformBuffer()
    {
        UniformBuffer = Rr_CreateBuffer(sizeof(SGPUUniform), RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_DYNAMIC);
    }

    void InitSampler()
    {
        auto Info = Rr_SamplerInfo{
            .MagFilter = RR_FILTER_LINEAR,
            .MinFilter = RR_FILTER_LINEAR,
        };
        Sampler = Rr_CreateSampler(&Info);
    }

    void InitQuadPipeline()
    {
        auto ColorTarget = Rr_ColorTargetInfo{
            .Format = Rr_GetImageFormat(Rr_GetSwapchainImage()),
        };

        auto VertexShader = Rr_LoadAsset(EXAMPLE_ASSET_QUAD_VERT_SPV);
        auto VertexShaderInfo = Rr_ShaderInfo{
            .SPVSize = VertexShader.Size,
            .SPVData = VertexShader.Data,
        };

        auto FragmentShader = Rr_LoadAsset(EXAMPLE_ASSET_QUAD_FRAG_SPV);
        auto FragmentShaderInfo = Rr_ShaderInfo{
            .SPVSize = FragmentShader.Size,
            .SPVData = FragmentShader.Data,
        };

        auto PipelineInfo = Rr_GraphicsPipelineCreateInfo{
            .VertexShaderInfo = &VertexShaderInfo,
            .FragmentShaderInfo = &FragmentShaderInfo,
            .ColorTargetCount = 1,
            .ColorTargets = &ColorTarget,
        };

        QuadGraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);
    }

    void InitImage2D(const SPNGImage &PNGImage)
    {
        auto Width = PNGImage.Width;
        auto Height = PNGImage.Height;

        auto ImageSize = Width * Height * 4;

        Rr_ReleaseImage(OriginalImage2D);
        OriginalImage2D = Rr_CreateImage2D(
            { Width, Height },
            RR_IMAGE_FORMAT_R8G8B8A8_SRGB,
            RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT);

        auto StagingBuffer = Rr_CreateBuffer(Width * Height * 4, RR_BUFFER_FLAGS_STAGING);

        auto StagingData = (std::byte *)Rr_GetMappedBufferData(StagingBuffer);
        std::memcpy(StagingData, PNGImage.Data, ImageSize);

        Rr_CopyBufferToImage2D(Rr_GetGraph(), StagingBuffer, 0, { Width, Height }, OriginalImage2D, 0);

        Rr_ReleaseImage(BlurredImage2D);
        BlurredImage2D = Rr_CreateImage2D(
            { Width, Height },
            RR_IMAGE_FORMAT_R8G8B8A8_SRGB,
            RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT);

        Rr_ReleaseBuffer(StagingBuffer);
    }

    void InitCubePipeline()
    {
        auto VertexAttributes = std::array{
            Rr_VertexInputAttribute{ .Location = 0, .Format = RR_FORMAT_FLOAT3 },
        };

        auto VertexInputBindings = std::array{
            Rr_VertexInputBinding{
                .Rate = RR_VERTEX_INPUT_RATE_VERTEX,
                .AttributeCount = VertexAttributes.size(),
                .Attributes = VertexAttributes.data(),
            },
        };

        auto ColorTarget = Rr_ColorTargetInfo{
            .Blend = Rr_AlphaBlend(),
            .Format = Rr_GetImageFormat(Rr_GetSwapchainImage()),
        };

        auto VertexShader = Rr_LoadAsset(EXAMPLE_ASSET_CUBE_VERT_SPV);
        auto VertexShaderInfo = Rr_ShaderInfo{
            .SPVSize = VertexShader.Size,
            .SPVData = VertexShader.Data,
        };

        auto FragmentShader = Rr_LoadAsset(EXAMPLE_ASSET_CUBE_FRAG_SPV);
        auto FragmentShaderInfo = Rr_ShaderInfo{
            .SPVSize = FragmentShader.Size,
            .SPVData = FragmentShader.Data,
        };

        auto PipelineInfo = Rr_GraphicsPipelineCreateInfo{
            .VertexShaderInfo = &VertexShaderInfo,
            .FragmentShaderInfo = &FragmentShaderInfo,
            .VertexInputBindingCount = VertexInputBindings.size(),
            .VertexInputBindings = VertexInputBindings.data(),
            .ColorTargetCount = 1,
            .ColorTargets = &ColorTarget,
        };

        CubeGraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);
    }

    void InitImageCube()
    {
        auto Right = SPNGImage{ EXAMPLE_ASSET_RIGHT_PNG };
        auto Left = SPNGImage{ EXAMPLE_ASSET_LEFT_PNG };
        auto Up = SPNGImage{ EXAMPLE_ASSET_UP_PNG };
        auto Down = SPNGImage{ EXAMPLE_ASSET_DOWN_PNG };
        auto Front = SPNGImage{ EXAMPLE_ASSET_FRONT_PNG };
        auto Back = SPNGImage{ EXAMPLE_ASSET_BACK_PNG };

        auto Width = Up.Width;
        auto Height = Up.Height;

        auto LayerSize = Width * Height * 4;

        OriginalImageCube =
            Rr_CreateImageCube({ Width, Height }, RR_IMAGE_FORMAT_R8G8B8A8_SRGB, RR_IMAGE_FLAGS_TRANSFER_BIT);

        auto StagingBuffer = Rr_CreateBuffer(Width * Height * 4 * 6, RR_BUFFER_FLAGS_STAGING);

        auto StagingData = (std::byte *)Rr_GetMappedBufferData(StagingBuffer);
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

        Rr_ReleaseBuffer(StagingBuffer);
    }

    void Event(Rr_Event const *Event)
    {
        switch (Event->Type)
        {
            case RR_EVENT_TYPE_DROP_FILE:
            {
                InitImage2D(SPNGImage{ Event->DropFile.Path });
            }
            break;
            default:
                return;
        }
    }

    void Reblur(Rr_Graph *Graph)
    {
        switch (Type)
        {
            case EBlurType::BOX_2D:
            {
                BoxBlur2D
                    .Blur(Graph, OriginalImage2D, IntermediateImageA, IntermediateImageB, BlurredImage2D, Blur2DPasses);
            }
            break;
            case EBlurType::KAWASE_2D:
            {
                KawaseBlur2D.Blur(
                    Graph,
                    OriginalImage2D,
                    IntermediateImageA,
                    IntermediateImageB,
                    BlurredImage2D,
                    KawaseBlur2DPasses,
                    KawaseBlur2DMultiplier);
            }
            break;
            case EBlurType::DUAL_KAWASE_2D:
            {
                DualKawaseBlur2D.Blur(
                    Graph,
                    OriginalImage2D,
                    IntermediateImageA,
                    IntermediateImageB,
                    BlurredImage2D,
                    DualKawaseBlur2DLevels,
                    DualKawaseBlur2DMultiplier);
            }
            break;
            case EBlurType::BOX_CUBE:
            {
                BoxBlurCube.Blur(Rr_GetGraph(), OriginalImageCube, BlurredImageCube, BlurCubePasses);
            }
            break;
        }
    }

    void Draw2D(Rr_Graph *Graph)
    {
        Rr_BeginGraphLabel(Graph, "DrawBlur2D");

        auto ColorTarget = Rr_ColorTarget{
            .Image = Rr_GetSwapchainImage(),
            .LoadOp = RR_LOAD_OP_DONT_CARE,
            .StoreOp = RR_STORE_OP_STORE,
        };
        auto GraphicsNode = Rr_AddGraphicsNode(Graph, 1, &ColorTarget, nullptr);
        Rr_BindGraphicsPipeline(GraphicsNode, QuadGraphicsPipeline);
        Rr_BindCombinedImage2DSampler(GraphicsNode, BlurredImage2D, Sampler, 0, 0);
        Rr_Draw(GraphicsNode, 6, 1, 0, 0);

        Rr_EndGraphLabel(Graph, "DrawBlur2D");
    }

    void DrawCube(Rr_Graph *Graph)
    {
        Rr_BeginGraphLabel(Graph, "DrawBlurCube");

        auto SwapchainImage = Rr_GetSwapchainImage();

        Camera.Update(Rr_GetImage2DAspect(SwapchainImage));

        auto Uniform = SGPUUniform{
            .View = Camera.GetViewMatrix(),
            .Projection = Camera.GetProjectionMatrix(),
        };
        std::memcpy(Rr_GetMappedBufferData(UniformBuffer), &Uniform, sizeof(SGPUUniform));

        auto ColorTarget = Rr_ColorTarget{
            .Image = Rr_GetSwapchainImage(),
            .LoadOp = RR_LOAD_OP_DONT_CARE,
            .StoreOp = RR_STORE_OP_STORE,
        };
        auto GraphicsNode = Rr_AddGraphicsNode(Graph, 1, &ColorTarget, nullptr);
        Rr_BindGraphicsPipeline(GraphicsNode, CubeGraphicsPipeline);
        Rr_BindUniformBuffer(GraphicsNode, UniformBuffer, 0, 0, 0, sizeof(SGPUUniform));
        Rr_BindCombinedImageCubeSampler(GraphicsNode, BlurredImageCube, Sampler, 0, 1);
        Rr_Draw(GraphicsNode, 3, 1, 0, 0);

        Rr_EndGraphLabel(Graph, "DrawBlurCube");
    }

    void Iterate()
    {
        Rr_UIBeginDebugOverlayTabs();
        Rr_UIBeginWindowEx("Blur.cxx", nullptr, RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT);
        Rr_UIText(
            "This example shows various blur algorithms.\nYou can drop "
            "a PNG image into the window to blur it (works only with 2D "
            "algorithms).");
        Rr_UISeparator();
        auto BlurTypes = std::array{ "Box 2D", "Kawase 2D", "Dual Kawase 2D", "Box Cube" };
        Rr_UICombobox("Mode", BlurTypes.size(), BlurTypes.data(), (std::uint32_t *)&Type);
        auto Graph = Rr_GetGraph();
        Reblur(Graph);
        switch (Type)
        {
            case EBlurType::BOX_2D:
            {
                if (Rr_UISliderInt("Kernel Size", &Blur2DKernelSize, 3, 9))
                {
                    BoxBlur2D.RecreatePipelines(Blur2DKernelSize);
                }
                Rr_UISliderInt("Passes", &Blur2DPasses, 0, 4);

                Draw2D(Graph);
            }
            break;
            case EBlurType::KAWASE_2D:
            {
                Rr_UISliderInt("Passes", &KawaseBlur2DPasses, 0, 9);
                Rr_UISliderFloat("Sample Position Multiplier", &KawaseBlur2DMultiplier, 0.1f, 25.0f);

                Draw2D(Graph);
            }
            break;
            case EBlurType::DUAL_KAWASE_2D:
            {
                Rr_UISliderInt("Downsample Levels", &DualKawaseBlur2DLevels, 0, 4);
                Rr_UISliderFloat("Sample Position Multiplier", &DualKawaseBlur2DMultiplier, 0.1f, 25.0f);

                Draw2D(Graph);
            }
            break;
            case EBlurType::BOX_CUBE:
            {
                if (Rr_UISliderInt("Radius", &BlurCubeRadius, 2, 16))
                {
                    BoxBlurCube.RecreatePipelines(BlurCubeRadius);
                }
                Rr_UISliderInt("Passes", &BlurCubePasses, 0, 16);

                DrawCube(Graph);
            }
            break;
        }
        Rr_UIEndWindow();
        Rr_UIEndDebugOverlayTabs();
    }

    void InitIntermediateImages()
    {
        IntermediateImageA = Rr_CreateImage2D(
            { MAX_IMAGE_SIZE, MAX_IMAGE_SIZE },
            RR_IMAGE_FORMAT_R8G8B8A8_UNORM,
            RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_STORAGE_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT);
        IntermediateImageB = Rr_CreateImage2D(
            { MAX_IMAGE_SIZE, MAX_IMAGE_SIZE },
            RR_IMAGE_FORMAT_R8G8B8A8_UNORM,
            RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_STORAGE_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT);
    }

    SBlurApp()
        : BoxBlur2D(Blur2DKernelSize)
        , DualKawaseBlur2D()
        , BoxBlurCube(RR_IMAGE_FORMAT_R8G8B8A8_UNORM, 512, BlurCubeRadius)
    {
        InitIntermediateImages();
        InitQuadPipeline();
        InitCubePipeline();
        InitUniformBuffer();
        InitSampler();
        InitImage2D(EXAMPLE_ASSET_BACK_PNG);
        InitImageCube();
        Reblur(Rr_GetGraph());
    }

    ~SBlurApp()
    {
        Rr_ReleaseBuffer(UniformBuffer);
        Rr_ReleaseSampler(Sampler);
        Rr_ReleaseGraphicsPipeline(QuadGraphicsPipeline);
        Rr_ReleaseImage(OriginalImage2D);
        Rr_ReleaseImage(BlurredImage2D);
        Rr_ReleaseGraphicsPipeline(CubeGraphicsPipeline);
        Rr_ReleaseImage(OriginalImageCube);
        Rr_ReleaseImage(BlurredImageCube);
        Rr_ReleaseImage(IntermediateImageA);
        Rr_ReleaseImage(IntermediateImageB);
    }
};

int main()
{
    static SBlurApp *App{};

    auto Config = Rr_Config{
        .WindowTitle = "Blur",
        .WindowFlags = RR_WINDOW_FLAGS_RESIZE_BIT,
        .InitFunc = []() { App = new SBlurApp(); },
        .EventFunc = [](Rr_Event const *Event) { App->Event(Event); },
        .IterateFunc = []() { App->Iterate(); },
        .CleanupFunc = []() { delete App; },
    };
    Rr_Run(&Config);
}
