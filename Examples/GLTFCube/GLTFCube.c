#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#define CGLTF_IMPLEMENTATION
#include <cgltf/cgltf.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <stdio.h>
#include <string.h>

typedef struct SUniformData SUniformData;
struct SUniformData
{
    Rr_Mat4 Model;
    Rr_Mat4 View;
    Rr_Mat4 Projection;
    float Time;
};

static Rr_Image2D *DepthAttachment;
static Rr_Buffer *UniformBuffer;
static Rr_GraphicsPipeline *GraphicsPipeline;
static Rr_Sampler *Sampler;

static Rr_Image2D *PrimitiveTexture;
static Rr_Buffer *PrimitiveBuffer;
static size_t PrimitiveIndexOffset;
static size_t PrimitiveIndexCount;

static SUniformData UniformData;

static void InitGLTFPrimitive(void)
{
    Rr_Asset LoadedAsset = Rr_LoadAsset(EXAMPLE_ASSET_CUBE_GLB);

    cgltf_options Options = { 0 };
    cgltf_data *Data = NULL;
    cgltf_result Result =
        cgltf_parse(&Options, LoadedAsset.Pointer, LoadedAsset.Size, &Data);
    if (Result != cgltf_result_success)
    {
        fprintf(stderr, "Failed to load GLTF data!");

        exit(1);
    }
    cgltf_load_buffers(&Options, Data, NULL);

    typedef struct
    {
        Rr_Vec3 Position;
        Rr_Vec2 UV;
        Rr_Vec3 Normal;
    } SVertex;

    cgltf_mesh *Mesh = Data->meshes;
    cgltf_primitive *Primitive = Mesh->primitives;

    if (Primitive->material &&
        Primitive->material->has_pbr_metallic_roughness &&
        Primitive->material->pbr_metallic_roughness.base_color_texture.texture)
    {
        cgltf_texture *Texture = Primitive->material->pbr_metallic_roughness
                                     .base_color_texture.texture;
        if (strcmp(Texture->image->mime_type, "image/png") == 0 ||
            strcmp(Texture->image->mime_type, "image/jpeg") == 0)
        {
            size_t ImageDataSize = (size_t)Texture->image->buffer_view->size;
            stbi_uc const *ImageData =
                (stbi_uc const *)Texture->image->buffer_view->buffer->data +
                Texture->image->buffer_view->offset;

            int32_t ImageWidth;
            int32_t ImageHeight;
            int32_t ImageChannels;
            char *Data = (char *)stbi_load_from_memory(
                ImageData,
                ImageDataSize,
                &ImageWidth,
                &ImageHeight,
                &ImageChannels,
                4);
            size_t DataSize = sizeof(uint32_t) * ImageWidth * ImageHeight;

            Rr_Buffer *StagingBuffer = Rr_CreateBuffer(
                DataSize,
                RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_STAGING_BIT);
            Rr_ReleaseBuffer(StagingBuffer);
            memcpy(Rr_GetMappedBufferData(StagingBuffer), Data, DataSize);

            stbi_image_free(Data);

            PrimitiveTexture = Rr_CreateImage2D(
                Rr_IntV2(ImageWidth, ImageHeight),
                RR_IMAGE_FORMAT_R8G8B8A8_SRGB,
                RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT);
            Rr_CopyBufferToImage2D(
                Rr_GetGraph(),
                StagingBuffer,
                0,
                Rr_IntV2(ImageWidth, ImageHeight),
                PrimitiveTexture,
                0);
        }
    }

    cgltf_accessor const *PositionAccessor =
        cgltf_find_accessor(Primitive, cgltf_attribute_type_position, 0);
    cgltf_accessor const *UVAccessor =
        cgltf_find_accessor(Primitive, cgltf_attribute_type_texcoord, 0);
    cgltf_accessor const *NormalAccessor =
        cgltf_find_accessor(Primitive, cgltf_attribute_type_normal, 0);
    cgltf_accessor const *IndexAccessor = Primitive->indices;
    if (cgltf_component_size(IndexAccessor->component_type) != sizeof(uint16_t))
    {
        fprintf(stderr, "Only 16-bit indices are supported!");

        exit(1);
    }

    size_t VertexDataSize = PositionAccessor->count * sizeof(SVertex);
    size_t IndexDataSize = IndexAccessor->count *
                           cgltf_component_size(IndexAccessor->component_type);
    size_t TotalSize = VertexDataSize + IndexDataSize;

    Rr_Buffer *StagingBuffer = Rr_CreateBuffer(
        TotalSize,
        RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT);
    Rr_ReleaseBuffer(StagingBuffer);
    char *StagingData = Rr_GetMappedBufferData(StagingBuffer);
    SVertex *StagingVertices = (SVertex *)StagingData;
    uint16_t *StagingIndices = (uint16_t *)(StagingData + VertexDataSize);

    for (size_t VertexIndex = 0; VertexIndex < PositionAccessor->count;
         ++VertexIndex)
    {
        SVertex Vertex = { 0 };
        cgltf_accessor_read_float(
            PositionAccessor,
            VertexIndex,
            Vertex.Position.Elements,
            3);
        cgltf_accessor_read_float(
            UVAccessor,
            VertexIndex,
            Vertex.UV.Elements,
            2);
        cgltf_accessor_read_float(
            NormalAccessor,
            VertexIndex,
            Vertex.Normal.Elements,
            3);
        StagingVertices[VertexIndex] = Vertex;
    }
    cgltf_accessor_unpack_indices(
        IndexAccessor,
        StagingIndices,
        sizeof(uint16_t),
        IndexAccessor->count);

    PrimitiveIndexCount = IndexAccessor->count;
    PrimitiveIndexOffset = VertexDataSize;
    PrimitiveBuffer = Rr_CreateBuffer(
        TotalSize,
        RR_BUFFER_FLAGS_INDEX_BIT | RR_BUFFER_FLAGS_VERTEX_BIT);
    Rr_TransferNode *Node = Rr_AddTransferNode(Rr_GetGraph());
    Rr_TransferBufferData(
        Node,
        TotalSize,
        StagingBuffer,
        0,
        PrimitiveBuffer,
        0);

    cgltf_free(Data);
}

