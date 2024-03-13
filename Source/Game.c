#include "Game.h"

#include <SDL3/SDL_scancode.h>

#include <Rr/RrApp.h>
#include <Rr/RrTypes.h>
#include <Rr/RrMesh.h>
#include <Rr/RrRenderer.h>
#include <Rr/RrInput.h>

typedef enum EInputAction
{
    EIA_UP,
    EIA_DOWN,
    EIA_LEFT,
    EIA_RIGHT,
    EIA_COUNT,
} EInputAction;

static Rr_InputMapping InputMappings[RR_MAX_INPUT_MAPPINGS] = {0};

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
    Rr_Asset DoorFrameOBJ;
    RrAsset_Extern(&DoorFrameOBJ, DoorFrameOBJ);

    Rr_ParseOBJ(&RawMesh, &DoorFrameOBJ);

    Rr_SetMesh(&App->Renderer, &RawMesh);
}

static void Cleanup(Rr_App* App)
{
    Rr_FreeRawMesh(&RawMesh);
}

static void Update(Rr_App* App)
{
    Rr_KeyState Up = Rr_GetKeyState(App->InputState, EIA_UP);
    // SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "%d", Up);
}

void RunGame(void)
{
    InitInputMappings();
    Rr_AppConfig Config = {
        .Title = "VulkanPlayground",
        .ReferenceResolution = { 320, 240 },
        .InputConfig = &(Rr_InputConfig) {
            .Mappings = InputMappings,
            .Count = EIA_COUNT
        },
        .InitFunc = Init,
        .CleanupFunc = Cleanup,
        .UpdateFunc = Update,
    };
    Rr_Run(&Config);
}
