#include "Game.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <imgui/cimgui.h>

#include <cglm/vec3.h>
#include <cglm/mat4.h>
#include <cglm/euler.h>

#include <SDL3/SDL_scancode.h>

#include <Rr/RrDefines.h>
#include <Rr/RrImage.h>
#include <Rr/RrApp.h>
#include <Rr/RrBuffer.h>
#include <Rr/RrTypes.h>
#include <Rr/RrMesh.h>
#include <Rr/RrRenderer.h>
#include <Rr/RrInput.h>
#include <Rr/RrDescriptor.h>
#include <Rr/RrPipeline.h>

#include "Render.h"

typedef struct SFrameData
{
    Rr_Buffer ShaderGlobalsBuffer;
    Rr_Buffer ObjectDataBuffer;
} SFrameData;

typedef struct SUber3DGlobals
{
    mat4 View;
    mat4 Proj;
    mat4 ViewProj;
    vec4 AmbientColor;
} SUber3DGlobals;

typedef struct SUber3DObject
{
    mat4 Model;
} SUber3DObject;

typedef enum EInputAction
{
    EIA_UP,
    EIA_DOWN,
    EIA_LEFT,
    EIA_RIGHT,
    EIA_COUNT,
} EInputAction;

static Rr_InputMapping InputMappings[RR_MAX_INPUT_MAPPINGS] = { 0 };

static SUber3DGlobals ShaderGlobals;

static VkDescriptorSetLayout ShaderGlobalsLayout;
static VkDescriptorSetLayout ObjectDataLayout;

static Rr_MeshBuffers MonkeyMesh = { 0 };
static Rr_Pipeline MeshPipeline = { 0 };

static Rr_Image SceneDepthImage;
static Rr_Image SceneColorImage;

static void InitInputMappings(void)
{
    InputMappings[EIA_UP].Primary = SDL_SCANCODE_W;
    InputMappings[EIA_DOWN].Primary = SDL_SCANCODE_S;
    InputMappings[EIA_LEFT].Primary = SDL_SCANCODE_A;
    InputMappings[EIA_RIGHT].Primary = SDL_SCANCODE_D;
}

