#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <ctime>
#include <vector>

const uint32_t COUNT_SQRT = 256;
const uint32_t COUNT = COUNT_SQRT * COUNT_SQRT;
const uint32_t TOTAL_SIZE = sizeof(uint32_t) * COUNT;

struct SValidator
{
    Rr_Renderer *Renderer;
    uint32_t DispatchSize;
    Rr_PipelineLayout *Layout;
    Rr_ComputePipeline *Pipeline;
    Rr_Image2D *ResultImage;

    explicit SValidator()
    {
        std::array Bindings = {
            Rr_Binding{
                .Index = 0,
                .Type = RR_BINDING_TYPE_STORAGE_BUFFER,
                .Stages = RR_SHADER_STAGE_COMPUTE_BIT,
            },
            Rr_Binding{
                .Index = 1,
                .Type = RR_BINDING_TYPE_STORAGE_BUFFER,
                .Stages = RR_SHADER_STAGE_COMPUTE_BIT,
            },
            Rr_Binding{
                .Index = 2,
                .Type = RR_BINDING_TYPE_STORAGE_IMAGE,
                .Stages = RR_SHADER_STAGE_COMPUTE_BIT,
            },
        };
        std::array BindingSets = {
            Rr_BindingSet{ Bindings.size(), Bindings.data() },
        };
        Layout =
            Rr_CreatePipelineLayout(BindingSets.size(), BindingSets.data());

        uint32_t LocalSize = std::sqrt(Rr_GetMaxComputeWorkgroupInvocations());
        DispatchSize = COUNT_SQRT / LocalSize;

        std::array Specializations = {
            Rr_PipelineSpecialization{
                .ConstantID = 0,
                .Size = sizeof(LocalSize),
                .Data = &LocalSize,
            },
            Rr_PipelineSpecialization{
                .ConstantID = 1,
                .Size = sizeof(LocalSize),
                .Data = &LocalSize,
            },
            Rr_PipelineSpecialization{
                .ConstantID = 2,
                .Size = sizeof(COUNT_SQRT),
                .Data = &COUNT_SQRT,
            },
        };

        Rr_Asset ComputeShader = Rr_LoadAsset(EXAMPLE_ASSET_VALIDATE_COMP_SPV);

        Rr_ComputePipelineCreateInfo PipelineCreateInfo = {};
        PipelineCreateInfo.Layout = Layout;
        PipelineCreateInfo.ShaderSPVSize = ComputeShader.Size;
        PipelineCreateInfo.ShaderSPVData = ComputeShader.Pointer;
        PipelineCreateInfo.SpecializationCount = Specializations.size();
        PipelineCreateInfo.Specializations = Specializations.data();

        Pipeline = Rr_CreateComputePipeline(&PipelineCreateInfo);

        ResultImage = Rr_CreateImage2D(
            { COUNT_SQRT, COUNT_SQRT },
            RR_IMAGE_FORMAT_R8G8B8A8_UNORM,
            RR_IMAGE_FLAGS_STORAGE_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT);
    }

    ~SValidator()
    {
        Rr_ReleaseComputePipeline(Pipeline);
        Rr_ReleasePipelineLayout(Layout);
        Rr_ReleaseImage(ResultImage);
    }

    Rr_Image2D *Validate(
        uint32_t Count,
        Rr_Buffer *SortedBuffer,
        Rr_Buffer *UnsortedBuffer)
    {
        assert(RR_IS_POW2(Count));

        Rr_GraphNode *ComputeNode = Rr_AddComputeNode(Rr_GetGraph());
        Rr_BindComputePipeline(ComputeNode, Pipeline);
        Rr_BindStorageBuffer(
            ComputeNode,
            SortedBuffer,
            0,
            0,
            0,
            sizeof(uint32_t) * Count);
        Rr_BindStorageBuffer(
            ComputeNode,
            UnsortedBuffer,
            0,
            1,
            0,
            sizeof(uint32_t) * Count);
        Rr_BindStorageImage2DRW(ComputeNode, ResultImage, 0, 2);
        Rr_Dispatch(ComputeNode, DispatchSize, DispatchSize, 1);

        return ResultImage;
    }
};

