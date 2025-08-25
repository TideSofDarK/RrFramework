#include <Rr/Rr.h>

#include <cstdlib>

struct SMyStruct
{
    uint64_t Test0{};
    char Test3{};
    int Test1;
    short Test10{};
    double Test2{};
    float Test4{};
    char Test5{};
    float Test6{};

    SMyStruct(int InTest)
        : Test1(InTest)
    {
    }
};

#define RR_HIVE_TYPE      SMyStruct
#define RR_HIVE_TYPE_NAME MyStruct
#include <Rr/Rr_Hive.h>

static Rr_Arena *Arena;
static SMyStructHive Hive;

static void Init()
{
    Arena = Rr_CreateDefaultArena();

    for (size_t Index = 0; Index < 12; ++Index)
    {
        *PushMyStructIntoHive(&Hive, Arena).Element = SMyStruct((int)Index);
    }
}

static void Iterate()
{
    Rr_Graph *Graph = Rr_GetGraph();

    Rr_ColorClear ColorClear = {};

    Rr_AddClearColorImage2DNode(
        Graph,
        "clear",
        &ColorClear,
        Rr_GetSwapchainImage());

    if (Rr_UIBeginWindow("Hive", NULL, 0))
    {
        Rr_UILabelF("Total Count: %d", Hive.Count);
        Rr_UILabelF("Total Capacity: %d", Hive.Capacity);
#ifdef RR_DEBUG
        Rr_UILabelF("Total Groups Allocated: %d", Hive.TotalGroups);
#endif
        Rr_UIBeginHorizontal();
        if (Rr_UIButton("Add"))
        {
            *PushMyStructIntoHive(&Hive, Arena).Element =
                SMyStruct(std::rand());
        }
        if (Rr_UIButton("Clear"))
        {
            ClearMyStructHive(&Hive);
        }
        Rr_UIEndHorizontal();
        Rr_UISeparator();
        int Index = 0;
        for (SMyStructHiveIterator It = Hive.Begin;
             It.Element != Hive.End.Element;)
        {
            Rr_UIBeginHorizontal();
            Rr_UILabelF(
                "%d) Group: %d, Value: %d",
                Index,
                It.Group->GroupNumber,
                It.Element->Test1);
            if (Rr_UIButton("Remove"))
            {
                RemoveFromMyStructHive(&Hive, &It);
            }
            else
            {
                AdvanceMyStructHiveIterator(&It);
            }
            Rr_UIEndHorizontal();
            Index++;
        }
        Rr_UIEndWindow();
    }
}

static void Cleanup()
{
    Rr_DestroyArena(Arena);
}

int main(int ArgC, char **ArgV)
{
    Rr_AppConfig Config = {};
    Config.Title = "HiveTest";
    Config.InitFunc = Init;
    Config.CleanupFunc = Cleanup;
    Config.IterateFunc = Iterate;
    Rr_Run(&Config);

    return 0;
}
