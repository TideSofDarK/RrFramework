#include <Rr/Rr.h>

#include <stdio.h>

#define MAX_BUFFERS 128
static Rr_Buffer *Buffers[MAX_BUFFERS] = { 0 };
static uint32_t BufferCount = 0;

static void Init(void)
{
}

static void Iterate(void)
{
    Rr_UIDebugOverlay();

    Rr_Graph *Graph = Rr_GetGraph();

    Rr_ClearColorImage2D(
        Graph,
        (Rr_ColorClear){ 0.01f, 0.01f, 0.02f, 1.0f },
        Rr_GetSwapchainImage());

    Rr_UIBeginWindowEx(
        "AllocatorTest.c",
        NULL,
        RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT);
    {
        Rr_UIText("Use debug overlay to track created allocations.");

        static bool Host = true;
        Rr_UICheckbox("Staging", &Host);
        static bool Uniform = false;
        Rr_UICheckbox("Uniform", &Uniform);
        static bool PerFrame = false;
        Rr_UICheckbox("Per Frame", &PerFrame);
        static uint32_t Size = RR_KIBIBYTES(16);
        Rr_UIInputUnsignedInt("Size", &Size);
        if ((Host || Uniform) && BufferCount < MAX_BUFFERS &&
            Rr_UIButton("Create Buffer"))
        {
            Rr_BufferFlags Flags = 0;
            if (Host)
            {
                Flags |= RR_BUFFER_FLAGS_STAGING_BIT;
            }
            if (Uniform)
            {
                Flags |= RR_BUFFER_FLAGS_UNIFORM_BIT;
            }
            if (PerFrame)
            {
                Flags |= RR_BUFFER_FLAGS_PER_FRAME_BIT;
            }
            Buffers[BufferCount++] = Rr_CreateBuffer(Size, Flags);
        }
        Rr_UISeparator();
        if (Rr_UIBeginTree("Buffers"))
        {
            for (uint32_t Index = 0; Index < BufferCount; ++Index)
            {
                if (!Buffers[Index])
                {
                    continue;
                }
                char BufferName[64];
                snprintf(
                    BufferName,
                    64,
                    "Release Buffer (%zuKiB)###%d",
                    Rr_GetBufferSize(Buffers[Index]),
                    Index);
                if (Rr_UIButton(BufferName))
                {
                    Rr_ReleaseBuffer(Buffers[Index]);
                    Buffers[Index] = NULL;
                }
            }

            Rr_UIEndTree();
        }
    }
    Rr_UIEndWindow();
}

static void Cleanup(void)
{
}

int main(int ArgC, char **ArgV)
{
    Rr_Config Config = {
        .WindowTitle = "AllocatorTest",
        .InitFunc = Init,
        .IterateFunc = Iterate,
        .CleanupFunc = Cleanup,
        .WindowFlags = RR_WINDOW_FLAGS_RESIZE_BIT,
    };
    Rr_Run(&Config);

    return 0;
}
