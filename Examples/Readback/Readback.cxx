#include <Rr/Rr.h>

#include <array>
#include <cassert>
#include <iostream>

struct SReadbackApp
{
    Rr_Image2D *ColorImage{};
    Rr_Buffer *ReadbackBuffer{};

    void Init()
    {
        ColorImage = Rr_CreateImage2D(
            { 256, 256 },
            RR_IMAGE_FORMAT_R8G8B8A8_UNORM,
            RR_IMAGE_FLAGS_COLOR_ATTACHMENT_BIT);
        ReadbackBuffer =
            Rr_CreateBuffer(256 * 256 * 4, RR_BUFFER_FLAGS_MAPPED_BIT);
    }

    void Iterate()
    {
        Rr_Graph *Graph = Rr_GetGraph();
        Rr_CopyImage2DToBuffer(
            Graph,
            ColorImage,
            { 0, 0 },
            { 256, 256 },
            RR_IMAGE_ASPECT_COLOR_BIT,
            0,
            ReadbackBuffer,
            0);
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
