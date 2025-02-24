#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include <algorithm>
#include <cstring>
#include <execution>
#include <numeric>

#include "Camera.hxx"
#include "Input.hxx"

struct SSplatData
{
    struct SGPUSplat
    {
        Rr_Vec4 Position;
        Rr_Quat Quat;
        Rr_Vec4 Scale;
        Rr_Vec4 Color;
    };

    struct SSplat
    {
        Rr_Vec3 Position;
        Rr_Vec3 Scale;
        unsigned char r, g, b, a;
        unsigned char rw, rx, ry, rz;

        Rr_Quat Quat() const
        {
            return Rr_Quat{ ((float)rx - 128.0f) / 128.0f,
                            ((float)ry - 128.0f) / 128.0f,
                            ((float)rz - 128.0f) / 128.0f,
                            ((float)rw - 128.0f) / 128.0f };
        }

        Rr_Vec4 Color() const
        {
            return Rr_Vec4{ (float)r / 255.0f,
                            (float)g / 255.0f,
                            (float)b / 255.0f,
                            (float)a / 255.0f };
        }
    };

    size_t Count{};

    std::vector<SGPUSplat> GPUSplats;
    std::vector<uint32_t> Sorted;
    Rr_Buffer *Buffer{};
    Rr_Buffer *SortedBuffer{};

    Rr_Renderer *Renderer{};

    void Load(Rr_Renderer *Renderer, Rr_AssetRef AssetRef)
    {
        this->Renderer = Renderer;

        Rr_Asset Asset = Rr_LoadAsset(AssetRef);

        Count = Asset.Size / 32;

        GPUSplats.resize(Count);
        Sorted.resize(Count);
        std::iota(Sorted.begin(), Sorted.end(), 0);

        for(size_t Index = 0; Index < Count; ++Index)
        {
            SSplat *Splat = ((SSplat *)Asset.Pointer) + Index;
            SGPUSplat &GPUSplat = GPUSplats[Index];

            GPUSplat.Position.XYZ = Splat->Position;
            GPUSplat.Position.Y *= -1.0f;
            GPUSplat.Scale.XYZ = Splat->Scale;
            GPUSplat.Quat = Rr_NormQ(Splat->Quat());
            GPUSplat.Color = Splat->Color();
        }

        Buffer = Rr_CreateBuffer(
            Renderer,
            sizeof(SGPUSplat) * Count,
            RR_BUFFER_FLAGS_STORAGE_BIT);

        Rr_UploadToDeviceBufferImmediate(
            Renderer,
            Buffer,
            RR_MAKE_DATA(sizeof(SGPUSplat) * Count, GPUSplats.data()));

        SortedBuffer = Rr_CreateBuffer(
            Renderer,
            sizeof(uint32_t) * Count,
            RR_BUFFER_FLAGS_STORAGE_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
                RR_BUFFER_FLAGS_STAGING_INCOHERENT_BIT);
    }

    void Cleanup()
    {
        Rr_DestroyBuffer(Renderer, Buffer);
        Rr_DestroyBuffer(Renderer, SortedBuffer);
    }

    void Render(Rr_App *App, Rr_Vec4 CameraPos, Rr_GraphNode *OffscreenNode)
    {
        std::sort(
            std::execution::par_unseq,
            Sorted.begin(),
            Sorted.end(),
            [&](uint32_t a, uint32_t b) {
                return Rr_LenV3(GPUSplats[b].Position.XYZ - CameraPos.XYZ) <
                       Rr_LenV3(GPUSplats[a].Position.XYZ - CameraPos.XYZ);
            });

        char *Dst = (char *)Rr_MapBuffer(Renderer, SortedBuffer);

        std::memcpy(Dst, Sorted.data(), sizeof(uint32_t) * Count);

        Rr_FlushBufferRange(
            Renderer,
            SortedBuffer,
            0,
            sizeof(uint32_t) * Count);

        Rr_UnmapBuffer(Renderer, SortedBuffer);

        Rr_BindGraphicsStorageBuffer(
            OffscreenNode,
            Buffer,
            0,
            1,
            0,
            sizeof(SGPUSplat) * Count);

        Rr_BindGraphicsStorageBuffer(
            OffscreenNode,
            SortedBuffer,
            0,
            2,
            0,
            sizeof(uint32_t) * Count);

        Rr_Draw(OffscreenNode, 6, Count, 0, 0);
    }
};

