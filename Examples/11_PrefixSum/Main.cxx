#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include <array>
#include <cfloat>
#include <cstring>
#include <iostream>
#include <numeric>
#include <vector>

static Rr_PipelineLayout *Layout;
static Rr_ComputePipeline *Pipeline;
static Rr_Buffer *Buffer;
static Rr_Buffer *UniformBuffer;

std::vector<uint32_t> Numbers;

const uint32_t COUNT = 456456;

uint32_t GetPrefixSum(uint32_t Number)
{
    uint32_t Result = 0;
    for(uint32_t Index = 0; Index <= Number; ++Index)
    {
        Result = Index + Result;
    }
    return Result;
}

static void Init(Rr_App *App, void *UserData)
{
    Rr_Renderer *Renderer = Rr_GetRenderer(App);

    std::array Bindings = {
        Rr_PipelineBinding{ 0, 1, RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER },
        Rr_PipelineBinding{ 1, 1, RR_PIPELINE_BINDING_TYPE_STORAGE_BUFFER },
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

    uint32_t ThreadsPerWorkgroup = Rr_GetMaxComputeWorkgroupInvocations(Renderer);
    std::array Specializations = {
        Rr_PipelineSpecialization{ 0,
                                   RR_MAKE_DATA_STRUCT(ThreadsPerWorkgroup) },
    };

    Rr_ComputePipelineCreateInfo PipelineCreateInfo = {};
    PipelineCreateInfo.Layout = Layout;
    PipelineCreateInfo.ShaderSPV =
        Rr_LoadAsset(EXAMPLE_ASSET_PREFIXSUM_COMP_SPV);
    PipelineCreateInfo.SpecializationCount = Specializations.size();
    PipelineCreateInfo.Specializations = Specializations.data();

    Pipeline = Rr_CreateComputePipeline(Renderer, &PipelineCreateInfo);

    Numbers.resize(COUNT);
    std::iota(Numbers.begin(), Numbers.end(), 0);
    size_t NumbersSize = sizeof(uint32_t) * Numbers.size();
    Buffer = Rr_CreateBuffer(
        Renderer,
        NumbersSize,
        RR_BUFFER_FLAGS_STORAGE_BIT);

    UniformBuffer = Rr_CreateBuffer(
        Renderer,
        sizeof(Rr_IntVec4),
        RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_MAPPED_BIT |
            RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT);

    std::cout << GetPrefixSum(COUNT - 1) << std::endl;
}

static void Iterate(Rr_App *App, void *UserData)
{
    Rr_Renderer *Renderer = Rr_GetRenderer(App);

    size_t NumbersSize = COUNT * sizeof(uint32_t);

    std::memcpy(
        Rr_GetMappedBufferData(Renderer, UniformBuffer),
        &COUNT,
        sizeof(uint32_t));

    Rr_GraphNode *ComputeNode = Rr_AddComputeNode(Renderer, "compute");
    Rr_BindComputePipeline(ComputeNode, Pipeline);
    Rr_BindUniformBuffer(ComputeNode, UniformBuffer, 0, 0, 0, sizeof(uint32_t));
    Rr_BindStorageBuffer(ComputeNode, Buffer, 0, 1, 0, NumbersSize);
    Rr_Dispatch(ComputeNode, 1, 1, 1);

    Rr_Image *SwapchainImage = Rr_GetSwapchainImage(Renderer);

    Rr_ColorTarget ColorTarget;
    ColorTarget.Clear = { 0.5, 0.0, 0.5, 1.0 };
    ColorTarget.LoadOp = RR_LOAD_OP_CLEAR;
    ColorTarget.Slot = 0;
    ColorTarget.StoreOp = RR_STORE_OP_STORE;

    Rr_GraphNode *GraphicsNode = Rr_AddGraphicsNode(
        Renderer,
        "clear",
        1,
        &ColorTarget,
        &SwapchainImage,
        nullptr,
        nullptr);
    Rr_BindStorageBuffer(GraphicsNode, Buffer, 0, 0, 0, NumbersSize);
}

static void Cleanup(Rr_App *App, void *UserData)
{
    Rr_Renderer *Renderer = Rr_GetRenderer(App);

    Rr_DestroyBuffer(Renderer, Buffer);
    Rr_DestroyBuffer(Renderer, UniformBuffer);
    Rr_DestroyComputePipeline(Renderer, Pipeline);
    Rr_DestroyPipelineLayout(Renderer, Layout);
}

int main()
{
    Rr_AppConfig Config = {};
    Config.Title = "11_PrefixSum";
    Config.Version = "1.0.0";
    Config.Package = "com.rr.examples.11_prefixsum";
    Config.InitFunc = Init;
    Config.CleanupFunc = Cleanup;
    Config.IterateFunc = Iterate;
    Rr_Run(&Config);
}
