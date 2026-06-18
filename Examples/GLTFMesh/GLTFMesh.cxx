#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#define CGLTF_IMPLEMENTATION
#include "../../Vendor/cgltf/cgltf.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../../Vendor/stb/stb_image.h"

#include <array>
#include <cstring>

class COrbitCamera
{
    float FieldOfView{ RR_ANGLE_DEG(70) };
    float Pitch{};
    float Yaw{};
    float Distance{ 10.0f };
    Rr_Vec3 Center{};
    Rr_Mat4 Transform{ Rr_M4D(1.0f) };
    Rr_Mat4 ProjMatrix{ Rr_M4D(1.0f) };

public:
    Rr_Mat4 GetProjectionMatrix() const
    {
        return ProjMatrix;
    }

    Rr_Mat4 GetViewMatrix() const
    {
        return Rr_InvGeneral(Transform);
    }

    Rr_Vec3 GetForwardVector() const
    {
        return Rr_Norm(Transform.Columns[2].XYZ);
    }

    Rr_Vec3 GetRightVector() const
    {
        return Rr_Norm(Transform.Columns[0].XYZ);
    }

    void UpdatePerspective(float Aspect)
    {
        ProjMatrix = Rr_Perspective_RH(FieldOfView, Aspect, 0.1f, 100.0f);
        ProjMatrix.Elements[1][1] *= -1.0f;
    }

    void Update()
    {
        auto MouseDelta = Rr_GetMousePositionDelta();
        auto MouseState = Rr_GetMouseState();
        auto LeftButton = MouseState & RR_MOUSE_BUTTON_LEFT_BIT;
        auto RightButton = MouseState & RR_MOUSE_BUTTON_RIGHT_BIT;

        if (LeftButton || RightButton)
        {
            if (RightButton)
            {
                Rr_SetRelativeMouseMode(true);

                auto constexpr SENSITIVITY = 0.05f;
                Distance += MouseDelta.Y * SENSITIVITY;
            }
            if (LeftButton)
            {
                Rr_SetRelativeMouseMode(true);

                if (Rr_IsScancodePressed(RR_SCANCODE_LSHIFT))
                {
                    auto constexpr SENSITIVITY = 0.0125f;
                    auto ForwardVector = GetForwardVector();
                    auto RightVector = GetRightVector();
                    auto UpVector = Rr_Cross(ForwardVector, RightVector);
                    Center += UpVector * MouseDelta.Y * SENSITIVITY;
                    Center -= RightVector * MouseDelta.X * SENSITIVITY;
                }
                else
                {
                    auto constexpr SENSITIVITY = 0.005f;
                    Yaw -= MouseDelta.X * SENSITIVITY;
                    Pitch -= MouseDelta.Y * SENSITIVITY;
                }
            }
        }
        else
        {
            Rr_SetRelativeMouseMode(false);
        }

        Yaw = Rr_WrapMax(Yaw, RR_PI32 * 2.0f);
        Pitch = RR_CLAMP(RR_PI32 * -0.5f, Pitch, RR_PI32 * 0.5f);

        Transform = Rr_TranslateV(Center) * Rr_Rotate_RH(Yaw, Rr_V3(0.0f, 1.0f, 0.0f)) *
                    Rr_Rotate_RH(Pitch, Rr_V3(1.0f, 0.0f, 0.0f)) * Rr_Translate(0.0f, 0.0f, Distance);
    }
};

typedef struct
{
    Rr_Vec3 Position;
    Rr_Vec2 UV;
    Rr_Vec3 Normal;
} SVertex;

struct SGPUUniform
{
    Rr_Mat4 Model;
    Rr_Mat4 View;
    Rr_Mat4 Projection;
    float Time;
};

