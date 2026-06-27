#include <Rr/Rr_Math.h>

#include <assert.h>
#include <stdio.h>

static inline void EqualV4(Rr_Vec4 const *Vec0, Rr_Vec4 const *Vec1)
{
    assert(Vec0->X == Vec1->X);
    assert(Vec0->Y == Vec1->Y);
    assert(Vec0->Z == Vec1->Z);
    assert(Vec0->W == Vec1->W);
}

static inline void TestCeil(Rr_Vec4 Original)
{
    Rr_Vec4 Vec0 = Rr_CeilV4(Original);
    Rr_Vec4 Vec1 = Original;
    Vec1.X = ceilf(Vec1.X);
    Vec1.Y = ceilf(Vec1.Y);
    Vec1.Z = ceilf(Vec1.Z);
    Vec1.W = ceilf(Vec1.W);
    EqualV4(&Vec0, &Vec1);
}

static inline void TestFloor(Rr_Vec4 Original)
{
    Rr_Vec4 Vec0 = Rr_FloorV4(Original);
    Rr_Vec4 Vec1 = Original;
    Vec1.X = floorf(Vec1.X);
    Vec1.Y = floorf(Vec1.Y);
    Vec1.Z = floorf(Vec1.Z);
    Vec1.W = floorf(Vec1.W);
    EqualV4(&Vec0, &Vec1);
}

static inline void TestTrunc(Rr_Vec4 Original)
{
    Rr_Vec4 Vec0 = Rr_TruncV4(Original);
    Rr_Vec4 Vec1 = Original;
    Vec1.X = truncf(Vec1.X);
    Vec1.Y = truncf(Vec1.Y);
    Vec1.Z = truncf(Vec1.Z);
    Vec1.W = truncf(Vec1.W);
    EqualV4(&Vec0, &Vec1);
}

static inline void TestRound(Rr_Vec4 Original)
{
    Rr_Vec4 Vec0 = Rr_RoundV4(Original);
    Rr_Vec4 Vec1 = Original;
    Vec1.X = roundf(Vec1.X);
    Vec1.Y = roundf(Vec1.Y);
    Vec1.Z = roundf(Vec1.Z);
    Vec1.W = roundf(Vec1.W);
    EqualV4(&Vec0, &Vec1);
}

int main(int ArgCount, char **Args)
{
    Rr_Vec4 VecOriginal;

    float const CASES[] = {
        -1.0f, -0.75f, -0.5f, -0.25f, -0.0f,    0.0f,
        0.25f, 0.5f,   0.75f, 1.0f,   123.456f, -123.456f,
    };

    for (size_t Index = 0; Index < RR_ARRAY_COUNT(CASES); ++Index)
    {
        TestCeil(Rr_V4F(CASES[Index]));
    }

    for (size_t Index = 0; Index < RR_ARRAY_COUNT(CASES); ++Index)
    {
        TestFloor(Rr_V4F(CASES[Index]));
    }

    for (size_t Index = 0; Index < RR_ARRAY_COUNT(CASES); ++Index)
    {
        TestTrunc(Rr_V4F(CASES[Index]));
    }

    for (size_t Index = 0; Index < RR_ARRAY_COUNT(CASES); ++Index)
    {
        TestRound(Rr_V4F(CASES[Index]));
    }

    return 0;
}
