#include <Rr/Rr.h>

#include <stdio.h>
#include <stdlib.h>

typedef struct SMyStruct SMyStruct;
struct SMyStruct
{
    uint64_t Test0;
    char Test3;
    int Test1;
    short Test10;
    double Test2;
    float Test4;
    char Test5;
    float Test6;
};

#define RR_HIVE_TYPE      SMyStruct
#define RR_HIVE_TYPE_NAME MyStruct
#define RR_HIVE_NAME      SMyHive
#include <Rr/Rr_Hive.h>

static Rr_Arena *Arena;
static SMyHive Hive;

static void Init(void *UserData)
{
    Arena = Rr_CreateDefaultArena();

    for(size_t Index = 0; Index < 12; ++Index)
    {
        *PushMyStructIntoHive(&Hive, Arena).Element = (SMyStruct){
            .Test1 = (int)Index,
        };
    }
}

static void Iterate(void *UserData)
{
    Rr_Renderer *Renderer = Rr_GetRenderer();

    Rr_AddClearColorImageNode(
        Renderer,
        "clear",
        &(Rr_ColorClear){ 0 },
        Rr_GetSwapchainImage(Renderer));

    Rr_BeginWindow("Hive", 0);
    Rr_LabelF("Total Count: %d", Hive.Count);
    Rr_LabelF("Total Capacity: %d", Hive.Capacity);
#ifdef RR_DEBUG
    Rr_LabelF("Total Groups Allocated: %d", Hive.TotalGroups);
#endif
    Rr_BeginHorizontal();
    if(Rr_Button("Add"))
    {
        *PushMyStructIntoHive(&Hive, Arena).Element = (SMyStruct){
            .Test1 = rand(),
        };
    }
    if(Rr_Button("Clear"))
    {
    }
    Rr_EndHorizontal();
    Rr_Separator();
    int Index = 0;
    for(SMyHiveIterator It = Hive.Begin; It.Element != Hive.End.Element;)
    {
        Rr_BeginHorizontal();
        Rr_LabelF(
            "%d) Group: %d, Skip: %d, Value: %d",
            Index,
            It.Group->GroupNumber,
            *It.Skip,
            It.Element->Test1);
        if(Rr_Button("Remove"))
        {
            RemoveFromMyStructHive(&Hive, &It);
        }
        else
        {
            AdvanceMyStructHiveIterator(&It);
        }
        Rr_EndHorizontal();
        Index++;
    }
    Rr_EndWindow();
}

static void Cleanup(void *UserData)
{
    Rr_DestroyArena(Arena);
}

int main(int ArgC, char **ArgV)
{
    Rr_AppConfig Config = {
        .Title = "HiveTest",
        .Version = "1.0.0",
        .Package = "com.rr.examples.hivetest",
        .InitFunc = Init,
        .CleanupFunc = Cleanup,
        .IterateFunc = Iterate,
    };
    Rr_Run(&Config);

    return 0;
}
