#include "Game.h"
#include "Rr/RrDefines.h"
#include "Rr/RrImage.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <imgui/cimgui.h>

#include <cglm/vec3.h>
#include <cglm/mat4.h>

#include <SDL3/SDL_scancode.h>

#include <Rr/RrApp.h>
#include <Rr/RrBuffer.h>
#include <Rr/RrTypes.h>
#include <Rr/RrMesh.h>
#include <Rr/RrRenderer.h>
#include <Rr/RrInput.h>
#include <Rr/RrDescriptor.h>

#include "Render.h"

typedef struct SPerFrameData
{
    Rr_Buffer ShaderGlobalsBuffer;
} SPerFrameData;

typedef struct SShaderGlobals
{
    mat4 View;
    mat4 Proj;
    mat4 ViewProj;
    vec4 AmbientColor;
} SShaderGlobals;

typedef enum EInputAction
{
    EIA_UP,
    EIA_DOWN,
    EIA_LEFT,
    EIA_RIGHT,
    EIA_COUNT,
} EInputAction;

static VkDescriptorSetLayout ShaderGlobalsLayout;

static mat4 CameraTransform = { 0 };

static Rr_InputMapping InputMappings[RR_MAX_INPUT_MAPPINGS] = { 0 };

static Rr_RawMesh RawMesh = { 0 };

static void InitInputMappings(void)
{
    InputMappings[EIA_UP].Primary = SDL_SCANCODE_W;
    InputMappings[EIA_DOWN].Primary = SDL_SCANCODE_S;
    InputMappings[EIA_LEFT].Primary = SDL_SCANCODE_A;
    InputMappings[EIA_RIGHT].Primary = SDL_SCANCODE_D;
}

static void Init(Rr_App* App)
{
    InitInputMappings();
    InitRenderer();

    Rr_Asset POCDepthEXR;
    RrAsset_Extern(&POCDepthEXR, POCDepthEXR);
    Rr_LoadEXRDepth(&POCDepthEXR, &App->Renderer);

    Rr_Asset DoorFrameOBJ;
    RrAsset_Extern(&DoorFrameOBJ, DoorFrameOBJ);

    Rr_ParseOBJ(&RawMesh, &DoorFrameOBJ);

    Rr_SetMesh(&App->Renderer, &RawMesh);

    Rr_DescriptorLayoutBuilder Builder = { 0 };
    DescriptorLayoutBuilder_Add(&Builder, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    ShaderGlobalsLayout = DescriptorLayoutBuilder_Build(&Builder, App->Renderer.Device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
}

static void Cleanup(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;

    Rr_DestroyRawMesh(&RawMesh);

    for (int Index = 0; Index < RR_FRAME_OVERLAP; Index++)
    {
        SPerFrameData* PerFrameData = &((SPerFrameData*)Renderer->PerFrameDatas)[Index];

        Rr_DestroyBuffer(&PerFrameData->ShaderGlobalsBuffer, Renderer->Allocator);
    }
    SDL_free(Renderer->PerFrameDatas);

    vkDestroyDescriptorSetLayout(Renderer->Device, ShaderGlobalsLayout, NULL);
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
    SPerFrameData* PerFrameData = &((SPerFrameData*)Renderer->PerFrameDatas)[Renderer->FrameNumber % RR_FRAME_OVERLAP];
    VkDevice Device = Renderer->Device;

    Rr_DestroyBuffer(&PerFrameData->ShaderGlobalsBuffer, Renderer->Allocator);
    Rr_InitMappedBuffer(&PerFrameData->ShaderGlobalsBuffer, Renderer->Allocator, sizeof(SShaderGlobals), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    SShaderGlobals ShaderGlobals;
    glm_vec4_copy((vec4){ 1.0f, 1.0f, 1.0f, 0.5f }, ShaderGlobals.AmbientColor);
    SDL_memcpy(
        PerFrameData->ShaderGlobalsBuffer.AllocationInfo.pMappedData,
        &ShaderGlobals,
        sizeof(SShaderGlobals));
    VkDescriptorSet SceneDataDescriptorSet = DescriptorAllocator_Allocate(&Frame->DescriptorAllocator, Renderer->Device, ShaderGlobalsLayout);
    SDescriptorWriter Writer = { 0 };
    DescriptorWriter_Init(&Writer, 0, 1);
    DescriptorWriter_WriteBuffer(&Writer, 0, PerFrameData->ShaderGlobalsBuffer.Handle, sizeof(SShaderGlobals), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    DescriptorWriter_Update(&Writer, Device, SceneDataDescriptorSet);
    DescriptorWriter_Cleanup(&Writer);
}

void RunGame(void)
{
    Rr_AppConfig Config = {
        .Title = "VulkanPlayground",
        .ReferenceResolution = { 1920 / 2, 1080 / 2 },
        .InputConfig = &(Rr_InputConfig){
            .Mappings = InputMappings,
            .Count = EIA_COUNT },
        .PerFrameDataSize = sizeof(SPerFrameData),
        .InitFunc = Init,
        .CleanupFunc = Cleanup,
        .UpdateFunc = Update,
        .DrawFunc = Draw
    };
    Rr_Run(&Config);
}
