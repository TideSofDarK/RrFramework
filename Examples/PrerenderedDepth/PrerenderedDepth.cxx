#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include "tinyexr.h"

#include <array>
#include <print>

using UScancodes = std::array<bool, RR_SCANCODE_COUNT>;

// Rr_Image *Rr_CreateDepthImageFromEXR(Rr_AssetRef AssetRef)
// {
// Rr_Asset Asset = Rr_LoadAsset(AssetRef);

// const char *Error;

// EXRVersion Version;
// int32_t Result = ParseEXRVersionFromMemory(
// &Version,
// (unsigned char *)Asset.Pointer,
// Asset.Size);
// if (Result != 0)
// {
// std::println("Error opening EXR file!");
// }

// EXRHeader Header;
// Result = ParseEXRHeaderFromMemory(
// &Header,
// &Version,
// (unsigned char *)Asset.Pointer,
// Asset.Size,
// &Error);
// if (Result != 0)
// {
// std::println("Error opening EXR file: %s", Error);
// // FreeEXRErrorMessage(Error);
// }

// EXRImage Image;
// InitEXRImage(&Image);

// Result = LoadEXRImageFromMemory(
// &Image,
// &Header,
// (unsigned char *)Asset.Pointer,
// Asset.Size,
// &Error);
// if (Result != 0)
// {
// std::println("Error opening EXR file: %s", Error);
// // FreeEXRHeader(&Header);
// // FreeEXRErrorMessage(Error);
// }

// /* Calculate depth (https://en.wikipedia.org/wiki/Z-buffering) */

// float Near = 0.5f;
// float Far = 50.0f;
// float FarPlusNear = Far + Near;
// float FarMinusNear = Far - Near;
// float FTimesNear = Far * Near;
// for (int32_t Index = 0; Index < Image.width * Image.height; Index++)
// {
// float *Current = (float *)Image.images[0] + Index;
// float ZReciprocal = 1.0f / *Current;
// float Depth = FarPlusNear / FarMinusNear +
// ZReciprocal * ((-2.0f * FTimesNear) / (FarMinusNear));
// Depth = (Depth + 1.0f) / 2.0f;
// if (Depth > 1.0f)
// {
// Depth = 1.0f;
// }
// *Current = Depth;
// }

// Rr_IntVec3 Extent = {
// .Width = Image.width,
// .Height = Image.height,
// .Depth = 1,
// };

// // size_t DataSize = Extent.Width * Extent.Height * sizeof(float);

// // Rr_Image *DepthImage = Rr_CreateImage(
// //     App,
// //     Extent,
// //     RR_TEXTURE_FORMAT_D32_SFLOAT,
// //     RR_IMAGE_USAGE_SAMPLED | RR_IMAGE_USAGE_TRANSFER,
// //     false);

// // Rr_UploadImage(
// //     App,
// //     UploadContext,
// //     DepthImage->AllocatedImages[0].Handle,
// //     Rr_GetVulkanExtent3D(&Extent),
// //     VK_IMAGE_ASPECT_DEPTH_BIT,
// //     VK_PIPELINE_STAGE_TRANSFER_BIT,
// //     VK_ACCESS_TRANSFER_READ_BIT,
// //     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
// //     Image.images[0],
// //     DataSize);

// FreeEXRHeader(&Header);
// FreeEXRImage(&Image);

// // return DepthImage;
// return nullptr;
// }

struct SGPUUniform
{
    Rr_Mat4 View;
    Rr_Mat4 Projection;
    float Near;
    float Far;
};

struct SPrerenderedDepth
{
    Rr_Renderer *Renderer;

    static const Rr_TextureFormat DEPTH_FORMAT = RR_TEXTURE_FORMAT_D32_SFLOAT;

    Rr_PipelineLayout *PipelineLayout;
    Rr_GraphicsPipeline *GraphicsPipeline;

    Rr_Buffer *UniformBuffer;

    Rr_Image2D *BackgroundColorImage;
    Rr_Image2D *BackgroundDepthImage;

    UScancodes Scancodes{};

    SGPUUniform Uniform;