static void InitDepthImage(void)
{
    Rr_IntVec2 SwapchainSize = Rr_GetImage2DExtent(Rr_GetSwapchainImage());

    if (DepthAttachment != NULL)
    {
        Rr_IntVec2 DepthImageSize = Rr_GetImage2DExtent(DepthAttachment);

        if (DepthImageSize.X >= SwapchainSize.X &&
            DepthImageSize.Y >= SwapchainSize.Y)
        {
            return;
        }

        Rr_ReleaseImage(DepthAttachment);
    }

    DepthAttachment = Rr_CreateImage2D(
        (Rr_IntVec2){ SwapchainSize.Width, SwapchainSize.Height },
        RR_IMAGE_FORMAT_D32_SFLOAT,
        RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT |
            RR_IMAGE_FLAGS_TRANSFER_BIT);
}

static void Init(void)
{
    Rr_SamplerInfo SamplerInfo = { 0 };
    SamplerInfo.MinFilter = RR_FILTER_LINEAR;
    SamplerInfo.MagFilter = RR_FILTER_LINEAR;
    Sampler = Rr_CreateSampler(&SamplerInfo);

    Rr_VertexInputAttribute VertexAttributes[] = {
        { .Location = 0, .Format = RR_FORMAT_VEC3 },
        { .Location = 1, .Format = RR_FORMAT_VEC2 },
        { .Location = 2, .Format = RR_FORMAT_VEC3 },
    };

    Rr_VertexInputBinding VertexInputBindings[] = {
        {
            .Rate = RR_VERTEX_INPUT_RATE_VERTEX,
            .AttributeCount = RR_ARRAY_COUNT(VertexAttributes),
            .Attributes = VertexAttributes,
        },
    };

    Rr_ColorTargetInfo ColorTargets[1] = {
        {
            .Format = Rr_GetImageFormat(Rr_GetSwapchainImage()),
        },
    };

    Rr_Asset VertexShader = Rr_LoadAsset(EXAMPLE_ASSET_GLTFCUBE_VERT_SPV);
    Rr_ShaderInfo VertexShaderInfo = {
        .SPVSize = VertexShader.Size,
        .SPVData = VertexShader.Pointer,
    };

    Rr_Asset FragmentShader = Rr_LoadAsset(EXAMPLE_ASSET_GLTFCUBE_FRAG_SPV);
    Rr_ShaderInfo FragmentShaderInfo = {
        .SPVSize = FragmentShader.Size,
        .SPVData = FragmentShader.Pointer,
    };

    Rr_GraphicsPipelineCreateInfo PipelineInfo = {
        .VertexShaderInfo = &VertexShaderInfo,
        .FragmentShaderInfo = &FragmentShaderInfo,
        .VertexInputBindingCount = RR_ARRAY_COUNT(VertexInputBindings),
        .VertexInputBindings = VertexInputBindings,
        .ColorTargetCount = RR_ARRAY_COUNT(ColorTargets),
        .ColorTargets = ColorTargets,
        .DepthStencil.Format = RR_IMAGE_FORMAT_D32_SFLOAT,
        .DepthStencil.EnableDepthTest = true,
        .DepthStencil.EnableDepthWrite = true,
        .DepthStencil.CompareOp = RR_COMPARE_OP_LESS,
        .Rasterizer.FrontFace = RR_FRONT_FACE_CLOCKWISE,
        .Rasterizer.CullMode = RR_CULL_MODE_BACK,
    };
    GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

    UniformBuffer = Rr_CreateBuffer(
        sizeof(UniformData),
        RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_STAGING_BIT |
            RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT);

    UniformData.Model = Rr_M4D(1.0f);
    UniformData.View = Rr_LookAt_RH(
        Rr_V3(0.0f, 0.0f, -5.0f),
        Rr_V3F(0.0f),
        Rr_V3(0.0f, 1.0f, 0.0f));

    InitGLTFPrimitive();

    InitDepthImage();
}

