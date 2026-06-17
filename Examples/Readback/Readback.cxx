#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include <array>
#include <cassert>

struct SReadbackApp
{
    Rr_ComputePipeline *ComputePipeline{};
    Rr_Image2D *ColorImage{};
    Rr_Buffer *ReadbackBuffer{};

    void Init()
    {
        Rr_Asset ComputeShader = Rr_LoadAsset(EXAMPLE_ASSET_READBACK_COMP_SPV);
        Rr_ShaderInfo ShaderInfo = {
            .SPVSize = ComputeShader.Size,
            .SPVData = ComputeShader.Data,
        };

        ComputePipeline = Rr_CreateComputePipeline(&ShaderInfo);

        ColorImage = Rr_CreateImage2D(
            { 256, 256 },
            RR_IMAGE_FORMAT_R8G8B8A8_UNORM,
            RR_IMAGE_FLAGS_STORAGE_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT);

        ReadbackBuffer = Rr_CreateBuffer(256 * 256 * 4, RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_READBACK_BIT);
    }

    void ComputeAndCopyToHost()
    {
        auto Scratch = Rr_GetScratch(nullptr);

        Rr_Graph *SubGraph = Rr_BeginGraph(RR_QUEUE_TYPE_MAIN, Scratch.Arena);

        Rr_GraphNode *ComputeNode = Rr_AddComputeNode(SubGraph);
        Rr_BindComputePipeline(ComputeNode, ComputePipeline);
        Rr_BindStorageImage2DRW(ComputeNode, ColorImage, 0, 0);
        Rr_Dispatch(ComputeNode, 256 / 32, 256 / 32, 1);

        Rr_CopyImage2DToBuffer(
            SubGraph,
            ColorImage,
            { 0, 0 },
            { 256, 256 },
            RR_IMAGE_ASPECT_COLOR_BIT,
            0,
            ReadbackBuffer,
            0);

        Rr_EndGraph(SubGraph);

        Rr_DestroyScratch(Scratch);
    }

    void Iterate()
    {
        ComputeAndCopyToHost();

        Rr_Image2D *SwapchainImage = Rr_GetSwapchainImage();
        Rr_IntVec2 SwapchainExtent = Rr_GetImage2DExtent(SwapchainImage);

        Rr_BlitImage2D(
            Rr_GetGraph(),
            ColorImage,
            Rr_GetSwapchainImage(),
            { 0, 0, 256, 256 },
            { 0, 0, SwapchainExtent.X, SwapchainExtent.Y },
            RR_IMAGE_ASPECT_COLOR_BIT);

        uint8_t *BufferData = reinterpret_cast<uint8_t *>(Rr_GetMappedBufferData(ReadbackBuffer));

        Rr_UIBeginDebugOverlayTabs();
        if (Rr_UIBeginWindow("Readback.cxx"))
        {
            Rr_UIText(
                "This examples shows copying an image into a\n"
                "CPU-visible buffer and reading pixel color under the cursor.");

            Rr_Vec2 MousePosition = Rr_GetMousePosition();
            Rr_Vec2 SwapchainExtentF = { static_cast<float>(SwapchainExtent.Width),
                                         static_cast<float>(SwapchainExtent.Height) };
            Rr_Vec2 MouseNormalized = MousePosition / SwapchainExtentF;
            MouseNormalized = Rr_MinV2(Rr_MaxV2(MouseNormalized, Rr_V2F(0.0f)), SwapchainExtentF);
            Rr_Vec2 PixelCoordF = MouseNormalized * 255.0f;
            Rr_IntVec2 PixelCoord = {
                static_cast<int>(PixelCoordF.X),
                static_cast<int>(PixelCoordF.Y),
            };
            uint8_t *PixelChannels = BufferData + PixelCoord.Y * 256 * 4 + PixelCoord.X * 4;
            Rr_Vec3 PixelColor = {
                static_cast<float>(PixelChannels[0]) / 255.0f,
                static_cast<float>(PixelChannels[1]) / 255.0f,
                static_cast<float>(PixelChannels[2]) / 255.0f,
            };

            Rr_UIInputColor3("Pixel Color", PixelColor.Elements);
        }
        Rr_UIEndWindow();
        Rr_UIEndDebugOverlayTabs();
    }

    void Cleanup()
    {
        Rr_ReleaseComputePipeline(ComputePipeline);
        Rr_ReleaseImage(ColorImage);
        Rr_ReleaseBuffer(ReadbackBuffer);
    }
};

int main()
{
    static SReadbackApp App;

    Rr_Config Config = {
        .WindowTitle = "Readback",
        .InitFunc = []() { App.Init(); },
        .IterateFunc = []() { App.Iterate(); },
        .CleanupFunc = []() { App.Cleanup(); },
    };
    Rr_Run(&Config);
}
