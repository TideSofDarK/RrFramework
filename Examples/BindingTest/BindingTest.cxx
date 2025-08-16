#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include <array>

struct alignas(16) SS0B0Element
{
    Rr_Vec4 ZeroVector;
    Rr_Vec4 OneVector;
};

struct alignas(16) SS1B1Element
{
    Rr_Vec4 TwoVector;
    std::uint32_t ThousandU32;
};

struct alignas(16) SS2Element
{
    Rr_Vec4 Vector;
    std::uint32_t U32;
};

struct alignas(16) SS2B4Element
{
    Rr_Vec4 Vector;
    std::uint32_t U32;
};

struct SApp
{
    Rr_PipelineLayout *PipelineLayout;
    Rr_ComputePipeline *ComputePipeline;

    Rr_Buffer *UniformBufferA;
    Rr_Buffer *UniformBufferB;
    Rr_Buffer *StorageBuffer;
    Rr_Buffer *ReadonlyStorageBuffer;

    Rr_Image2D *StorageImageA;
    Rr_Image2D *StorageImageB;

    int UniformAlignment;

    void InitPipeline()
    {
        std::array Set0Bindings = {
            Rr_PipelineBinding{ 0, 1, RR_PIPELINE_BINDING_TYPE_STORAGE_BUFFER },
        };
        std::array Set1Bindings = {
            Rr_PipelineBinding{ 1, 1, RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER },
        };
        std::array Set2Bindings = {
            Rr_PipelineBinding{ 2, 1, RR_PIPELINE_BINDING_TYPE_STORAGE_BUFFER },
            Rr_PipelineBinding{ 4, 4, RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER },
        };
        std::array Set3Bindings = {
            Rr_PipelineBinding{ 13, 1, RR_PIPELINE_BINDING_TYPE_STORAGE_IMAGE },
        };
        std::array Sets = {
            Rr_PipelineBindingSet{
                Set0Bindings.size(),
                Set0Bindings.data(),
                RR_SHADER_STAGE_COMPUTE_BIT,
            },
            Rr_PipelineBindingSet{
                Set1Bindings.size(),
                Set1Bindings.data(),
                RR_SHADER_STAGE_COMPUTE_BIT,
            },
            Rr_PipelineBindingSet{
                Set2Bindings.size(),
                Set2Bindings.data(),
                RR_SHADER_STAGE_COMPUTE_BIT,
            },
            Rr_PipelineBindingSet{
                Set3Bindings.size(),
                Set3Bindings.data(),
                RR_SHADER_STAGE_COMPUTE_BIT,
            },
        };
        PipelineLayout =
            Rr_CreatePipelineLayout((uint32_t)Sets.size(), Sets.data());

        uint32_t LocalSize = 16;

        std::array Specializations = {
            Rr_PipelineSpecialization{
                0,
                RR_MAKE_DATA_STRUCT(LocalSize),
            },
            Rr_PipelineSpecialization{
                1,
                RR_MAKE_DATA_STRUCT(LocalSize),
            },
        };

        Rr_ComputePipelineCreateInfo PipelineCreateInfo = {};
        PipelineCreateInfo.Layout = PipelineLayout;
        PipelineCreateInfo.ShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_BINDINGTEST_COMP_SPV);
        PipelineCreateInfo.SpecializationCount = Specializations.size();
        PipelineCreateInfo.Specializations = Specializations.data();

