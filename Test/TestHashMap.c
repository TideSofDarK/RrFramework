#include <Rr/Rr.h>

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <time.h>

typedef struct TestKey TestKey;
struct TestKey
{
    char String[8];
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

#define INITIAL_CAPACITY 64

#define RR_HASH_MAP_NAME             TestMap
#define RR_HASH_MAP_KEY_TYPE         TestKey
#define RR_HASH_MAP_VALUE_TYPE       TestValue
#define RR_HASH_MAP_COMPARE_NAME     TestKeyEqual
#define RR_HASH_MAP_INITIAL_CAPACITY INITIAL_CAPACITY
#include "../Source/Rr_HashMap.h"

#define TEST_COUNT 999998

int main(int ArgCount, char **Args)
{
    Rr_InitSystem();
    Rr_InitThreadContext();

    Rr_Scratch Scratch = Rr_GetScratch(NULL);
    Rr_Arena *Arena = Scratch.Arena;

    TestMap Map = { 0 };
    InitTestMap(&Map, Arena);

    assert(Map.Capacity == INITIAL_CAPACITY);
    assert(RR_IS_POW2(Map.Capacity));

    ReserveTestMap(&Map, 25, Arena);

    /* Default load factor is 0.75 so make sure it's 64. */

    assert(Map.Capacity == 64);

    TestPair *TestPairs = Rr_Alloc(sizeof(TestPair) * TEST_COUNT, Arena);
    for (size_t Index = 0; Index < TEST_COUNT; ++Index)
    {
        TestPair *TestPair = &TestPairs[Index];
        TestKey *Key = &TestPair->Key;
        TestValue *Value = &TestPair->Value;
        snprintf(Key->String, sizeof(Key->String), "k%zu", Index);
        Value->Integer = Index;

        InsertIntoTestMap(&Map, &TestPair->Key, &TestPair->Value, Arena);
    }

    /* Make sure we are at the correct count. */

    assert(Map.Count == TEST_COUNT);
    assert(RR_IS_POW2(Map.Capacity));

    /* Make sure we can visit every bucket. */

    {
        size_t Count = 0;
        TestMapIterator It = BeginInTestMap(&Map);
        while (!IsTestMapEnd(It))
        {
            Count++;

            It = NextInTestMap(It);
        }
        assert(Count == Map.Count);
    }

    /* Lets find and erase every even indexed element. */

    for (size_t Index = 0; Index < TEST_COUNT; Index += 2)
    {
        TestKey Key = { 0 };
        snprintf(Key.String, sizeof(Key.String), "k%zu", Index);

        TestMapIterator It = FindInTestMap(&Map, &Key);
        assert(strcmp(It.Data->Key.String, Key.String) == 0);
        assert(It.Data->Value.Integer == Index);
        EraseFromTestMap(It);
    }

    /* Make sure we have halved the count. */

    assert(Map.Count == (TEST_COUNT / 2));

    size_t CurrentCapacity = Map.Capacity;

    RehashTestMap(&Map, 1, Arena);

    /* Make sure rehashing with grow doubled our capacity. */

    assert(Map.Count == (TEST_COUNT / 2));
    assert(Map.Capacity == (CurrentCapacity * 2));

    RehashTestMap(&Map, 0, NULL);

    /* Make sure rehashing without grow didn't affect count/capacity. */

    assert(Map.Count == (TEST_COUNT / 2));
    assert(Map.Capacity == (CurrentCapacity * 2));

    TestKey Key = { 0 };

    /* Make sure an odd '1337' is still present. */

    snprintf(Key.String, sizeof(Key.String), "k%u", 1337U);
    TestMapIterator It = FindInTestMap(&Map, &Key);
    assert(It.Data);
    assert(It.Data->Value.Integer == 1337ULL);

    /* Make sure an even '1338' is erased. */

    snprintf(Key.String, sizeof(Key.String), "k%u", 1338U);
    It = FindInTestMap(&Map, &Key);
    assert(IsTestMapEnd(It));

    /* Insert it again and make sure we can find it. */

    TestValue Value = { 1338ULL };
    InsertIntoTestMap(&Map, &Key, &Value, NULL);
    It = FindInTestMap(&Map, &Key);
    assert(It.Data);
    assert(It.Data->Value.Integer == 1338ULL);

    Rr_DestroyScratch(Scratch);
}