static void Event(Rr_Event const *Event)
{
    switch (Event->Type)
    {
        case RR_EVENT_TYPE_SWAPCHAIN_CREATED:
        {
            InitDepthImage();
        }
        default:
            return;
    }
}

static void Iterate(void)
{
    Rr_UIBeginWindowEx("GLTFCube.c", NULL, RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT);
    Rr_UIText("This example demonstrates using cGLTF to load and draw meshes.");
    Rr_UIEndWindow();

    Rr_Image2D *SwapchainImage = Rr_GetSwapchainImage();
    Rr_IntVec2 SwapchainSize = Rr_GetImage2DExtent(SwapchainImage);

    UniformData.Projection = Rr_Perspective_RH(
        0.7643276f,
        SwapchainSize.Width / (float)SwapchainSize.Height,
        0.5f,
        50.0f);
    UniformData.Model = Rr_MulM4(
        Rr_Rotate_RH(0.005f, (Rr_Vec3){ 0.0f, 1.0f, 0.0f }),
        UniformData.Model);
    UniformData.Time = (float)Rr_GetTimeSeconds();
    memcpy(
        Rr_GetMappedBufferData(UniformBuffer),
        &UniformData,
        sizeof(UniformData));

    Rr_ColorTarget ColorTarget = {
        .Image = SwapchainImage,
        .LoadOp = RR_LOAD_OP_CLEAR,
        .StoreOp = RR_STORE_OP_STORE,
        .Clear = (Rr_ColorClear){ { 0.03f, 0.03f, 0.04f, 1.0f } },
    };
    Rr_DepthTarget DepthTarget = {
        .LoadOp = RR_LOAD_OP_CLEAR,
        .StoreOp = RR_STORE_OP_STORE,
        .Clear = {
            .Depth = 1.0f,
        },
        .Image = DepthAttachment,
    };
    Rr_GraphNode *GraphicsNode =
        Rr_AddGraphicsNode(Rr_GetGraph(), 1, &ColorTarget, &DepthTarget);
    Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
    Rr_BindVertexBuffer(GraphicsNode, PrimitiveBuffer, 0, 0);
    Rr_BindIndexBuffer(
        GraphicsNode,
        PrimitiveBuffer,
        0,
        PrimitiveIndexOffset,
        RR_INDEX_TYPE_UINT16);
    Rr_BindUniformBuffer(
        GraphicsNode,
        UniformBuffer,
        0,
        0,
        0,
        sizeof(UniformData));
    Rr_BindSampler(GraphicsNode, Sampler, 0, 1);
    Rr_BindSampledImage2D(GraphicsNode, PrimitiveTexture, 0, 2);
    Rr_DrawIndexed(GraphicsNode, PrimitiveIndexCount, 1, 0, 0, 0);
}

static void Cleanup(void)
{
    Rr_ReleaseBuffer(PrimitiveBuffer);
    Rr_ReleaseImage(PrimitiveTexture);
    Rr_ReleaseImage(DepthAttachment);
    Rr_ReleaseBuffer(UniformBuffer);
    Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
    Rr_ReleaseSampler(Sampler);
}

int main(int ArgC, char **ArgV)
{
    Rr_AppConfig Config = {
        .Title = "GLTFCube",
        .WindowFlags = RR_WINDOW_FLAGS_RESIZE_BIT,
        .InitFunc = Init,
        .EventFunc = Event,
        .CleanupFunc = Cleanup,
        .IterateFunc = Iterate,
    };
    Rr_Run(&Config);

    return 0;
}
