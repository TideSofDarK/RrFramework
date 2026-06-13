#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include <array>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include "../../Vendor/stb/stb_image.h"

static constexpr std::int32_t MAX_IMAGE_SIZE = 4096;

struct SCube
{
    static float constexpr CubePositions[] = {
        1.00,  1.00,  -1.00, 1.00,  1.00,  -1.00, 1.00,  1.00,  -1.00,
        1.00,  -1.00, -1.00, 1.00,  -1.00, -1.00, 1.00,  -1.00, -1.00,
        1.00,  1.00,  1.00,  1.00,  1.00,  1.00,  1.00,  1.00,  1.00,
        1.00,  -1.00, 1.00,  1.00,  -1.00, 1.00,  1.00,  -1.00, 1.00,
        -1.00, 1.00,  -1.00, -1.00, 1.00,  -1.00, -1.00, 1.00,  -1.00,
        -1.00, -1.00, -1.00, -1.00, -1.00, -1.00, -1.00, -1.00, -1.00,
        -1.00, 1.00,  1.00,  -1.00, 1.00,  1.00,  -1.00, 1.00,  1.00,
        -1.00, -1.00, 1.00,  -1.00, -1.00, 1.00,  -1.00, -1.00, 1.00,
    };
    static uint16_t constexpr CubeIndices[] = {
        1,  13, 19, 1,  19, 7,  9, 6, 18, 9, 18, 21, 23, 20, 14, 23, 14, 17,
        16, 4,  10, 16, 10, 22, 5, 2, 8,  5, 8,  11, 15, 12, 0,  15, 0,  3,
    };

    Rr_Buffer *Buffer{};
    size_t IndexOffset{};
    size_t IndexCount{};

    void Init()
    {
        size_t TotalSize = sizeof(CubePositions) + sizeof(CubeIndices);
        Rr_Buffer *StagingBuffer = Rr_CreateBuffer(
            TotalSize,
            RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT);
        Rr_ReleaseBuffer(StagingBuffer);
        char *BufferData = (char *)Rr_GetMappedBufferData(StagingBuffer);
        std::memcpy(BufferData, CubePositions, sizeof(CubePositions));
        BufferData += sizeof(CubePositions);
        std::memcpy(BufferData, CubeIndices, sizeof(CubeIndices));

        Buffer = Rr_CreateBuffer(
            TotalSize,
            RR_BUFFER_FLAGS_VERTEX_BIT | RR_BUFFER_FLAGS_INDEX_BIT);
        auto TransferNode = Rr_AddTransferNode(Rr_GetGraph());
        Rr_TransferBufferData(
            TransferNode,
            TotalSize,
            StagingBuffer,
            0,
            Buffer,
            0);
        IndexOffset = sizeof(CubePositions);
        IndexCount = sizeof(CubeIndices) / sizeof(*CubeIndices);
    }