class CGLTFMeshApp
{
    Rr_Image2D *DepthImage{};
    Rr_Buffer *UniformBuffer{};
    Rr_GraphicsPipeline *GraphicsPipeline{};
    Rr_Sampler *Sampler{};
    Rr_Image2D *PrimitiveTexture{};
    Rr_Buffer *PrimitiveBuffer{};
    size_t PrimitiveIndexOffset{};
    size_t PrimitiveIndexCount{};
    SGPUUniform GPUUniform{};
    COrbitCamera Camera;

    void InitGLTFPrimitive(void)
    {
        Rr_Asset LoadedAsset = Rr_LoadAsset(EXAMPLE_ASSET_CUBE_GLB);

        auto Options = cgltf_options{};
        cgltf_data *Data = nullptr;
        auto Result = cgltf_parse(&Options, LoadedAsset.Data, LoadedAsset.Size, &Data);
        assert(Result == cgltf_result_success);
        cgltf_load_buffers(&Options, Data, nullptr);

        assert(Data->scene);
        assert(Data->meshes);

        auto Mesh = Data->meshes;
        auto Primitive = Mesh->primitives;

        if (Primitive->material && Primitive->material->has_pbr_metallic_roughness &&
            Primitive->material->pbr_metallic_roughness.base_color_texture.texture)
        {
            auto Texture = Primitive->material->pbr_metallic_roughness.base_color_texture.texture;

            if (std::strcmp(Texture->image->mime_type, "image/png") != 0 &&
                std::strcmp(Texture->image->mime_type, "image/jpeg") != 0)
            {
                assert(false);
            }

            int ImageWidth, ImageHeight, ImageChannels;
            auto ImageData = (std::byte *)stbi_load_from_memory(
                (stbi_uc const *)Texture->image->buffer_view->buffer->data + Texture->image->buffer_view->offset,
                (int)Texture->image->buffer_view->size,
                &ImageWidth,
                &ImageHeight,
                &ImageChannels,
                4);
            assert(ImageData);

            auto ImageDataSize = ImageWidth * ImageHeight * ImageChannels;
            auto StagingBuffer = Rr_CreateBuffer(ImageDataSize, RR_BUFFER_FLAGS_STAGING);
            Rr_ReleaseBuffer(StagingBuffer);
            std::memcpy(Rr_GetMappedBufferData(StagingBuffer), ImageData, ImageDataSize);
            stbi_image_free(ImageData);

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

        auto PositionAccessor = cgltf_find_accessor(Primitive, cgltf_attribute_type_position, 0);
        auto UVAccessor = cgltf_find_accessor(Primitive, cgltf_attribute_type_texcoord, 0);
        auto NormalAccessor = cgltf_find_accessor(Primitive, cgltf_attribute_type_normal, 0);
        auto IndexAccessor = Primitive->indices;
        assert(cgltf_component_size(IndexAccessor->component_type) == sizeof(uint16_t));

        auto VertexDataSize = PositionAccessor->count * sizeof(SVertex);
        auto IndexDataSize = IndexAccessor->count * cgltf_component_size(IndexAccessor->component_type);
        auto TotalSize = VertexDataSize + IndexDataSize;

        auto StagingBuffer = Rr_CreateBuffer(TotalSize, RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT);
        Rr_ReleaseBuffer(StagingBuffer);
        auto StagingData = (std::byte *)Rr_GetMappedBufferData(StagingBuffer);
        auto StagingVertices = (SVertex *)StagingData;
        auto StagingIndices = (uint16_t *)(StagingData + VertexDataSize);

        for (auto VertexIndex = 0; VertexIndex < PositionAccessor->count; ++VertexIndex)
        {
            auto Vertex = SVertex{};
            cgltf_accessor_read_float(PositionAccessor, VertexIndex, Vertex.Position.Elements, 3);
            cgltf_accessor_read_float(UVAccessor, VertexIndex, Vertex.UV.Elements, 2);
            cgltf_accessor_read_float(NormalAccessor, VertexIndex, Vertex.Normal.Elements, 3);
            StagingVertices[VertexIndex] = Vertex;
        }
        cgltf_accessor_unpack_indices(IndexAccessor, StagingIndices, sizeof(uint16_t), IndexAccessor->count);

        PrimitiveIndexCount = IndexAccessor->count;
        PrimitiveIndexOffset = VertexDataSize;
        PrimitiveBuffer = Rr_CreateBuffer(TotalSize, RR_BUFFER_FLAGS_INDEX_BIT | RR_BUFFER_FLAGS_VERTEX_BIT);
        auto TransferNode = Rr_AddTransferNode(Rr_GetGraph());
        Rr_TransferBufferData(TransferNode, TotalSize, StagingBuffer, 0, PrimitiveBuffer, 0);

        cgltf_free(Data);
    }

    void InitDepthImage(void)
    {
        Rr_IntVec2 SwapchainSize = Rr_GetImage2DExtent(Rr_GetSwapchainImage());

        if (DepthImage != nullptr)
        {
            Rr_IntVec2 DepthImageSize = Rr_GetImage2DExtent(DepthImage);

            if (DepthImageSize.X >= SwapchainSize.X && DepthImageSize.Y >= SwapchainSize.Y)
            {
                return;
            }

            Rr_ReleaseImage(DepthImage);
        }

        DepthImage = Rr_CreateImage2D(
            (Rr_IntVec2){ SwapchainSize.Width, SwapchainSize.Height },
            RR_IMAGE_FORMAT_D32_SFLOAT,
            RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT);
    }

public:
    CGLTFMeshApp()
    {
        auto SamplerInfo = Rr_SamplerInfo{
            .MagFilter = RR_FILTER_LINEAR,
            .MinFilter = RR_FILTER_LINEAR,
        };
        Sampler = Rr_CreateSampler(&SamplerInfo);

        std::array VertexAttributes = {
            Rr_VertexInputAttribute{ .Location = 0, .Format = RR_FORMAT_FLOAT3 },
            Rr_VertexInputAttribute{ .Location = 1, .Format = RR_FORMAT_FLOAT2 },
            Rr_VertexInputAttribute{ .Location = 2, .Format = RR_FORMAT_FLOAT3 },
        };

        std::array VertexInputBindings = {
            Rr_VertexInputBinding{
                .Rate = RR_VERTEX_INPUT_RATE_VERTEX,
                .AttributeCount = VertexAttributes.size(),
                .Attributes = VertexAttributes.data(),
            },
        };

        auto ColorTargets = std::array{
            Rr_ColorTargetInfo{
                .Format = Rr_GetImageFormat(Rr_GetSwapchainImage()),
            },
        };

        auto VertexShader = Rr_LoadAsset(EXAMPLE_ASSET_GLTFMESH_VERT_SPV);
        auto VertexShaderInfo = Rr_ShaderInfo{
            .SPVSize = VertexShader.Size,
            .SPVData = VertexShader.Data,
        };

        auto FragmentShader = Rr_LoadAsset(EXAMPLE_ASSET_GLTFMESH_FRAG_SPV);
        auto FragmentShaderInfo = Rr_ShaderInfo{
            .SPVSize = FragmentShader.Size,
            .SPVData = FragmentShader.Data,
        };

        auto PipelineInfo = Rr_GraphicsPipelineCreateInfo{
            .VertexShaderInfo = &VertexShaderInfo,
            .FragmentShaderInfo = &FragmentShaderInfo,
            .VertexInputBindingCount = VertexInputBindings.size(),
            .VertexInputBindings = VertexInputBindings.data(),
            .ColorTargetCount = ColorTargets.size(),
            .ColorTargets = ColorTargets.data(),
            .Rasterizer =
                Rr_Rasterizer{
                    .CullMode = RR_CULL_MODE_NONE,
                    .FrontFace = RR_FRONT_FACE_COUNTER_CLOCKWISE,
                },
            .DepthStencil =
                Rr_DepthStencil{
                    .Format = RR_IMAGE_FORMAT_D32_SFLOAT,
                    .CompareOp = RR_COMPARE_OP_LESS,
                    .EnableDepthTest = true,
                    .EnableDepthWrite = true,
                },
        };
        GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

        UniformBuffer = Rr_CreateBuffer(sizeof(GPUUniform), RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_DYNAMIC);

        GPUUniform.Model = Rr_M4D(1.0f);
        GPUUniform.View = Rr_LookAt_RH(Rr_V3(0.0f, 0.0f, -5.0f), Rr_V3F(0.0f), Rr_V3(0.0f, 1.0f, 0.0f));

        InitGLTFPrimitive();

        InitDepthImage();
    }

    void Event(Rr_Event const *Event)
    {
        switch (Event->Type)
        {
            case RR_EVENT_TYPE_SWAPCHAIN_CREATED:
            {
                InitDepthImage();

                return;
            }
            default:
                return;
        }
    }

    void Iterate()
    {
        Rr_UIBeginDebugOverlayTabs();
        if (Rr_UIBeginWindow("GLTFMesh.cxx"))
        {
            Rr_UIText("This example shows using cGLTF to load and draw meshes.");
        }
        Rr_UIEndWindow();
        Rr_UIEndDebugOverlayTabs();

        auto SwapchainImage = Rr_GetSwapchainImage();
        auto SwapchainAspect = Rr_GetImage2DAspect(SwapchainImage);

        Camera.UpdatePerspective(SwapchainAspect);
        Camera.Update();

        GPUUniform.Model = Rr_MulM4(Rr_Rotate_RH(0.005f, Rr_V3(0.0f, 1.0f, 0.0f)), GPUUniform.Model);
        GPUUniform.View = Camera.GetViewMatrix();
        GPUUniform.Projection = Camera.GetProjectionMatrix();
        GPUUniform.Time = (float)Rr_GetTimeSeconds();
        memcpy(Rr_GetMappedBufferData(UniformBuffer), &GPUUniform, sizeof(GPUUniform));

        auto ColorTarget = Rr_ColorTarget{
            .Image = SwapchainImage,
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
            .Clear = { Rr_V4(0.03f, 0.03f, 0.04f, 1.0f) },
        };
        auto DepthTarget = Rr_DepthTarget{
            .Image = DepthImage,
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
            .Clear = Rr_DepthClear{ 1.0f, 0 },
        };
        auto GraphicsNode = Rr_AddGraphicsNode(Rr_GetGraph(), 1, &ColorTarget, &DepthTarget);
        Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        Rr_BindVertexBuffer(GraphicsNode, PrimitiveBuffer, 0, 0);
        Rr_BindIndexBuffer(GraphicsNode, PrimitiveBuffer, 0, PrimitiveIndexOffset, RR_INDEX_TYPE_UINT16);
        Rr_BindUniformBuffer(GraphicsNode, UniformBuffer, 0, 0, 0, sizeof(GPUUniform));
        Rr_BindSampler(GraphicsNode, Sampler, 0, 1);
        Rr_BindSampledImage2D(GraphicsNode, PrimitiveTexture, 0, 2);
        Rr_DrawIndexed(GraphicsNode, PrimitiveIndexCount, 1, 0, 0, 0);
    }

    ~CGLTFMeshApp()
    {
        Rr_ReleaseBuffer(PrimitiveBuffer);
        Rr_ReleaseImage(PrimitiveTexture);
        Rr_ReleaseImage(DepthImage);
        Rr_ReleaseBuffer(UniformBuffer);
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
        Rr_ReleaseSampler(Sampler);
    }
};

int main()
{
    static CGLTFMeshApp *App{};

    Rr_Config Config = {
        .WindowTitle = "GLTFMesh",
        .WindowFlags = RR_WINDOW_FLAGS_RESIZE_BIT,
        .InitFunc = []() { App = new CGLTFMeshApp(); },
        .EventFunc = [](Rr_Event const *Event) { App->Event(Event); },
        .IterateFunc = []() { App->Iterate(); },
        .CleanupFunc = []() { delete App; },
    };
    Rr_Run(&Config);
}