struct SUniformData
{
    Rr_Mat4 View;
    Rr_Mat4 Projection;
    Rr_Vec3 HFOVFocal;
};

Rr_LoadThread *LoadThread;
Rr_Image *ColorAttachment;
Rr_Buffer *StagingBuffer;
Rr_Buffer *UniformBuffer;
Rr_PipelineLayout *PipelineLayout;
Rr_GraphicsPipeline *GraphicsPipeline;
Rr_Sampler *LinearSampler;
SCamera Camera;
SSplatData SplatData;

bool Loaded;

Rr_Vec4 BackgroundColor{ 0.0f, 0.0f, 0.0f, 1.0f };

static void OnLoadComplete(Rr_App *App, void *UserData)
{
    Loaded = true;
}

static void Init(Rr_App *App, void *UserData)
{
    Rr_Renderer *Renderer = Rr_GetRenderer(App);

    /* Create simple sampler. */

    Rr_SamplerInfo SamplerInfo = {};
    SamplerInfo.MinFilter = RR_FILTER_LINEAR;
    SamplerInfo.MagFilter = RR_FILTER_LINEAR;
    LinearSampler = Rr_CreateSampler(Renderer, &SamplerInfo);

    /* Create graphics pipeline. */

    Rr_PipelineBinding BindingsA[] = {
        { 0, 1, RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER },
        { 1, 1, RR_PIPELINE_BINDING_TYPE_STORAGE_BUFFER },
        { 2, 1, RR_PIPELINE_BINDING_TYPE_STORAGE_BUFFER },
    };
    Rr_PipelineBindingSet BindingSets[] = {
        {
            RR_ARRAY_COUNT(BindingsA),
            BindingsA,
            RR_SHADER_STAGE_VERTEX_BIT,
        },
    };
    PipelineLayout = Rr_CreatePipelineLayout(
        Renderer,
        RR_ARRAY_COUNT(BindingSets),
        BindingSets);

    Rr_VertexInputBinding VertexInputBinding = {
        RR_VERTEX_INPUT_RATE_VERTEX,
        0,
        nullptr,
    };

    Rr_ColorTargetInfo ColorTargets[1] = {};
    ColorTargets[0].Format = RR_TEXTURE_FORMAT_R8G8B8A8_UNORM;
    ColorTargets[0].Blend.BlendEnable = true;
    ColorTargets[0].Blend.SrcColorBlendFactor = RR_BLEND_FACTOR_SRC_ALPHA;
    ColorTargets[0].Blend.DstColorBlendFactor =
        RR_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    ColorTargets[0].Blend.ColorBlendOp = RR_BLEND_OP_ADD;
    ColorTargets[0].Blend.SrcAlphaBlendFactor = RR_BLEND_FACTOR_SRC_ALPHA;
    ColorTargets[0].Blend.DstAlphaBlendFactor =
        RR_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    ColorTargets[0].Blend.AlphaBlendOp = RR_BLEND_OP_ADD;

    Rr_PipelineInfo PipelineInfo = {};
    PipelineInfo.Layout = PipelineLayout;
    PipelineInfo.VertexShaderSPV = Rr_LoadAsset(EXAMPLE_ASSET_GS_VERT_SPV);
    PipelineInfo.FragmentShaderSPV = Rr_LoadAsset(EXAMPLE_ASSET_GS_FRAG_SPV);
    PipelineInfo.VertexInputBindingCount = 1;
    PipelineInfo.VertexInputBindings = &VertexInputBinding;
    PipelineInfo.ColorTargetCount = 1;
    PipelineInfo.ColorTargets = ColorTargets;

    GraphicsPipeline = Rr_CreateGraphicsPipeline(Renderer, &PipelineInfo);

    /* Create main draw target. */

    ColorAttachment = Rr_CreateImage(
        Renderer,
        { 1600, 1000, 1 },
        RR_TEXTURE_FORMAT_R8G8B8A8_UNORM,
        RR_IMAGE_FLAGS_COLOR_ATTACHMENT_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT |
            RR_IMAGE_FLAGS_SAMPLED_BIT);

    /* Create uniform buffer. */

    UniformBuffer = Rr_CreateBuffer(
        Renderer,
        sizeof(SUniformData),
        RR_BUFFER_FLAGS_UNIFORM_BIT);

    /* Create staging buffer */

    StagingBuffer = Rr_CreateBuffer(
        Renderer,
        RR_MEGABYTES(16),
        RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT |
            RR_BUFFER_FLAGS_PER_FRAME_BIT);

    Camera.SetPerspective(
        70.0f,
        Rr_GetImageExtent2D(ColorAttachment),
        0.1f,
        200.0f);
    Camera.Type = SCamera::EType::FP;
    Camera.Pitch = 290.0f;
    Camera.Yaw = 180.0f;

    SplatData.Load(Renderer, EXAMPLE_ASSET_BICYCLE_SPLAT);
}

