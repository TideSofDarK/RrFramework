#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../../Vendor/stb/stb_image.h"

#include <array>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

Rr_Image2D *CreateImage2D(Rr_Graph *Graph, const char *Path)
{
    std::int32_t ImageWidth;
    std::int32_t ImageHeight;
    std::int32_t ImageChannels;
    char *Data =
        (char *)stbi_load(Path, &ImageWidth, &ImageHeight, &ImageChannels, 4);

    std::size_t ImageDataSize = ImageWidth * ImageHeight * 4;

    Rr_Buffer *StagingBuffer = Rr_CreateBuffer(
        ImageDataSize,
        RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_STAGING_BIT);

    std::memcpy(Rr_GetMappedBufferData(StagingBuffer), Data, ImageDataSize);

    stbi_image_free(Data);

    Rr_Image2D *Image2D = Rr_CreateImage2D(
        { ImageWidth, ImageHeight },
        RR_IMAGE_FORMAT_R8G8B8A8_SRGB,
        RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT);

    Rr_CopyBufferToImage2D(
        Graph,
        StagingBuffer,
        0,
        { ImageWidth, ImageHeight },
        Image2D,
        0);

    Rr_ReleaseBuffer(StagingBuffer);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    return Image2D;
}

struct STransferThread
{
    std::mutex PathsMutex;
    std::condition_variable CondVar;
    std::queue<std::string> PathsQueue;

    std::mutex StopMutex;
    std::condition_variable StopCondVar;

    std::mutex ImagesMutex;
    std::queue<Rr_Image2D *> ImagesQueue;

    std::atomic_bool IsBusy;
    std::atomic_bool IsRunning;
    std::atomic_bool StopRequested;

    static void ThreadProc(STransferThread *Thread)
    {
        Thread->IsRunning = true;

        Rr_InitThreadContext();

        while (true)
        {
            Thread->IsBusy = false;

            std::string Path;
            {
                std::unique_lock PathsLock{ Thread->PathsMutex };
                Thread->CondVar.wait(PathsLock, [&]() {
                    return !Thread->PathsQueue.empty() || Thread->StopRequested;
                });
                if (Thread->StopRequested)
                {
                    break;
                }

                Path = std::move(Thread->PathsQueue.front());
                Thread->PathsQueue.pop();
            }

            Thread->IsBusy = true;

            Rr_Graph *Graph = Rr_BeginGraph(RR_GRAPH_FLAGS_TRANSFER_BIT);

            Rr_Image2D *Image2D = CreateImage2D(Graph, Path.c_str());

            if (!Image2D)
            {
                std::cerr << "Unable to create Image2D!\n";
                std::abort();
            }

            Rr_EndGraph(Graph);

            std::unique_lock ImagesLock{ Thread->ImagesMutex };
            Thread->ImagesQueue.push(Image2D);
        }

        Rr_CleanupThreadContext();

        std::unique_lock Lock{ Thread->StopMutex };
        Thread->IsRunning = false;
        Thread->StopCondVar.notify_all();
    }

    void Run()
    {
        auto Thread = std::thread{ ThreadProc, this };
        Thread.detach();
        IsRunning = true;
    }

    void Stop()
    {
        {
            std::unique_lock Lock{ PathsMutex };
            StopRequested = true;
            CondVar.notify_all();
        }

        std::unique_lock Lock{ StopMutex };
        StopCondVar.wait(Lock, [&]() { return !IsRunning; });
    }

    void AddToQueue(const char *Path)
    {
        std::unique_lock Lock{ PathsMutex };
        PathsQueue.push(Path);
        CondVar.notify_all();
    }

    bool AcquireLoadedImages(std::vector<Rr_Image2D *> &OutImages)
    {
        bool Acquired = false;
        std::unique_lock Lock{ ImagesMutex, std::try_to_lock };
        if (Lock.owns_lock())
        {
            while (!ImagesQueue.empty())
            {
                OutImages.push_back(ImagesQueue.front());
                ImagesQueue.pop();
                Acquired = true;
            }
        }
        return Acquired;
    }
};

struct STransferThreadApp
{
    Rr_PipelineLayout *PlaceholderLayout;
    Rr_GraphicsPipeline *PlaceholderPipeline;
    Rr_PipelineLayout *PipelineLayout;
    Rr_GraphicsPipeline *GraphicsPipeline;
    Rr_Sampler *Sampler;
    Rr_Buffer *UniformBuffer;
    STransferThread Thread;
    std::vector<Rr_Image2D *> Images;
    std::int32_t CurrentImageIndex{};

