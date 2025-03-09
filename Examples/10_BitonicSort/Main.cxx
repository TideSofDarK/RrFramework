#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <ctime>
#include <vector>

const uint32_t COUNT = 1 << 15;
const uint32_t TOTAL_SIZE = sizeof(uint32_t) * COUNT;

struct SValidator
{
    Rr_Renderer *Renderer;
    uint32_t ThreadsPerWorkgroup;
    Rr_PipelineLayout *Layout;
    Rr_ComputePipeline *Pipeline;
    Rr_Image *ResultImage;

    explicit SValidator(Rr_Renderer *Renderer)
        : Renderer(Renderer)
        , ThreadsPerWorkgroup(Rr_GetMaxComputeWorkgroupInvocations(Renderer))
    {
        std::array Bindings = {
            Rr_PipelineBinding{ 0, 1, RR_PIPELINE_BINDING_TYPE_STORAGE_BUFFER },
            Rr_PipelineBinding{ 1, 1, RR_PIPELINE_BINDING_TYPE_STORAGE_BUFFER },
            Rr_PipelineBinding{ 2, 1, RR_PIPELINE_BINDING_TYPE_STORAGE_IMAGE },
        };
        std::array BindingSets = {
            Rr_PipelineBindingSet{
                Bindings.size(),
                Bindings.data(),
                RR_SHADER_STAGE_COMPUTE_BIT,
            },
        };
        Layout = Rr_CreatePipelineLayout(
            Renderer,
            BindingSets.size(),
            BindingSets.data());

        std::array Specializations = {
            Rr_PipelineSpecialization{
                0,
                RR_MAKE_DATA_STRUCT(ThreadsPerWorkgroup) },
        };

        Rr_ComputePipelineCreateInfo PipelineCreateInfo = {};
        PipelineCreateInfo.Layout = Layout;
        PipelineCreateInfo.ShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_VALIDATE_COMP_SPV);
        PipelineCreateInfo.SpecializationCount = Specializations.size();
        PipelineCreateInfo.Specializations = Specializations.data();

        Pipeline = Rr_CreateComputePipeline(Renderer, &PipelineCreateInfo);