static void Render(Rr_App *App, Rr_Image *ColorAttachment)
{
    Rr_Renderer *Renderer = Rr_GetRenderer(App);

    SUniformData UniformData = {};
    UniformData.Projection = Camera.ProjMatrix;
    UniformData.View = Rr_VulkanMatrix() * Camera.ViewMatrix;
    UniformData.HFOVFocal = { Camera.HTanX, Camera.HTanY, Camera.FocalZ };

    char *StagingData = (char *)Rr_GetMappedBufferData(Renderer, StagingBuffer);
    std::memcpy(StagingData, &UniformData, sizeof(UniformData));
    size_t StagingOffset =
        RR_ALIGN_POW2(sizeof(UniformData), Rr_GetUniformAlignment(Renderer));

    Rr_GraphNode *TransferNode =
        Rr_AddTransferNode(App, "upload_uniform_buffer");
    Rr_TransferBufferData(
        App,
        TransferNode,
        sizeof(UniformData),
        StagingBuffer,
        0,
        UniformBuffer,
        0);

    Rr_ColorTarget OffscreenTarget = {
        0,
        RR_LOAD_OP_CLEAR,
        RR_STORE_OP_STORE,
        {
            BackgroundColor,
        },
    };
    Rr_GraphNode *OffscreenNode = Rr_AddGraphicsNode(
        App,
        "offscreen",
        1,
        &OffscreenTarget,
        &ColorAttachment,
        nullptr,
        nullptr);
    Rr_BindGraphicsPipeline(OffscreenNode, GraphicsPipeline);
    Rr_BindGraphicsUniformBuffer(
        OffscreenNode,
        UniformBuffer,
        0,
        0,
        0,
        sizeof(UniformData));

    SplatData.Render(App, UniformData.View[3], OffscreenNode);

    Rr_AddPresentNode(
        App,
        "present",
        ColorAttachment,
        LinearSampler,
        BackgroundColor,
        RR_PRESENT_MODE_FIT);
}

static void Iterate(Rr_App *App, void *UserData)
{
    {
        static Rr_InputState InputState;
        Rr_UpdateInputState(
            InputMappings.size(),
            InputMappings.data(),
            &InputState);

        Rr_KeyStates Keys = InputState.Keys;

        Camera.Update(App, &InputState);
        if(Rr_GetKeyState(Keys, EIA_FULLSCREEN) == RR_KEYSTATE_PRESSED)
        {
            Rr_ToggleFullscreen(App);
        }
    }

    Render(App, ColorAttachment);
}

static void Cleanup(Rr_App *App, void *UserData)
{
    Rr_Renderer *Renderer = Rr_GetRenderer(App);

    // Rr_DestroyLoadThread(App, LoadThread);
    Rr_DestroyImage(Renderer, ColorAttachment);
    SplatData.Cleanup();
    Rr_DestroyBuffer(Renderer, StagingBuffer);
    Rr_DestroyBuffer(Renderer, UniformBuffer);
    Rr_DestroyGraphicsPipeline(Renderer, GraphicsPipeline);
    Rr_DestroyPipelineLayout(Renderer, PipelineLayout);
    Rr_DestroySampler(Renderer, LinearSampler);
}

int main()
{
    Rr_AppConfig Config = {};
    Config.Title = "99_GS";
    Config.Version = "1.0.0";
    Config.Package = "com.rr.examples.99_gs";
    Config.InitFunc = Init;
    Config.CleanupFunc = Cleanup;
    Config.IterateFunc = Iterate;
    Rr_Run(&Config);
}
