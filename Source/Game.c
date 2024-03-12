#include "Game.h"

#include <Rr/RrApp.h>
#include <Rr/RrTypes.h>
#include <Rr/RrMesh.h>
#include <Rr/RrRenderer.h>

static Rr_RawMesh RawMesh = { 0 };

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
}

void RunGame(void)
{
    Rr_AppConfig Config = {
        .Title = "VulkanPlayground",
        .ReferenceResolution = { 320, 240 },
        .InitFunc = Init,
        .CleanupFunc = Cleanup,
        .UpdateFunc = Update,
    };
    Rr_Run(&Config);
}
