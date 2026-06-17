#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include <array>
#include <cstring>
#include <iostream>
#include <numeric>
#include <stdlib.h>
#include <vector>

static Rr_ComputePipeline *Pipeline;
static Rr_Buffer *InputBuffer;
static Rr_Buffer *OutputBuffer;
static Rr_Buffer *WorkgroupBuffer;
static Rr_Buffer *UniformBuffer;
static uint32_t ThreadsPerWorkgroup;

std::vector<uint32_t> Numbers;

static const uint32_t COUNT = 456456;

static uint32_t GetPrefixSum(uint32_t Number)
{
    uint32_t Result = 0;
    for (uint32_t Index = 0; Index <= Number; ++Index)
    {
        Result = Index + Result;
    }
    return Result;
}

static uint32_t GetDispatchSize()
{
    uint32_t DispatchSize = COUNT / ThreadsPerWorkgroup;
    if ((COUNT % ThreadsPerWorkgroup) != 0)
    {
        DispatchSize++;
    }
    return DispatchSize;
}

static void Init()
{
    ThreadsPerWorkgroup = Rr_GetMaxComputeWorkgroupInvocations();
    std::array Specializations = {
        Rr_PipelineSpecialization{ 0, sizeof(ThreadsPerWorkgroup), &ThreadsPerWorkgroup },
    };

    Rr_Asset ComputeShader = Rr_LoadAsset(EXAMPLE_ASSET_PREFIXSUM_COMP_SPV);
    Rr_ShaderInfo ShaderInfo = {
        .SPVSize = ComputeShader.Size,
        .SPVData = ComputeShader.Data,
        .SpecializationCount = Specializations.size(),
        .Specializations = Specializations.data(),
    };

    Pipeline = Rr_CreateComputePipeline(&ShaderInfo);

    Numbers.resize(COUNT);
    std::iota(Numbers.begin(), Numbers.end(), 0);
    size_t NumbersSize = sizeof(uint32_t) * Numbers.size();
    InputBuffer = Rr_CreateBuffer(NumbersSize, RR_BUFFER_FLAGS_STORAGE_BIT);

    OutputBuffer = Rr_CreateBuffer(NumbersSize, RR_BUFFER_FLAGS_STORAGE_BIT);

    WorkgroupBuffer = Rr_CreateBuffer(GetDispatchSize() * sizeof(uint32_t), RR_BUFFER_FLAGS_STORAGE_BIT);

    UniformBuffer = Rr_CreateBuffer(
        sizeof(Rr_IntVec4),
        RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_STAGING_BIT |
            RR_BUFFER_FLAGS_PER_FRAME_BIT);

    std::cout << GetPrefixSum(COUNT - 1) << std::endl;
}

static void Iterate()
{
    Rr_Graph *Graph = Rr_GetGraph();

    size_t NumbersSize = COUNT * sizeof(uint32_t);

    std::memcpy(Rr_GetMappedBufferData(UniformBuffer), &COUNT, sizeof(uint32_t));

    Rr_GraphNode *ComputeNode = Rr_AddComputeNode(Graph);
    Rr_BindComputePipeline(ComputeNode, Pipeline);
    Rr_BindUniformBuffer(ComputeNode, UniformBuffer, 0, 0, 0, sizeof(uint32_t));
    Rr_BindStorageBufferRW(ComputeNode, InputBuffer, 0, 1, 0, NumbersSize);
    Rr_BindStorageBufferRW(ComputeNode, OutputBuffer, 0, 2, 0, NumbersSize);
    Rr_BindStorageBufferRW(ComputeNode, WorkgroupBuffer, 0, 3, 0, GetDispatchSize() * sizeof(uint32_t));
    Rr_Dispatch(ComputeNode, GetDispatchSize(), 1, 1);

    Rr_Image2D *SwapchainImage = Rr_GetSwapchainImage();

    Rr_ColorTarget ColorTarget = {
        .Image = SwapchainImage,
        .LoadOp = RR_LOAD_OP_CLEAR,
        .StoreOp = RR_STORE_OP_STORE,
        .Clear = { 0.5, 0.0, 0.5, 1.0 },
    };

    Rr_GraphNode *GraphicsNode = Rr_AddGraphicsNode(Graph, 1, &ColorTarget, nullptr);
    // Rr_BindStorageBuffer(GraphicsNode, InputBuffer, 0, 0, 0, NumbersSize);
    // Rr_BindStorageBuffer(GraphicsNode, OutputBuffer, 0, 1, 0, NumbersSize);
}

static void Cleanup()
{
    Rr_ReleaseBuffer(InputBuffer);
    Rr_ReleaseBuffer(OutputBuffer);
    Rr_ReleaseBuffer(WorkgroupBuffer);
    Rr_ReleaseBuffer(UniformBuffer);
    Rr_ReleaseComputePipeline(Pipeline);
}

int main()
{
    Rr_Config Config = {
        .WindowTitle = "PrefixSum",
        .InitFunc = Init,
        .IterateFunc = Iterate,
        .CleanupFunc = Cleanup,
    };
    Rr_Run(&Config);
}
