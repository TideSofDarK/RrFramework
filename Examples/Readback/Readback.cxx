#include <Rr/Rr.h>

#include <cassert>
#include <print>

struct SReadbackApp
{
    Rr_Image2D *ColorImage{};
    Rr_Buffer *ReadbackBuffer{};

    void Init()
    {
        ColorImage = Rr_CreateImage2D(
            { 256, 256 },
            RR_IMAGE_FORMAT_R8G8B8A8_UNORM,
            RR_IMAGE_FLAGS_COLOR_ATTACHMENT_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT);
        ReadbackBuffer = Rr_CreateBuffer(
            256 * 256 * 4,
            RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_READBACK_BIT);
    }

    void Iterate()
    {
        Rr_Graph *SubGraph = Rr_BeginGraph(RR_QUEUE_TYPE_MAIN);

        float Channel = static_cast<float>(Rr_GetTimeSeconds());
        Channel = Channel - static_cast<long>(Channel);

        Rr_ClearColorImage2D(
            SubGraph,
            { .Vec4 = { Channel, Channel, Channel, 1.0f, } },
            ColorImage);
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

        Rr_BlitImage2D(
            Rr_GetGraph(),
            ColorImage,
            Rr_GetSwapchainImage(),
            { 0, 0, 256, 256 },
            { 0, 0, 256, 256 },
            RR_IMAGE_ASPECT_COLOR_BIT);

        uint8_t *BufferData =
            reinterpret_cast<uint8_t *>(Rr_GetMappedBufferData(ReadbackBuffer));

        Rr_UIBeginWindow("Readback.cxx");
        {
            Rr_IntVec4 Value = { BufferData[0],
                                 BufferData[1],
                                 BufferData[2],
                                 BufferData[3] };
            Rr_UIInputInt4("Pixel Value", Value.Elements);
        }
        Rr_UIEndWindow();
    }

    void Cleanup()
    {
        Rr_ReleaseImage(ColorImage);
        Rr_ReleaseBuffer(ReadbackBuffer);
    }
};

int main()
{
    static SReadbackApp App;

    Rr_AppConfig Config = {};
    Config.Title = "Readback";
    Config.InitFunc = []() { App.Init(); };
    Config.IterateFunc = []() { App.Iterate(); };
    Config.CleanupFunc = []() { App.Cleanup(); };
    Rr_Run(&Config);
}