struct SBitonicSorter
{
    const uint32_t LOCAL_SORT = 0;
    const uint32_t LOCAL_DISPERSE = 1;
    const uint32_t BIG_FLIP = 2;
    const uint32_t BIG_DISPERSE = 3;

    struct SGPUSortInfo
    {
        uint32_t Count;
        uint32_t Height;
        uint32_t Algorithm;
    };

    uint32_t ThreadsPerWorkgroup;
    Rr_PipelineLayout *Layout;
    Rr_ComputePipeline *Pipeline;
    Rr_Buffer *UniformBuffer;

    explicit SBitonicSorter()
        : ThreadsPerWorkgroup(
              Rr_NextPowerOfTwo(Rr_GetMaxComputeWorkgroupInvocations()) / 2)
    {
        std::array Bindings0 = {
            Rr_Binding{
                .Index = 0,
                .Type = RR_BINDING_TYPE_STORAGE_BUFFER,
                .Stages = RR_SHADER_STAGE_COMPUTE_BIT,
            },
        };
        std::array Bindings1 = {
            Rr_Binding{
                .Index = 0,
                .Type = RR_BINDING_TYPE_UNIFORM_BUFFER,
                .Stages = RR_SHADER_STAGE_COMPUTE_BIT,
            },
        };
        std::array BindingSets = {
            Rr_BindingSet{ Bindings0.size(), Bindings0.data() },
            Rr_BindingSet{ Bindings1.size(), Bindings1.data() },
        };
        Layout =
            Rr_CreatePipelineLayout(BindingSets.size(), BindingSets.data());

        std::array Specializations = {
            Rr_PipelineSpecialization{
                .ConstantID = 0,
                .Size = sizeof(ThreadsPerWorkgroup),
                .Data = &ThreadsPerWorkgroup,
            },
        };

        Rr_Asset ComputeShader =
            Rr_LoadAsset(EXAMPLE_ASSET_BITONICSORT_COMP_SPV);

        Rr_ComputePipelineCreateInfo PipelineCreateInfo = {};
        PipelineCreateInfo.Layout = Layout;
        PipelineCreateInfo.ShaderSPVSize = ComputeShader.Size;
        PipelineCreateInfo.ShaderSPVData = ComputeShader.Pointer;
        PipelineCreateInfo.SpecializationCount = Specializations.size();
        PipelineCreateInfo.Specializations = Specializations.data();

        Pipeline = Rr_CreateComputePipeline(&PipelineCreateInfo);

        UniformBuffer = Rr_CreateBuffer(
            sizeof(uint32_t) * 1024,
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_STAGING_BIT |
                RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT);
    }

    ~SBitonicSorter()
    {
        Rr_ReleaseComputePipeline(Pipeline);
        Rr_ReleasePipelineLayout(Layout);
        Rr_ReleaseBuffer(UniformBuffer);
    }

    void Sort(uint32_t Count, Rr_Buffer *Buffer)
    {
        assert(RR_IS_POW2(Count));

        Rr_GraphNode *ComputeNode = Rr_AddComputeNode(Rr_GetGraph());
        Rr_BindComputePipeline(ComputeNode, Pipeline);
        Rr_BindStorageBufferRW(
            ComputeNode,
            Buffer,
            0,
            0,
            0,
            sizeof(uint32_t) * Count);

        uint32_t DispatchSize = Count / 2 / ThreadsPerWorkgroup;
        size_t InfoBufferOffset = 0;
        auto Dispatch = [&](uint32_t Height, uint32_t Algorithm) {
            SGPUSortInfo SortInfo;
            SortInfo.Count = Count;
            SortInfo.Height = Height;
            SortInfo.Algorithm = Algorithm;

            char *Dst = (char *)Rr_GetMappedBufferData(UniformBuffer) +
                        InfoBufferOffset;
            std::memcpy(Dst, &SortInfo, sizeof(SGPUSortInfo));

            Rr_BindUniformBuffer(
                ComputeNode,
                UniformBuffer,
                1,
                0,
                InfoBufferOffset,
                sizeof(SGPUSortInfo));
            Rr_Dispatch(ComputeNode, DispatchSize, 1, 1);
            Rr_ComputeBarrier(ComputeNode);

            InfoBufferOffset +=
                RR_ALIGN_POW2(sizeof(SGPUSortInfo), Rr_GetUniformAlignment());
        };

        uint32_t Height = ThreadsPerWorkgroup * 2;

        Dispatch(Height, LOCAL_SORT);

        Height *= 2;

        for (; Height <= Count; Height *= 2)
        {
            Dispatch(Height, BIG_FLIP);

            for (uint32_t DisperseHeight = Height / 2; DisperseHeight > 1;
                 DisperseHeight /= 2)
            {
                if (DisperseHeight <= ThreadsPerWorkgroup * 2)
                {
                    Dispatch(DisperseHeight, LOCAL_DISPERSE);
                    break;
                }
                else
                {

                    Dispatch(DisperseHeight, BIG_DISPERSE);
                }
            }
        }
    }
};