        ResultImage = Rr_CreateImage(
            Renderer,
            { 2, 2, 1 },
            RR_TEXTURE_FORMAT_R8G8B8A8_UNORM,
            RR_IMAGE_FLAGS_STORAGE_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT);
    }

    ~SValidator()
    {
        Rr_DestroyComputePipeline(Renderer, Pipeline);
        Rr_DestroyPipelineLayout(Renderer, Layout);
        Rr_DestroyImage(Renderer, ResultImage);
    }

    Rr_Image *Validate(
        uint32_t Count,
        Rr_Buffer *SortedBuffer,
        Rr_Buffer *UnsortedBuffer)
    {
        assert(RR_IS_POW2(Count));

        Rr_GraphNode *ComputeNode = Rr_AddComputeNode(Renderer, "validate");
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
        Rr_BindStorageImage(ComputeNode, ResultImage, 0, 2);
        Rr_Dispatch(ComputeNode, Count / ThreadsPerWorkgroup, 1, 1);

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

    Rr_Renderer *Renderer;
    uint32_t ThreadsPerWorkgroup;
    Rr_PipelineLayout *Layout;
    Rr_ComputePipeline *Pipeline;
    Rr_Buffer *UniformBuffer;

    explicit SBitonicSorter(Rr_Renderer *Renderer)
        : Renderer(Renderer)
        , ThreadsPerWorkgroup(
              Rr_NextPowerOfTwo(
                  Rr_GetMaxComputeWorkgroupInvocations(Renderer)) /
              2)
    {
        std::array Bindings = {
            Rr_PipelineBinding{ 0, 1, RR_PIPELINE_BINDING_TYPE_STORAGE_BUFFER },
            Rr_PipelineBinding{ 1, 1, RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER },
        };
        std::array BindingSets = {
            Rr_PipelineBindingSet{
                Bindings.size(),
                Bindings.data(),
                RR_SHADER_STAGE_COMPUTE_BIT,
            },
        };
        Layout = Rr_CreatePipelineLayout(
            Renderer,
            BindingSets.size(),
            BindingSets.data());

        std::array Specializations = {
            Rr_PipelineSpecialization{
                0,
                RR_MAKE_DATA_STRUCT(ThreadsPerWorkgroup) },
        };

        Rr_ComputePipelineCreateInfo PipelineCreateInfo = {};
        PipelineCreateInfo.Layout = Layout;
        PipelineCreateInfo.ShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_BITONICSORT_COMP_SPV);
        PipelineCreateInfo.SpecializationCount = Specializations.size();
        PipelineCreateInfo.Specializations = Specializations.data();

        Pipeline = Rr_CreateComputePipeline(Renderer, &PipelineCreateInfo);

        UniformBuffer = Rr_CreateBuffer(
            Renderer,
            sizeof(uint32_t) * 1024,
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_STAGING_BIT |
                RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT);
    }

    ~SBitonicSorter()
    {
        Rr_DestroyComputePipeline(Renderer, Pipeline);
        Rr_DestroyPipelineLayout(Renderer, Layout);
        Rr_DestroyBuffer(Renderer, UniformBuffer);
    }

    void Sort(uint32_t Count, Rr_Buffer *Buffer)
    {
        assert(RR_IS_POW2(Count));

        uint32_t DispatchSize = Count / 2 / ThreadsPerWorkgroup;

        size_t InfoBufferOffset = 0;
        auto Dispatch = [&](uint32_t Height, uint32_t Algorithm) {
            SGPUSortInfo SortInfo;
            SortInfo.Count = Count;
            SortInfo.Height = Height;
            SortInfo.Algorithm = Algorithm;

            char *Dst =
                (char *)Rr_GetMappedBufferData(Renderer, UniformBuffer) +
                InfoBufferOffset;
            std::memcpy(Dst, &SortInfo, sizeof(SGPUSortInfo));

            Rr_GraphNode *ComputeNode = Rr_AddComputeNode(Renderer, "compute");
            Rr_BindComputePipeline(ComputeNode, Pipeline);
            Rr_BindStorageBuffer(
                ComputeNode,
                Buffer,
                0,
                0,
                0,
                sizeof(uint32_t) * Count);
            Rr_BindUniformBuffer(
                ComputeNode,
                UniformBuffer,
                0,
                1,
                InfoBufferOffset,
                sizeof(SGPUSortInfo));
            Rr_Dispatch(ComputeNode, DispatchSize, 1, 1);

            InfoBufferOffset += RR_ALIGN_POW2(
                sizeof(SGPUSortInfo),
                Rr_GetUniformAlignment(Renderer));
        };

        uint32_t Height = ThreadsPerWorkgroup * 2;

        Dispatch(Height, LOCAL_SORT);

        Height *= 2;

        for(; Height <= Count; Height *= 2)
        {
            Dispatch(Height, BIG_FLIP);

            for(uint32_t DisperseHeight = Height / 2; DisperseHeight > 1;
                DisperseHeight /= 2)
            {
                if(DisperseHeight <= ThreadsPerWorkgroup * 2)
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

static void Init(Rr_App *App, void *UserData)
{
    std::srand((unsigned int)std::time(NULL));

    Rr_Renderer *Renderer = Rr_GetRenderer(App);

    RandomNumbers.resize(COUNT);
    SortedNumbers.resize(COUNT);

    RandomNumbersBuffer = Rr_CreateBuffer(
        Renderer,
        sizeof(uint32_t) * COUNT,
        RR_BUFFER_FLAGS_STORAGE_BIT);

    SortedNumbersBuffer = Rr_CreateBuffer(
        Renderer,
        sizeof(uint32_t) * COUNT,
        RR_BUFFER_FLAGS_STORAGE_BIT);

    StagingBuffer = Rr_CreateBuffer(
        Renderer,
        sizeof(uint32_t) * COUNT * 2,
        RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT |
            RR_BUFFER_FLAGS_PER_FRAME_BIT);

    Sorter = new SBitonicSorter(Renderer);
    Validator = new SValidator(Renderer);
}

static void Iterate(Rr_App *App, void *UserData)
{
    Rr_Renderer *Renderer = Rr_GetRenderer(App);

    /* Upload both sorted and unsorted buffers and validate results. */

    std::generate(RandomNumbers.begin(), RandomNumbers.end(), []() {
        return std::rand() % COUNT;
    });

    std::memcpy(SortedNumbers.data(), RandomNumbers.data(), TOTAL_SIZE);
    std::sort(SortedNumbers.begin(), SortedNumbers.end());

    std::memcpy(
        Rr_GetMappedBufferData(Renderer, StagingBuffer),
        RandomNumbers.data(),
        TOTAL_SIZE);
    std::memcpy(
        TOTAL_SIZE + (char *)Rr_GetMappedBufferData(Renderer, StagingBuffer),
        SortedNumbers.data(),
        TOTAL_SIZE);

    Rr_GraphNode *TransferNode = Rr_AddTransferNode(Renderer, "upload");
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

    Rr_Image *ResultImage =
        Validator->Validate(COUNT, SortedNumbersBuffer, RandomNumbersBuffer);

    Rr_Image *SwapchainImage = Rr_GetSwapchainImage(Renderer);
    Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize(Renderer);
    Rr_AddBlitNode(
        Renderer,
        "blit",
        ResultImage,
        SwapchainImage,
        { 0, 0, 2, 2 },
        { 0, 0, SwapchainSize.Width, SwapchainSize.Height },
        RR_IMAGE_ASPECT_COLOR_BIT);
}

static void Cleanup(Rr_App *App, void *UserData)
{
    Rr_Renderer *Renderer = Rr_GetRenderer(App);

    delete Sorter;
    delete Validator;

    Rr_DestroyBuffer(Renderer, RandomNumbersBuffer);
    Rr_DestroyBuffer(Renderer, SortedNumbersBuffer);
    Rr_DestroyBuffer(Renderer, StagingBuffer);
}

int main()
{
    Rr_AppConfig Config = {};
    Config.Title = "10_BitonicSort";
    Config.Version = "1.0.0";
    Config.Package = "com.rr.examples.10_BitonicSort";
    Config.InitFunc = Init;
    Config.CleanupFunc = Cleanup;
    Config.IterateFunc = Iterate;
    Rr_Run(&Config);
}