static void InitMeshPipeline(Rr_Renderer* const Renderer)
{
    /* Layout */
    Rr_DescriptorLayoutBuilder Builder = { 0 };
    Rr_AddDescriptor(&Builder, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    Rr_AddDescriptor(&Builder, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    ShaderGlobalsLayout = Rr_BuildDescriptorLayout(&Builder, Renderer->Device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    Rr_ClearDescriptors(&Builder);
    Rr_AddDescriptor(&Builder, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    ObjectDataLayout = Rr_BuildDescriptorLayout(&Builder, Renderer->Device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    VkDescriptorSetLayout DescriptorSetLayouts[] = { ShaderGlobalsLayout, ObjectDataLayout };

    /* Pipeline */
    Rr_PipelineCreateInfo PipelineCreateInfo = {
        .DescriptorSetLayouts = DescriptorSetLayouts,
        .DescriptorSetLayoutCount = SDL_arraysize(DescriptorSetLayouts),
        .PushConstantsSize = 128
    };

    RrAsset_Extern(&PipelineCreateInfo.VertexShaderAsset, Uber3DVERT);
    RrAsset_Extern(&PipelineCreateInfo.FragmentShaderAsset, Uber3DFRAG);

    MeshPipeline = Rr_CreatePipeline(Renderer, &PipelineCreateInfo);
}

static void InitGlobals(Rr_Renderer* const Renderer)
{
    glm_vec4_copy((vec4){ 1.0f, 1.0f, 1.0f, 0.5f }, ShaderGlobals.AmbientColor);

    glm_euler_xyz((vec3){ glm_rad(63.5593f), glm_rad(0.0f), glm_rad(46.6919f) }, ShaderGlobals.View);
    ShaderGlobals.View[0][3] = 7.35889f;
    ShaderGlobals.View[1][3] = -6.92579f;
    ShaderGlobals.View[2][3] = 4.95831f;

    glm_perspective_rh_no(glm_rad(40.0f), (float)Renderer->DrawTarget.ActiveResolution.width / (float)Renderer->DrawTarget.ActiveResolution.height, 1.0f, 1000.0f, ShaderGlobals.Proj);

    glm_mat4_mul(ShaderGlobals.Proj, ShaderGlobals.View, ShaderGlobals.ViewProj);
}

static void Init(Rr_App* App)
{
    InitInputMappings();
    InitRenderer();

    Rr_Renderer* const Renderer = &App->Renderer;

    Rr_Asset POCDepthEXR;
    RrAsset_Extern(&POCDepthEXR, POCDepthEXR);
    SceneDepthImage = Rr_CreateImage_FromEXR(&POCDepthEXR, &App->Renderer);

    Rr_Asset POCColorPNG;
    RrAsset_Extern(&POCColorPNG, POCColorPNG);
    SceneColorImage = Rr_CreateImage_FromPNG(&POCColorPNG, &App->Renderer, VK_IMAGE_USAGE_SAMPLED_BIT, false, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    Rr_Asset DoorFrameOBJ;
    RrAsset_Extern(&DoorFrameOBJ, DoorFrameOBJ);
    MonkeyMesh = Rr_CreateMesh_FromOBJ(Renderer, &DoorFrameOBJ);

    InitMeshPipeline(Renderer);
    InitGlobals(Renderer);
}

static void Cleanup(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;
    VkDevice Device = Renderer->Device;

    Rr_DestroyPipeline(Renderer, &MeshPipeline);

    vkDestroyDescriptorSetLayout(Device, ShaderGlobalsLayout, NULL);
    vkDestroyDescriptorSetLayout(Device, ObjectDataLayout, NULL);

    for (int Index = 0; Index < RR_FRAME_OVERLAP; Index++)
    {
        SFrameData* PerFrameData = (SFrameData*)Renderer->PerFrameDatas + Index;

        Rr_DestroyBuffer(&PerFrameData->ShaderGlobalsBuffer, Renderer->Allocator);
        Rr_DestroyBuffer(&PerFrameData->ObjectDataBuffer, Renderer->Allocator);
    }
    SDL_free(Renderer->PerFrameDatas);

    Rr_DestroyImage(Renderer, &SceneDepthImage);
    Rr_DestroyImage(Renderer, &SceneColorImage);
    Rr_DestroyMesh(Renderer, &MonkeyMesh);
}

static void Update(Rr_App* App)
{
    Rr_KeyState Up = Rr_GetKeyState(App->InputState, EIA_UP);
    if (Up == RR_KEYSTATE_PRESSED)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "%d", Up);
    }

    igShowDemoWindow(NULL);
    Rr_DebugOverlay(App);
}

static void Draw(Rr_App* const App)
{
    Rr_Renderer* Renderer = &App->Renderer;
    Rr_Frame* Frame = Rr_GetCurrentFrame(Renderer);
    SFrameData* PerFrameData = (SFrameData*)Rr_GetCurrentFrameData(Renderer);
    VkDevice Device = Renderer->Device;

    Rr_DestroyBuffer(&PerFrameData->ShaderGlobalsBuffer, Renderer->Allocator);
    Rr_InitMappedBuffer(&PerFrameData->ShaderGlobalsBuffer, Renderer->Allocator, sizeof(SUber3DGlobals), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    Rr_DestroyBuffer(&PerFrameData->ObjectDataBuffer, Renderer->Allocator);
    Rr_InitMappedBuffer(&PerFrameData->ObjectDataBuffer, Renderer->Allocator, sizeof(SUber3DObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    SDL_memcpy(
        PerFrameData->ShaderGlobalsBuffer.AllocationInfo.pMappedData,
        &ShaderGlobals,
        sizeof(SUber3DGlobals));

    VkDescriptorSet SceneDataDescriptorSet = Rr_AllocateDescriptorSet(&Frame->DescriptorAllocator, Renderer->Device, ShaderGlobalsLayout);
    Rr_DescriptorWriter Writer = Rr_CreateDescriptorWriter(1, 1);
    Rr_WriteDescriptor_Buffer(&Writer, 0, PerFrameData->ShaderGlobalsBuffer.Handle, sizeof(SUber3DGlobals), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    Rr_WriteDescriptor_Image(&Writer, 1, SceneDepthImage.View, Renderer->NearestSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    Rr_UpdateDescriptorSet(&Writer, Device, SceneDataDescriptorSet);
    Rr_ResetDescriptorWriter(&Writer);

    VkDescriptorSet ObjectDataDescriptorSet = Rr_AllocateDescriptorSet(&Frame->DescriptorAllocator, Renderer->Device, ObjectDataLayout);
    Rr_WriteDescriptor_Buffer(&Writer, 0, PerFrameData->ObjectDataBuffer.Handle, sizeof(SUber3DObject), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    Rr_UpdateDescriptorSet(&Writer, Device, ObjectDataDescriptorSet);
    Rr_DestroyDescriptorWriter(&Writer);
}

void RunGame(void)
{
    Rr_AppConfig Config = {
        .Title = "VulkanPlayground",
        .ReferenceResolution = { 1920 / 2, 1080 / 2 },
        .InputConfig = &(Rr_InputConfig){
            .Mappings = InputMappings,
            .Count = EIA_COUNT },
        .PerFrameDataSize = sizeof(SFrameData),
        .InitFunc = Init,
        .CleanupFunc = Cleanup,
        .UpdateFunc = Update,
        .DrawFunc = Draw
    };
    Rr_Run(&Config);
}