        ComputePipeline = Rr_CreateComputePipeline(&PipelineCreateInfo);
    }

    void InitBuffers()
    {
        UniformAlignment = Rr_GetUniformAlignment();

        {
            UniformBufferA = Rr_CreateBuffer(
                RR_MEGABYTES(1),
                RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_STAGING_BIT |
                    RR_BUFFER_FLAGS_MAPPED_BIT);

            SS1B1Element *Data =
                (SS1B1Element *)Rr_GetMappedBufferData(UniformBufferA);

            SS1B1Element Element;
            Element.TwoVector = { 2.0f, 2.0f, 2.0f, 2.0f };
            Element.ThousandU32 = 1000;
            for (int Index = 0; Index < 8; ++Index)
            {
                std::memcpy(Data + Index, &Element, sizeof(Element));
            }
        }

        {
            UniformBufferB = Rr_CreateBuffer(
                RR_MEGABYTES(1),
                RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_STAGING_BIT |
                    RR_BUFFER_FLAGS_MAPPED_BIT);

            char *Data = (char *)Rr_GetMappedBufferData(UniformBufferB);

            for (int Index = 0; Index < 4; ++Index)
            {
                SS2B4Element Element;
                Element.Vector =
                    Rr_Vec4{ 1.0f, 1.0f, 1.0f, 1.0f } * float(Index);
                Element.U32 = 1000 * Index;
                std::memcpy(
                    Data + Index * UniformAlignment,
                    &Element,
                    sizeof(Element));
            }

            SS2B4Element Element;
            Element.Vector = Rr_Vec4{};
            Element.U32 = 0;
            std::memcpy(Data + 5 * UniformAlignment, &Element, sizeof(Element));
        }

        {
            StorageBuffer = Rr_CreateBuffer(
                RR_MEGABYTES(1),
                RR_BUFFER_FLAGS_STORAGE_BIT | RR_BUFFER_FLAGS_STAGING_BIT |
                    RR_BUFFER_FLAGS_MAPPED_BIT);
            SS2Element *Data =
                (SS2Element *)Rr_GetMappedBufferData(StorageBuffer);
            SS2Element Element;
            Element.Vector = { 0.5f, 0.5f, 0.5f, 0.5f };
            Element.U32 = 128;
            for (int Index = 0; Index < 8; ++Index)
            {
                std::memcpy(Data + Index, &Element, sizeof(Element));
            }
        }

        {
            ReadonlyStorageBuffer = Rr_CreateBuffer(
                RR_MEGABYTES(1),
                RR_BUFFER_FLAGS_STORAGE_BIT | RR_BUFFER_FLAGS_STAGING_BIT |
                    RR_BUFFER_FLAGS_MAPPED_BIT);
            void *Data = Rr_GetMappedBufferData(ReadonlyStorageBuffer);
            SS0B0Element Element;
            Element.ZeroVector = {};
            Element.OneVector = { 1.0f, 1.0f, 1.0f, 1.0f };
            std::memcpy(Data, &Element, sizeof(Element));
        }
    }

    void InitImages()
    {
        StorageImageA = Rr_CreateImage2D(
            { 256, 256 },
            RR_TEXTURE_FORMAT_R8G8B8A8_UNORM,
            RR_IMAGE_FLAGS_STORAGE_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT);
        StorageImageB = Rr_CreateImage2D(
            { 256, 256 },
            RR_TEXTURE_FORMAT_R8G8B8A8_UNORM,
            RR_IMAGE_FLAGS_STORAGE_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT);
    }

    void Init()
    {
        InitPipeline();
        InitBuffers();
        InitImages();
    }

    void Iterate()
    {
        Rr_GraphNode *ComputeNode =
            Rr_AddComputeNode(Rr_GetGraph(), "compute_a");
        Rr_BindComputePipeline(ComputeNode, ComputePipeline);
        Rr_BindStorageBuffer(
            ComputeNode,
            ReadonlyStorageBuffer,
            0,
            0,
            0,
            sizeof(SS0B0Element));
        Rr_BindUniformBuffer(
            ComputeNode,
            UniformBufferA,
            1,
            1,
            0,
            sizeof(SS1B1Element) * 8);
        Rr_BindStorageBuffer(
            ComputeNode,
            StorageBuffer,
            2,
            2,
            0,
            sizeof(SS2Element) * 8);
        int UniformAlignment = Rr_GetUniformAlignment();
        for (int Index = 0; Index < 4; ++Index)
        {
            Rr_BindUniformBufferAt(
                ComputeNode,
                UniformBufferB,
                2,
                4,
                Index,
                Index * UniformAlignment,
                sizeof(SS2B4Element));
        }
        Rr_BindStorageImage2D(ComputeNode, StorageImageA, 3, 13);
        Rr_Dispatch(ComputeNode, 16, 16, 1);
        Rr_BindUniformBufferAt(
            ComputeNode,
            UniformBufferB,
            2,
            4,
            1,
            4 * UniformAlignment,
            sizeof(SS2B4Element));
        Rr_BindStorageImage2D(ComputeNode, StorageImageB, 3, 13);
        Rr_Dispatch(ComputeNode, 16, 16, 1);

        Rr_AddBlitNode(
            Rr_GetGraph(),
            "blit_a",
            StorageImageA,
            Rr_GetSwapchainImage(),
            { 0, 0, 256, 256 },
            { 0, 0, Rr_GetSwapchainSize().Width, Rr_GetSwapchainSize().Height },
            RR_IMAGE_ASPECT_COLOR_BIT);

        Rr_AddBlitNode(
            Rr_GetGraph(),
            "blit_b",
            StorageImageB,
            Rr_GetSwapchainImage(),
            { 0, 0, 256, 256 },
            { 0,
              0,
              Rr_GetSwapchainSize().Width / 2,
              Rr_GetSwapchainSize().Height / 2 },
            RR_IMAGE_ASPECT_COLOR_BIT);
    }

    void Cleanup()
    {
        Rr_ReleaseComputePipeline(ComputePipeline);
        Rr_ReleasePipelineLayout(PipelineLayout);
        Rr_ReleaseBuffer(UniformBufferA);
        Rr_ReleaseBuffer(UniformBufferB);
        Rr_ReleaseBuffer(StorageBuffer);
        Rr_ReleaseBuffer(ReadonlyStorageBuffer);
        Rr_ReleaseImage(StorageImageA);
        Rr_ReleaseImage(StorageImageB);
    }
};

int main()
{
    static SApp App;

    Rr_AppConfig Config = {};
    Config.Title = "BindingTest";
    Config.InitFunc = []() { App.Init(); };
    Config.IterateFunc = []() { App.Iterate(); };
    Config.CleanupFunc = []() { App.Cleanup(); };
    Rr_Run(&Config);
}
