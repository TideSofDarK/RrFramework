#include <Rr/Rr.h>

#include <limits.h>
#include <stdio.h>
#include <time.h>

typedef struct TestKey TestKey;
struct TestKey
{
    char *String;
};

typedef struct TestValue TestValue;
struct TestValue
{
    uint64_t Integer;
};

static inline bool TestKeyEqual(TestKey const *KeyA, TestKey const *KeyB)
{
    return strcmp(KeyA->String, KeyB->String) == 0;
}

typedef struct TestPair TestPair;
struct TestPair
{
    TestKey Key;
    TestValue Value;
};

#define INITIAL_CAPACITY 32

#define RR_HASH_MAP_NAME             TestMap
#define RR_HASH_MAP_KEY_TYPE         TestKey
#define RR_HASH_MAP_VALUE_TYPE       TestValue
#define RR_HASH_MAP_COMPARE_NAME     TestKeyEqual
#define RR_HASH_MAP_INITIAL_CAPACITY INITIAL_CAPACITY
#include "../Source/Rr_HashMap.h"

#define TEST_COUNT 1337

int main(int ArgCount, char **Args)
{
    srand(time(NULL));

    Rr_InitSystem();
    Rr_InitThreadContext();

    Rr_Scratch Scratch = Rr_GetScratch(NULL);
    Rr_Arena *Arena = Scratch.Arena;

    TestMap Map = { 0 };
    InitTestMap(&Map, Arena);

    assert(Map.Capacity == INITIAL_CAPACITY);

    /* Assuming default load factor of 0.75 */
    ReserveTestMap(&Map, 25, Arena);
    assert(Map.Capacity == 64);

    TestPair *TestPairs = Rr_Alloc(sizeof(TestPair) * TEST_COUNT, Arena);
    for (size_t Index = 0; Index < TEST_COUNT; ++Index)
    {
        TestPair *TestPair = &TestPairs[Index];
        TestPair->Key.String = Rr_AllocNoZero(8, Arena);
        snprintf(TestPair->Key.String, 8, "k%zu", Index);
        TestPair->Value.Integer = rand();

        InsertIntoTestMap(&Map, &TestPair->Key, &TestPair->Value, Arena);
    }

    assert(Map.Count == TEST_COUNT);
    assert(RR_IS_POW2(Map.Capacity));

    Rr_DestroyScratch(Scratch);
}