    void InitPipeline()
    {
        std::array Bindings = {
            Rr_PipelineBinding{ 0, 1, RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER },
        };
        std::array Sets = {
            Rr_PipelineBindingSet{
                Bindings.size(),
                Bindings.data(),
                RR_SHADER_STAGE_VERTEX_BIT | RR_SHADER_STAGE_FRAGMENT_BIT,
            },
        };
        PipelineLayout = Rr_CreatePipelineLayout(
            Renderer,
            (uint32_t)Sets.size(),
            Sets.data());

        Rr_ColorTargetInfo ColorTarget = {};
        ColorTarget.Format = Rr_GetSwapchainFormat(Renderer);
        ColorTarget.Blend = Rr_AlphaBlend();

        Rr_GraphicsPipelineCreateInfo PipelineInfo = {};
        PipelineInfo.Layout = PipelineLayout;
        PipelineInfo.VertexShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_PRERENDEREDDEPTH_VERT_SPV);
        PipelineInfo.FragmentShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_PRERENDEREDDEPTH_FRAG_SPV);
        PipelineInfo.ColorTargetCount = 1;
        PipelineInfo.ColorTargets = &ColorTarget;
        PipelineInfo.DepthStencil.EnableDepthTest = true;
        PipelineInfo.DepthStencil.EnableDepthWrite = true;
        PipelineInfo.DepthStencil.CompareOp = RR_COMPARE_OP_LESS;
        PipelineInfo.DepthStencil.Format = DEPTH_FORMAT;

        GraphicsPipeline = Rr_CreateGraphicsPipeline(Renderer, &PipelineInfo);
    }

    void InitBackground()
    {
        // Create Rr_LoadingThread
        // Add custom loading task: a callback and a payload (ID?)
        // Submit task(s)
        // The thread calls the callback and passes Rr_UploadContext
        // From here it should be possible to create buffers/images
        // And upload data to them
        // After processing all of the tasks notify the user with OnComplete
        // callback

        // Rr_UploadContext shouldn't be exclusive to threaded loading
        // Options:
        // 1) Rr_LoadImmediate
        //    This will require the same setup as above meaning callbacks etc
        // 2) Extend Rr_BeginImmediate
        //    This function could return Rr_UploadContext
        //    As such no callbacks needed

        // Currently Rr_Load.c implements PNG and GLTF loading
        // by using functions that take Rr_UploadContext
        // These function should become public since Rr_UploadContext
        // is being exposed to the user

        // Let's do this !!
    }

    void InitUniform(float Aspect)
    {
        UniformBuffer = Rr_CreateBuffer(
            Renderer,
            sizeof(Uniform),
            RR_BUFFER_FLAGS_UNIFORM_BIT);

        Uniform.Projection =
            Rr_Perspective_LH_ZO(RR_ANGLE_DEG(43.7927f), Aspect, 0.5f, 50.0f);
        Uniform.View = Rr_EulerXYZ({ 90.0f - 63.5593f, -46.6919f, 0.0f }) *
                       Rr_Translate({ -7.35889f, -4.0f, -6.92579f });

        Rr_UploadToDeviceBufferImmediate(
            Rr_GetRenderer(),
            UniformBuffer,
            RR_MAKE_DATA_STRUCT(Uniform));
    }

    void InitDepthImage()
    {
        // Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize(Renderer);
        // DepthImage = Rr_CreateImage(
        //     Renderer,
        //     { SwapchainSize.X, SwapchainSize.Y, 1 },
        //     DEPTH_FORMAT,
        //     RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    SPrerenderedDepth()
        : Renderer(Rr_GetRenderer())
    {
        InitPipeline();
        InitBackground();
        InitUniform(0.0f);
    }

    void Iterate()
    {
        // Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize(Renderer);

        // Camera.Aspect = (float)SwapchainSize.X / (float)SwapchainSize.Y;
        // Camera.UpdatePerspective();
        // Camera.Update(Scancodes);

        // SGPUUniform Uniform = {
        //     .View = Camera.ViewMatrix,
        //     .Projection = Camera.ProjMatrix,
        //     .Near = Camera.Near,
        //     .Far = Camera.Far,
        //     .GridSmall = 1.0f,
        //     .GridBig = 10.0f,
        // };
        // std::memcpy(
        //     Rr_GetMappedBufferData(Renderer, UniformBuffer),
        //     &Uniform,
        //     sizeof(SGPUUniform));

        // Rr_Image *SwapchainImage = Rr_GetSwapchainImage(Renderer);

        // Rr_ColorClear ColorClear = {};
        // ColorClear.Vec4 = { 13.0f / 255.0f,
        //                     14.0f / 255.0f,
        //                     28.0f / 255.0f,
        //                     1.0f };
        // Rr_ColorTarget ColorTarget = {
        //     .Slot = 0,
        //     .LoadOp = RR_LOAD_OP_CLEAR,
        //     .StoreOp = RR_STORE_OP_STORE,
        //     .Clear = ColorClear,
        // };
        // Rr_DepthTarget DepthTarget = {
        //     .LoadOp = RR_LOAD_OP_CLEAR,
        //     .StoreOp = RR_STORE_OP_STORE,
        //     .Clear = Rr_DepthClear(1.0f, 0),
        // };
        // Rr_GraphNode *GraphicsNode = Rr_AddGraphicsNode(
        //     Renderer,
        //     "grid",
        //     1,
        //     &ColorTarget,
        //     &SwapchainImage,
        //     &DepthTarget,
        //     DepthImage);
        // Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        // Rr_BindUniformBuffer(
        //     GraphicsNode,
        //     UniformBuffer,
        //     0,
        //     0,
        //     0,
        //     sizeof(SGPUUniform));
        // Rr_Draw(GraphicsNode, 6, 1, 0, 0);

        Rr_UIDebugOverlay();
    }

    ~SPrerenderedDepth()
    {
        // Rr_DestroyGraphicsPipeline(Renderer, GraphicsPipeline);
        // Rr_DestroyPipelineLayout(Renderer, PipelineLayout);
        // Rr_DestroyBuffer(Renderer, UniformBuffer);
        // Rr_DestroyImage(Renderer, DepthImage);
    }
};

static void Init(void *UserData)
{
    new (UserData) SPrerenderedDepth();
}

static void Iterate(void *UserData)
{
    auto SmoothGrid = std::bit_cast<SPrerenderedDepth *>(UserData);
    SmoothGrid->Iterate();
}

static void Cleanup(void *UserData)
{
    auto SmoothGrid = std::bit_cast<SPrerenderedDepth *>(UserData);
    SmoothGrid->~SPrerenderedDepth();
}

int main()
{
    alignas(SPrerenderedDepth) std::array<std::byte, sizeof(SPrerenderedDepth)>
        SmoothGrid;
    Rr_AppConfig Config = {};
    Config.Title = "PrerenderedDepth";
    Config.Version = "1.0.0";
    Config.Package = "com.rr.examples.prerendereddepth";
    Config.InitFunc = Init;
    Config.IterateFunc = Iterate;
    Config.CleanupFunc = Cleanup;
    Config.UserData = SmoothGrid.data();
    Rr_Run(&Config);
}