    void InitPipeline()
    {
        std::array Bindings = {
            Rr_Binding{
                .Index = 0,
                .Type = RR_BINDING_TYPE_UNIFORM_BUFFER,
                .Stages = RR_SHADER_STAGE_FRAGMENT_BIT,
            },
            Rr_Binding{
                .Index = 1,
                .Type = RR_BINDING_TYPE_COMBINED_IMAGE_SAMPLER,
                .Stages = RR_SHADER_STAGE_FRAGMENT_BIT,
            },
        };
        std::array Sets = {
            Rr_BindingSet{ Bindings.size(), Bindings.data() },
        };
        PipelineLayout =
            Rr_CreatePipelineLayout((uint32_t)Sets.size(), Sets.data());

        Rr_ColorTargetInfo ColorTarget = {};
        ColorTarget.Format = Rr_GetSwapchainFormat();
        ColorTarget.Blend = Rr_AlphaBlend();

        Rr_Asset VertexShader =
            Rr_LoadAsset(EXAMPLE_ASSET_TRANSFERTHREAD_VERT_SPV);
        Rr_Asset FragmentShader =
            Rr_LoadAsset(EXAMPLE_ASSET_TRANSFERTHREAD_FRAG_SPV);

        Rr_GraphicsPipelineCreateInfo PipelineInfo = {};
        PipelineInfo.Layout = PipelineLayout;
        PipelineInfo.VertexShaderSPVSize = VertexShader.Size;
        PipelineInfo.VertexShaderSPVData = VertexShader.Pointer;
        PipelineInfo.FragmentShaderSPVSize = FragmentShader.Size;
        PipelineInfo.FragmentShaderSPVData = FragmentShader.Pointer;
        PipelineInfo.ColorTargetCount = 1;
        PipelineInfo.ColorTargets = &ColorTarget;

        GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);
    }

    void InitSampler()
    {
        Rr_SamplerInfo SamplerInfo = {};
        SamplerInfo.MagFilter = RR_FILTER_LINEAR;
        SamplerInfo.MinFilter = RR_FILTER_LINEAR;
        SamplerInfo.AddressModeW = RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        Sampler = Rr_CreateSampler(&SamplerInfo);
    }

    void InitUniformBuffer()
    {
        UniformBuffer = Rr_CreateBuffer(
            16,
            RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_UNIFORM_BIT |
                RR_BUFFER_FLAGS_PER_FRAME_BIT | RR_BUFFER_FLAGS_STAGING_BIT);
    }

    void InitPlaceholder()
    {
        std::array Bindings = {
            Rr_Binding{
                .Index = 0,
                .Type = RR_BINDING_TYPE_UNIFORM_BUFFER,
                .Stages = RR_SHADER_STAGE_FRAGMENT_BIT,
            },
        };
        std::array Sets = {
            Rr_BindingSet{ Bindings.size(), Bindings.data() },
        };
        PlaceholderLayout =
            Rr_CreatePipelineLayout((uint32_t)Sets.size(), Sets.data());

        Rr_ColorTargetInfo ColorTarget = {};
        ColorTarget.Format = Rr_GetSwapchainFormat();
        ColorTarget.Blend = Rr_AlphaBlend();

        Rr_Asset VertexShader =
            Rr_LoadAsset(EXAMPLE_ASSET_PLACEHOLDER_VERT_SPV);
        Rr_Asset FragmentShader =
            Rr_LoadAsset(EXAMPLE_ASSET_PLACEHOLDER_FRAG_SPV);

        Rr_GraphicsPipelineCreateInfo PipelineInfo = {};
        PipelineInfo.Layout = PipelineLayout;
        PipelineInfo.VertexShaderSPVSize = VertexShader.Size;
        PipelineInfo.VertexShaderSPVData = VertexShader.Pointer;
        PipelineInfo.FragmentShaderSPVSize = FragmentShader.Size;
        PipelineInfo.FragmentShaderSPVData = FragmentShader.Pointer;
        PipelineInfo.ColorTargetCount = 1;
        PipelineInfo.ColorTargets = &ColorTarget;

        PlaceholderPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);
    }

    void Init()
    {
        InitSampler();
        InitPipeline();
        InitUniformBuffer();
        InitPlaceholder();

        Thread.Run();
    }

    void Event(Rr_Event *Event)
    {
        if (Event->Type == RR_EVENT_TYPE_DROP_FILE)
        {
            Thread.AddToQueue(Event->DropFile.Path);
        }

        if (Event->Type == RR_EVENT_TYPE_QUIT)
        {
            Thread.Stop();
        }
    }

    void Iterate()
    {
        Rr_UIDebugOverlay();
        Rr_UIBeginWindow(
            "TransferThread.cxx",
            nullptr,
            RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT);
        Rr_UILabel(
            "This example demonstrates loading images from another thread.");
        if (Images.size() > 1)
        {
            Rr_UISliderInt(
                "Selected Image",
                &CurrentImageIndex,
                0,
                Images.size() - 1);
        }
        Rr_UIEndWindow();

        if (Thread.AcquireLoadedImages(Images))
        {
            CurrentImageIndex = Images.size() - 1;
        }

        Rr_Image2D *SwapchainImage = Rr_GetSwapchainImage();
        Rr_IntVec2 SwapchainExtent = Rr_GetSwapchainSize();
        Rr_Rect SwapchainRect{
            0,
            0,
            (float)SwapchainExtent.Width,
            (float)SwapchainExtent.Height,
        };

        Rr_ColorTarget ColorTarget = {
            .Image = SwapchainImage,
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
            .Clear = Rr_ColorClear{ 0.0f, 0.0f, 0.0f, 1.0f },
        };

        Rr_GraphNode *GraphicsNode =
            Rr_AddGraphicsNode(Rr_GetGraph(), 1, &ColorTarget, NULL);

        struct
        {
            float Time = (float)Rr_GetTimeSeconds();
            std::uint32_t ImageCount = 3;
        } UniformData;

        std::memcpy(
            Rr_GetMappedBufferData(UniformBuffer),
            &UniformData,
            sizeof(UniformData));

        if (!Images.empty())
        {
            Rr_Image2D *Image2D = Images.at(CurrentImageIndex);

            Rr_IntVec2 ImageSize = Rr_GetImage2DExtent(Image2D);
            Rr_Rect ImageRect{
                0,
                0,
                (float)ImageSize.Width,
                (float)ImageSize.Height,
            };
            Rr_Rect Viewport = Rr_FitRect(&ImageRect, &SwapchainRect);
            Rr_SetViewport(GraphicsNode, &Viewport);
            Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
            Rr_BindUniformBuffer(
                GraphicsNode,
                UniformBuffer,
                0,
                0,
                0,
                sizeof(UniformData));
            Rr_BindCombinedImage2DSampler(GraphicsNode, Image2D, Sampler, 0, 1);
            Rr_Draw(GraphicsNode, 6, 1, 0, 0);
        }

        if (Thread.IsBusy)
        {
            Rr_Rect ImageRect{ 0, 0, 1, 1 };
            Rr_Rect Viewport = Rr_FitRect(&ImageRect, &SwapchainRect);
            Rr_SetViewport(GraphicsNode, &Viewport);
            Rr_BindGraphicsPipeline(GraphicsNode, PlaceholderPipeline);
            Rr_BindUniformBuffer(
                GraphicsNode,
                UniformBuffer,
                0,
                0,
                0,
                sizeof(UniformData));
            Rr_Draw(GraphicsNode, 6, 1, 0, 0);
        }
    }

    void Cleanup()
    {
        Rr_ReleaseGraphicsPipeline(PlaceholderPipeline);
        Rr_ReleasePipelineLayout(PlaceholderLayout);
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
        Rr_ReleasePipelineLayout(PipelineLayout);
        Rr_ReleaseBuffer(UniformBuffer);
        for (auto Image : Images)
        {
            Rr_ReleaseImage(Image);
        }
        Rr_ReleaseSampler(Sampler);
    }
};

int main()
{
    static STransferThreadApp App;

    Rr_AppConfig Config = {};
    Config.Title = "TransferThread";
    Config.InitFunc = []() { App.Init(); };
    Config.EventFunc = [](Rr_Event *Event) { App.Event(Event); };
    Config.IterateFunc = []() { App.Iterate(); };
    Config.CleanupFunc = []() { App.Cleanup(); };
    Rr_Run(&Config);
}
