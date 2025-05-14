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
        *PushMyStructIntoHive(&Hive, Arena) = (SMyStruct){
            .Test1 = (int)Index,
        };
    }
}

static void Iterate(void *UserData)
{
    Rr_BeginWindow("Hive", 0);
    Rr_LabelF("Total Count: %d", Hive.Count);
    Rr_BeginHorizontal();
    if(Rr_Button("Add"))
    {
        *PushMyStructIntoHive(&Hive, Arena) = (SMyStruct){
            .Test1 = rand(),
        };
    }
    if(Rr_Button("Clear"))
    {
    }
    Rr_EndHorizontal();
    Rr_Separator();
    int Index = 0;
    for(SMyHiveIterator It = Hive.Begin; MyStructHiveIteratorValid(&It);
        AdvanceMyStructHiveIterator(&It))
    {
        Rr_BeginHorizontal();
        Rr_LabelF("%d) Value == %d", Index, It.Element->Test1);
        if(Rr_Button("Remove"))
        {
            RemoveFromMyStructHive(&It);
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