std::vector<uint32_t> RandomNumbers;
Rr_Buffer *RandomNumbersBuffer;

std::vector<uint32_t> SortedNumbers;
Rr_Buffer *SortedNumbersBuffer;

Rr_Buffer *StagingBuffer;

SBitonicSorter *Sorter;
SValidator *Validator;

static void Init()
{
    std::srand((unsigned int)std::time(NULL));

    RandomNumbers.resize(COUNT);
    SortedNumbers.resize(COUNT);

    RandomNumbersBuffer =
        Rr_CreateBuffer(sizeof(uint32_t) * COUNT, RR_BUFFER_FLAGS_STORAGE_BIT);

    SortedNumbersBuffer =
        Rr_CreateBuffer(sizeof(uint32_t) * COUNT, RR_BUFFER_FLAGS_STORAGE_BIT);

    StagingBuffer = Rr_CreateBuffer(
        sizeof(uint32_t) * COUNT * 2,
        RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT |
            RR_BUFFER_FLAGS_PER_FRAME_BIT);

    Sorter = new SBitonicSorter();
    Validator = new SValidator();
}

static void Iterate()
{
    Rr_Graph *Graph = Rr_GetGraph();

    /* Upload both sorted and unsorted buffers and validate results. */

    std::generate(RandomNumbers.begin(), RandomNumbers.end(), []() {
        return std::rand() % COUNT;
    });

    std::memcpy(SortedNumbers.data(), RandomNumbers.data(), TOTAL_SIZE);
    std::sort(SortedNumbers.begin(), SortedNumbers.end());

    std::memcpy(
        Rr_GetMappedBufferData(StagingBuffer),
        RandomNumbers.data(),
        TOTAL_SIZE);
    std::memcpy(
        TOTAL_SIZE + (char *)Rr_GetMappedBufferData(StagingBuffer),
        SortedNumbers.data(),
        TOTAL_SIZE);

    Rr_TransferNode *TransferNode = Rr_AddTransferNode(Rr_GetGraph());
    Rr_TransferBufferData(
        TransferNode,
        TOTAL_SIZE,
        StagingBuffer,
        0,
        RandomNumbersBuffer,
        0);
    Rr_TransferBufferData(
        TransferNode,
        TOTAL_SIZE,
        StagingBuffer,
        TOTAL_SIZE,
        SortedNumbersBuffer,
        0);

    Sorter->Sort(COUNT, RandomNumbersBuffer);

    Rr_Image2D *ResultImage =
        Validator->Validate(COUNT, SortedNumbersBuffer, RandomNumbersBuffer);

    Rr_Image2D *SwapchainImage = Rr_GetSwapchainImage();
    Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize();
    Rr_BlitImage2D(
        Rr_GetGraph(),
        ResultImage,
        SwapchainImage,
        { 0, 0, COUNT_SQRT, COUNT_SQRT },
        { 0, 0, SwapchainSize.Width, SwapchainSize.Height },
        RR_IMAGE_ASPECT_COLOR_BIT);
}

static void Cleanup()
{
    delete Sorter;
    delete Validator;

    Rr_ReleaseBuffer(RandomNumbersBuffer);
    Rr_ReleaseBuffer(SortedNumbersBuffer);
    Rr_ReleaseBuffer(StagingBuffer);
}

int main()
{
    Rr_AppConfig Config = {};
    Config.Title = "BitonicSort";
    Config.InitFunc = Init;
    Config.CleanupFunc = Cleanup;
    Config.IterateFunc = Iterate;
    Rr_Run(&Config);
}