    void Cleanup()
    {
        Rr_ReleaseBuffer(Buffer);
    }
};

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

    void Update()
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
            (stbi_uc *)Asset.Data,
            (int32_t)Asset.Size,
            (int32_t *)&Width,
            (int32_t *)&Height,
            &Channels,
            DesiredChannels);
    }

    SPNGImage(const char *Path)
    {
        int32_t DesiredChannels = 4;
        Data = stbi_load(
            Path,
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
    Rr_ComputePipeline *Blur2DXPipeline{};
    Rr_ComputePipeline *Blur2DYPipeline{};
    Rr_Buffer *UniformBuffer;

    std::uint32_t LocalSizeX;

    void Blur(
        Rr_Graph *Graph,
        Rr_Image2D *OriginalImage,
        Rr_Image2D *IntermediateImageA,
        Rr_Image2D *IntermediateImageB,
        Rr_Image2D *TargetImage,
        std::int32_t Passes)
    {
        Rr_BeginGraphLabel(Graph, "BoxBlur2D");

        Rr_IntVec2 ImageSize = Rr_GetImage2DExtent(OriginalImage);

        Rr_CopyImage2D(
            Graph,
            OriginalImage,
            Rr_IntVec2{},
            IntermediateImageA,
            Rr_IntVec2{},
            ImageSize,
            0);

        std::memcpy(
            Rr_GetMappedBufferData(UniformBuffer),
            &ImageSize,
            sizeof(ImageSize));

        Rr_GraphNode *Node = Rr_AddComputeNode(Graph);
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

        Rr_CopyImage2D(
            Graph,
            IntermediateImageA,
            Rr_IntVec2{},
            TargetImage,
            Rr_IntVec2{},
            ImageSize,
            0);

        Rr_EndGraphLabel(Graph, "BoxBlur2D");
    }

    Rr_ComputePipeline *CreateBlurPipeline(
        Rr_AssetRef ComputeSPV,
        std::uint32_t KernelSize)
    {
        std::array Specializations = {
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

        Rr_Asset ComputeShader = Rr_LoadAsset(ComputeSPV);
        Rr_ShaderInfo ShaderInfo = {
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
        Blur2DXPipeline =
            CreateBlurPipeline(EXAMPLE_ASSET_BOX2DX_COMP_SPV, KernelSize);
        Rr_ReleaseComputePipeline(Blur2DYPipeline);
        Blur2DYPipeline =
            CreateBlurPipeline(EXAMPLE_ASSET_BOX2DY_COMP_SPV, KernelSize);
    }

    SBoxBlur2D(std::uint32_t KernelSize)
        : LocalSizeX(Rr_GetMaxComputeWorkgroupInvocations())
    {
        RecreatePipelines(KernelSize);

        UniformBuffer = Rr_CreateBuffer(
            RR_KIBIBYTES(1),
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
                RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_STAGING_BIT);
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
        Rr_IntVec2 OriginalSize = Rr_GetImage2DExtent(OriginalImage);

        if (Passes == 0)
        {
            Rr_CopyImage2D(
                Graph,
                OriginalImage,
                Rr_IntVec2{},
                TargetImage,
                Rr_IntVec2{},
                OriginalSize,
                0);

            return;
        }

        Rr_BeginGraphLabel(Graph, "Kawase2D");

        Rr_CopyImage2D(
            Graph,
            OriginalImage,
            Rr_IntVec2{},
            IntermediateImageA,
            Rr_IntVec2{},
            OriginalSize,
            0);

        char *UniformData = (char *)Rr_GetMappedBufferData(UniformBuffer);
        std::size_t UniformOffset = 0;
        std::size_t UniformAlignment = Rr_GetUniformAlignment();

        SGPUUniform GPUUniform = {
            .SrcSize = OriginalSize,
            .TexelSizeUV = Rr_V2F(1.0f) / Rr_V2(MAX_IMAGE_SIZE, MAX_IMAGE_SIZE),
        };

        for (auto Pass = 0; Pass < Passes; ++Pass)
        {
            Rr_GraphNode *Node = Rr_AddComputeNode(Graph);
            Rr_BindComputePipeline(Node, Pipeline);

            GPUUniform.SamplerPosMultiplier =
                float(Pass + 1) * SamplerPosMultiplier;

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
                OriginalSize.Width / LocalSize + 1,
                OriginalSize.Height / LocalSize + 1,
                1);

            UniformOffset +=
                RR_ALIGN_POW2(sizeof(GPUUniform), UniformAlignment);

            std::swap(IntermediateImageA, IntermediateImageB);
        }

        Rr_CopyImage2D(
            Graph,
            IntermediateImageA,
            Rr_IntVec2{},
            TargetImage,
            Rr_IntVec2{},
            OriginalSize,
            0);

        Rr_EndGraphLabel(Graph, "Kawase2D");
    }

    SKawaseBlur2D()
        : LocalSize(std::sqrt(Rr_GetMaxComputeWorkgroupInvocations()))
    {
        std::array Specializations = {
            Rr_PipelineSpecialization{
                .ConstantID = 0,
                .Size = sizeof(LocalSize),
                .Data = &LocalSize,
            },
        };

        Rr_Asset ComputeShader = Rr_LoadAsset(EXAMPLE_ASSET_KAWASE2D_COMP_SPV);
        Rr_ShaderInfo ShaderInfo = {
            .SPVSize = ComputeShader.Size,
            .SPVData = ComputeShader.Data,
            .SpecializationCount = Specializations.size(),
            .Specializations = Specializations.data(),
        };

        Pipeline = Rr_CreateComputePipeline(&ShaderInfo);

        Rr_SamplerInfo SamplerInfo = {};
        SamplerInfo.MinFilter = RR_FILTER_LINEAR;
        SamplerInfo.MagFilter = RR_FILTER_LINEAR;
        SamplerInfo.AddressModeU = RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        SamplerInfo.AddressModeV = RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        Sampler = Rr_CreateSampler(&SamplerInfo);

        UniformBuffer = Rr_CreateBuffer(
            RR_KIBIBYTES(1),
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
                RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_STAGING_BIT);
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

    Rr_Sampler *Sampler;
    Rr_Buffer *UniformBuffer;

    void Blur(
        Rr_Graph *Graph,
        Rr_Image2D *OriginalImage,
        Rr_Image2D *IntermediateImageA,
        Rr_Image2D *IntermediateImageB,
        Rr_Image2D *TargetImage,
        std::int32_t Levels,
        float SamplerPosMultiplier)
    {
        Rr_IntVec2 OriginalSize = Rr_GetImage2DExtent(OriginalImage);

        if (Levels == 0)
        {
            Rr_CopyImage2D(
                Graph,
                OriginalImage,
                Rr_IntVec2{},
                TargetImage,
                Rr_IntVec2{},
                OriginalSize,
                0);

            return;
        }

        Rr_BeginGraphLabel(Graph, "DualKawase2D");

        Rr_CopyImage2D(
            Graph,
            OriginalImage,
            Rr_IntVec2{},
            IntermediateImageA,
            Rr_IntVec2{},
            OriginalSize,
            0);

        char *UniformData = (char *)Rr_GetMappedBufferData(UniformBuffer);
        std::size_t UniformOffset = 0;
        std::size_t UniformAlignment = Rr_GetUniformAlignment();

        Rr_BeginGraphLabel(Graph, "DualKawase2DDown");

        SGPUUniform GPUUniform = {
            .SrcSize = OriginalSize,
            .TexelSizeUV = Rr_V2F(1.0f) / Rr_V2(MAX_IMAGE_SIZE, MAX_IMAGE_SIZE),
            .SamplerPosMultiplier = SamplerPosMultiplier,
        };

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
                GPUUniform.SrcSize.Width / LocalSize / 2 + 1,
                GPUUniform.SrcSize.Height / LocalSize / 2 + 1,
                1);

            UniformOffset +=
                RR_ALIGN_POW2(sizeof(GPUUniform), UniformAlignment);

            GPUUniform.SrcSize.Width >>= 1;
            GPUUniform.SrcSize.Height >>= 1;

            std::swap(IntermediateImageA, IntermediateImageB);
        }

        Rr_EndGraphLabel(Graph, "DualKawase2DDown");

        Rr_BeginGraphLabel(Graph, "DualKawase2DUp");

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
                GPUUniform.SrcSize.Width * 2 / LocalSize + 1,
                GPUUniform.SrcSize.Height * 2 / LocalSize + 1,
                1);

            UniformOffset +=
                RR_ALIGN_POW2(sizeof(GPUUniform), UniformAlignment);

            GPUUniform.SrcSize.Width =
                RR_MIN(GPUUniform.SrcSize.Width << 1, OriginalSize.Width);
            GPUUniform.SrcSize.Height =
                RR_MIN(GPUUniform.SrcSize.Height << 1, OriginalSize.Width);

            std::swap(IntermediateImageA, IntermediateImageB);
        }

        Rr_EndGraphLabel(Graph, "DualKawase2DUp");

        Rr_CopyImage2D(
            Graph,
            IntermediateImageA,
            Rr_IntVec2{},
            TargetImage,
            Rr_IntVec2{},
            OriginalSize,
            0);

        Rr_EndGraphLabel(Graph, "DualKawase2D");
    }

    Rr_ComputePipeline *CreateBlurPipeline(Rr_AssetRef ComputeSPV)
    {
        std::array Specializations = {
            Rr_PipelineSpecialization{
                .ConstantID = 0,
                .Size = sizeof(LocalSize),
                .Data = &LocalSize,
            },
        };

        Rr_Asset ComputeShader = Rr_LoadAsset(ComputeSPV);
        Rr_ShaderInfo ShaderInfo = {
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
        DownPipeline =
            CreateBlurPipeline(EXAMPLE_ASSET_DUALKAWASE2DDOWN_COMP_SPV);
        UpPipeline = CreateBlurPipeline(EXAMPLE_ASSET_DUALKAWASE2DUP_COMP_SPV);

        Rr_SamplerInfo SamplerInfo = {};
        SamplerInfo.MinFilter = RR_FILTER_LINEAR;
        SamplerInfo.MagFilter = RR_FILTER_LINEAR;
        SamplerInfo.AddressModeU = RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        SamplerInfo.AddressModeV = RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        Sampler = Rr_CreateSampler(&SamplerInfo);

        UniformBuffer = Rr_CreateBuffer(
            RR_MIBIBYTES(1),
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
                RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_STAGING_BIT);
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
    Rr_ImageCube *IntermediateImageA;
    Rr_ImageCube *IntermediateImageB;

    Rr_ImageFormat Format;
    std::uint32_t ImageSize;
    std::uint32_t LocalSize;

    void Blur(
        Rr_Graph *Graph,
        Rr_ImageCube *OriginalImage,
        Rr_ImageCube *BlurredImage,
        std::int32_t Passes)
    {
        Rr_BeginGraphLabel(Graph, "BoxBlurCube");

        Rr_CopyImageCube(Rr_GetGraph(), OriginalImage, IntermediateImageA, 0);

        Rr_GraphNode *Node = Rr_AddComputeNode(Graph);
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

    Rr_ComputePipeline *CreateBlurPipeline(
        Rr_AssetRef ComputeSPV,
        std::uint32_t Radius)
    {
        std::array Specializations = {
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

        Rr_Asset ComputeShader = Rr_LoadAsset(ComputeSPV);
        Rr_ShaderInfo ShaderInfo = {
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
    Rr_Buffer *UniformBuffer;
    Rr_Sampler *Sampler;

    SCamera Camera;
    SCube Cube;

    Rr_Image2D *IntermediateImageA;
    Rr_Image2D *IntermediateImageB;

    Rr_GraphicsPipeline *QuadGraphicsPipeline;
    Rr_Image2D *OriginalImage2D{};
    Rr_Image2D *BlurredImage2D{};

    std::int32_t Blur2DKernelSize = 5;
    std::int32_t Blur2DPasses = 2;
    SBoxBlur2D BoxBlur2D;

    std::int32_t KawaseBlur2DPasses = 5;
    float KawaseBlur2DMultiplier = 1.0f;
    SKawaseBlur2D KawaseBlur2D;

    std::int32_t DualKawaseBlur2DLevels = 1;
    float DualKawaseBlur2DMultiplier = 1.5f;
    SDualKawaseBlur2D DualKawaseBlur2D;

    Rr_GraphicsPipeline *CubeGraphicsPipeline;
    Rr_ImageCube *OriginalImageCube;
    Rr_ImageCube *BlurredImageCube;
    std::int32_t BlurCubeRadius = 4;
    std::int32_t BlurCubePasses = 4;
    SBoxBlurCube BoxBlurCube;

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
        Rr_ColorTargetInfo ColorTarget = {};
        ColorTarget.Format = Rr_GetImageFormat(Rr_GetSwapchainImage());

        Rr_Asset VertexShader = Rr_LoadAsset(EXAMPLE_ASSET_QUAD_VERT_SPV);
        Rr_ShaderInfo VertexShaderInfo = {
            .SPVSize = VertexShader.Size,
            .SPVData = VertexShader.Data,
        };

        Rr_Asset FragmentShader = Rr_LoadAsset(EXAMPLE_ASSET_QUAD_FRAG_SPV);
        Rr_ShaderInfo FragmentShaderInfo = {
            .SPVSize = FragmentShader.Size,
            .SPVData = FragmentShader.Data,
        };

        Rr_GraphicsPipelineCreateInfo PipelineInfo = {};
        PipelineInfo.VertexShaderInfo = &VertexShaderInfo;
        PipelineInfo.FragmentShaderInfo = &FragmentShaderInfo;
        PipelineInfo.ColorTargetCount = 1;
        PipelineInfo.ColorTargets = &ColorTarget;

        QuadGraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);
    }

    void InitImage2D(const SPNGImage &PNGImage)
    {
        int32_t Width = PNGImage.Width;
        int32_t Height = PNGImage.Height;

        int32_t ImageSize = Width * Height * 4;

        Rr_ReleaseImage(OriginalImage2D);
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

        Rr_ReleaseImage(BlurredImage2D);
        BlurredImage2D = Rr_CreateImage2D(
            { Width, Height },
            RR_IMAGE_FORMAT_R8G8B8A8_SRGB,
            RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT);

        Rr_ReleaseBuffer(StagingBuffer);
    }

    void InitCubePipeline()
    {
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
        ColorTarget.Format = Rr_GetImageFormat(Rr_GetSwapchainImage());
        ColorTarget.Blend = Rr_AlphaBlend();

        Rr_Asset VertexShader = Rr_LoadAsset(EXAMPLE_ASSET_CUBE_VERT_SPV);
        Rr_ShaderInfo VertexShaderInfo = {
            .SPVSize = VertexShader.Size,
            .SPVData = VertexShader.Data,
        };

        Rr_Asset FragmentShader = Rr_LoadAsset(EXAMPLE_ASSET_CUBE_FRAG_SPV);
        Rr_ShaderInfo FragmentShaderInfo = {
            .SPVSize = FragmentShader.Size,
            .SPVData = FragmentShader.Data,
        };

        Rr_GraphicsPipelineCreateInfo PipelineInfo = {};
        PipelineInfo.VertexShaderInfo = &VertexShaderInfo;
        PipelineInfo.FragmentShaderInfo = &FragmentShaderInfo;
        PipelineInfo.ColorTargetCount = 1;
        PipelineInfo.ColorTargets = &ColorTarget;
        PipelineInfo.Rasterizer.CullMode = RR_CULL_MODE_NONE;
        PipelineInfo.VertexInputBindingCount = VertexInputBindings.size();
        PipelineInfo.VertexInputBindings = VertexInputBindings.data();

        CubeGraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);
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

        Rr_ReleaseBuffer(StagingBuffer);
    }

    void InitCamera()
    {
        Rr_IntVec2 SwapchainSize = Rr_GetImage2DExtent(Rr_GetSwapchainImage());
        float Aspect = (float)SwapchainSize.Width / SwapchainSize.Height;
        Camera.UpdatePerspective(Aspect);
    }

    void Event(Rr_Event const *Event)
    {
        switch (Event->Type)
        {
            case RR_EVENT_TYPE_DROP_FILE:
            {
                SPNGImage PNGImage{ Event->DropFile.Path };
                InitImage2D(PNGImage);
            }
            break;
            case RR_EVENT_TYPE_SWAPCHAIN_CREATED:
            {
                InitCamera();
                return;
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
                BoxBlur2D.Blur(
                    Graph,
                    OriginalImage2D,
                    IntermediateImageA,
                    IntermediateImageB,
                    BlurredImage2D,
                    Blur2DPasses);
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
        Rr_BeginGraphLabel(Graph, "DrawBlur2D");

        Rr_ColorTarget ColorTarget = {
            .Image = Rr_GetSwapchainImage(),
            .LoadOp = RR_LOAD_OP_DONT_CARE,
            .StoreOp = RR_STORE_OP_STORE,
        };
        Rr_GraphNode *GraphicsNode =
            Rr_AddGraphicsNode(Graph, 1, &ColorTarget, nullptr);
        Rr_BindGraphicsPipeline(GraphicsNode, QuadGraphicsPipeline);
        Rr_BindCombinedImage2DSampler(
            GraphicsNode,
            BlurredImage2D,
            Sampler,
            0,
            0);
        Rr_Draw(GraphicsNode, 6, 1, 0, 0);

        Rr_EndGraphLabel(Graph, "DrawBlur2D");
    }

    void DrawCube(Rr_Graph *Graph)
    {
        Rr_BeginGraphLabel(Graph, "DrawBlurCube");

        Camera.Update();

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
            Rr_AddGraphicsNode(Graph, 1, &ColorTarget, nullptr);
        Rr_BindGraphicsPipeline(GraphicsNode, CubeGraphicsPipeline);
        Rr_BindVertexBuffer(GraphicsNode, Cube.Buffer, 0, 0);
        Rr_BindIndexBuffer(
            GraphicsNode,
            Cube.Buffer,
            0,
            Cube.IndexOffset,
            RR_INDEX_TYPE_UINT16);
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
        Rr_DrawIndexed(GraphicsNode, Cube.IndexCount, 1, 0, 0, 0);

        Rr_EndGraphLabel(Graph, "DrawBlurCube");
    }

    void Iterate()
    {
        Rr_Graph *Graph = Rr_GetGraph();

        Rr_UIBeginWindowEx(
            "Blur.cxx",
            nullptr,
            RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT);
        Rr_UIText(
            "This example demonstrates various blur algorithms.\nYou can drop "
            "a PNG image into the window to blur it (works only with 2D "
            "algorithms).");
        Rr_UISeparator();
        std::array BlurTypes = { "Box 2D",
                                 "Kawase 2D",
                                 "Dual Kawase 2D",
                                 "Box Cube" };
        Rr_UICombobox(
            "Mode",
            BlurTypes.size(),
            BlurTypes.data(),
            (std::uint32_t *)&Type);
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
                Rr_UISliderFloat(
                    "Sample Position Multiplier",
                    &KawaseBlur2DMultiplier,
                    0.1f,
                    25.0f);

                Draw2D(Graph);
            }
            break;
            case EBlurType::DUAL_KAWASE_2D:
            {
                Rr_UISliderInt(
                    "Downsample Levels",
                    &DualKawaseBlur2DLevels,
                    0,
                    4);
                Rr_UISliderFloat(
                    "Sample Position Multiplier",
                    &DualKawaseBlur2DMultiplier,
                    0.1f,
                    25.0f);

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
    }

    void InitIntermediateImages()
    {
        IntermediateImageA = Rr_CreateImage2D(
            { MAX_IMAGE_SIZE, MAX_IMAGE_SIZE },
            RR_IMAGE_FORMAT_R8G8B8A8_UNORM,
            RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_STORAGE_BIT |
                RR_IMAGE_FLAGS_SAMPLED_BIT);
        IntermediateImageB = Rr_CreateImage2D(
            { MAX_IMAGE_SIZE, MAX_IMAGE_SIZE },
            RR_IMAGE_FORMAT_R8G8B8A8_UNORM,
            RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_STORAGE_BIT |
                RR_IMAGE_FLAGS_SAMPLED_BIT);
    }

    SBlurApp()
        : BoxBlur2D(Blur2DKernelSize)
        , DualKawaseBlur2D()
        , BoxBlurCube(RR_IMAGE_FORMAT_R8G8B8A8_UNORM, 512, BlurCubeRadius)
    {
        InitCamera();
        InitIntermediateImages();
        InitQuadPipeline();
        InitCubePipeline();
        InitUniformBuffer();
        InitSampler();
        InitImage2D(EXAMPLE_ASSET_BACK_PNG);
        InitImageCube();
        Cube.Init();
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
        Cube.Cleanup();
    }
};

int main()
{
    static SBlurApp *App{};

    Rr_Config Config = {
        .WindowTitle = "Blur",
        .WindowFlags = RR_WINDOW_FLAGS_RESIZE_BIT,
        .InitFunc = []() { App = new SBlurApp(); },
        .EventFunc = [](Rr_Event const *Event) { App->Event(Event); },
        .IterateFunc = []() { App->Iterate(); },
        .CleanupFunc = []() { delete App; },
    };
    Rr_Run(&Config);
}
