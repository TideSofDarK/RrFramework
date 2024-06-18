#pragma once

#include "Rr_Defines.h"

// Dummy macros for when test framework is not present.
#ifndef COVERAGE
    #define COVERAGE(a, b)
#endif

#ifndef ASSERT_COVERED
    #define ASSERT_COVERED(a)
#endif

#ifdef RR_MATH_NO_SSE
    #warning "RR_MATH_NO_SSE is deprecated, use RR_MATH_NO_SIMD instead"
    #define RR_MATH_NO_SIMD
#endif

/* let's figure out if SSE is really available (unless disabled anyway)
   (it isn't on non-x86/x86_64 platforms or even x86 without explicit SSE
   support)
   => only use "#ifdef RR_MATH__USE_SSE" to check for SSE support below this
   block! */
#ifndef RR_MATH_NO_SIMD
    #ifdef _MSC_VER /* MSVC supports SSE in amd64 mode or _M_IX86_FP >= 1 (2 \
                       means SSE2) */
        #if defined(_M_AMD64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
            #define RR_MATH__USE_SSE 1
        #endif
    #else
    /* not MSVC, probably GCC, clang, icc or something that doesn't support SSE
     * anyway */
        #ifdef __SSE__ /* they #define __SSE__ if it's supported */
            #define RR_MATH__USE_SSE 1
        #endif /*  __SSE__ */
    #endif
    /* not _MSC_VER */
    #ifdef __ARM_NEON
        #define RR_MATH__USE_NEON 1
    #endif
/* NEON Supported */
#endif /* #ifndef RR_MATH_NO_SIMD */

#if (                                                  \
    !defined(__cplusplus) && defined(__STDC_VERSION__) \
    && __STDC_VERSION__ >= 201112L)
    #define RR_MATH__USE_C11_GENERICS 1
#endif

#ifdef RR_MATH__USE_SSE
    #include <xmmintrin.h>
#endif

#ifdef RR_MATH__USE_NEON
    #include <arm_neon.h>
#endif

#ifdef _MSC_VER
    #pragma warning(disable : 4201)
#endif

#if defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wpedantic"
    #pragma GCC diagnostic ignored "-Wfloat-equal"
    #pragma GCC diagnostic ignored "-Wmissing-braces"
    #ifdef __clang__
        #pragma GCC diagnostic ignored "-Wunsafe-buffer-usage"
        #pragma GCC diagnostic ignored "-Wgnu-anonymous-struct"
        #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    #endif
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define Rr_DEPRECATED(msg) __attribute__((deprecated(msg)))
#elif defined(_MSC_VER)
    #define Rr_DEPRECATED(msg) __declspec(deprecated(msg))
#else
    #define Rr_DEPRECATED(msg)
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#if !defined(RR_MATH_USE_DEGREES) && !defined(RR_MATH_USE_TURNS) \
    && !defined(RR_MATH_USE_RADIANS)
    #define RR_MATH_USE_RADIANS
#endif

#define RR_PI 3.14159265358979323846
#define RR_PI32 3.14159265359f
#define RR_DEG180 180.0
#define RR_DEG18032 180.0f
#define RR_TURNHALF 0.5
#define RR_TURNHALF32 0.5f

#define Rr_RadToDeg (Rr_StaticCast(Rr_F32, RR_DEG180 / RR_PI))
#define Rr_RadToTurn (Rr_StaticCast(Rr_F32, RR_TURNHALF / RR_PI))
#define Rr_DegToRad (Rr_StaticCast(Rr_F32, RR_PI / RR_DEG180))
#define Rr_DegToTurn (Rr_StaticCast(Rr_F32, RR_TURNHALF / RR_DEG180))
#define Rr_TurnToRad (Rr_StaticCast(Rr_F32, RR_PI / RR_TURNHALF))
#define Rr_TurnToDeg (Rr_StaticCast(Rr_F32, RR_DEG180 / RR_TURNHALF))

#if defined(RR_MATH_USE_RADIANS)
    #define Rr_AngleRad(a) (a)
    #define Rr_AngleDeg(a) ((a) * Rr_DegToRad)
    #define Rr_AngleTurn(a) ((a) * Rr_TurnToRad)
#elif defined(RR_MATH_USE_DEGREES)
    #define Rr_AngleRad(a) ((a) * Rr_RadToDeg)
    #define Rr_AngleDeg(a) (a)
    #define Rr_AngleTurn(a) ((a) * Rr_TurnToDeg)
#elif defined(RR_MATH_USE_TURNS)
    #define Rr_AngleRad(a) ((a) * Rr_RadToTurn)
    #define Rr_AngleDeg(a) ((a) * Rr_DegToTurn)
    #define Rr_AngleTurn(a) (a)
#endif

#if !defined(RR_MATH_PROVIDE_MATH_FUNCTIONS)
    #include <math.h>
    #define Rr_SINF sinf
    #define Rr_COSF cosf
    #define Rr_TANF tanf
    #define Rr_SQRTF sqrtf
    #define Rr_ACOSF acosf
#endif

#if !defined(Rr_ANGLE_USER_TO_INTERNAL)
    #define Rr_ANGLE_USER_TO_INTERNAL(a) (Rr_ToRad(a))
#endif

#if !defined(Rr_ANGLE_INTERNAL_TO_USER)
    #if defined(RR_MATH_USE_RADIANS)
        #define Rr_ANGLE_INTERNAL_TO_USER(a) (a)
    #elif defined(RR_MATH_USE_DEGREES)
        #define Rr_ANGLE_INTERNAL_TO_USER(a) ((a) * Rr_RadToDeg)
    #elif defined(RR_MATH_USE_TURNS)
        #define Rr_ANGLE_INTERNAL_TO_USER(a) ((a) * Rr_RadToTurn)
    #endif
#endif

#define Rr_MIN(a, b) ((a) > (b) ? (b) : (a))
#define Rr_MAX(a, b) ((a) < (b) ? (b) : (a))
#define Rr_ABS(a) ((a) > 0 ? (a) : -(a))
#define Rr_MOD(a, m) (((a) % (m)) >= 0 ? ((a) % (m)) : (((a) % (m)) + (m)))
#define Rr_SQUARE(x) ((x) * (x))

typedef union Rr_Vec2
{
    struct
    {
        Rr_F32 X, Y;
    };

    struct
    {
        Rr_F32 U, V;
    };

    struct
    {
        Rr_F32 Left, Right;
    };

    struct
    {
        Rr_F32 Width, Height;
    };

    Rr_F32 Elements[2];

#ifdef __cplusplus
    inline Rr_F32&
    operator[](Rr_I32 Index)
    {
        return Elements[Index];
    }
    inline const Rr_F32&
    operator[](Rr_I32 Index) const
    {
        return Elements[Index];
    }
#endif
} Rr_Vec2;

typedef union Rr_Vec3
{
    struct
    {
        Rr_F32 X, Y, Z;
    };

    struct
    {
        Rr_F32 U, V, W;
    };

    struct
    {
        Rr_F32 R, G, B;
    };

    struct
    {
        Rr_Vec2 XY;
        Rr_F32 _Ignored0;
    };

    struct
    {
        Rr_F32 _Ignored1;
        Rr_Vec2 YZ;
    };

    struct
    {
        Rr_Vec2 UV;
        Rr_F32 _Ignored2;
    };

    struct
    {
        Rr_F32 _Ignored3;
        Rr_Vec2 VW;
    };

    Rr_F32 Elements[3];

#ifdef __cplusplus
    inline Rr_F32&
    operator[](Rr_I32 Index)
    {
        return Elements[Index];
    }
    inline const Rr_F32&
    operator[](Rr_I32 Index) const
    {
        return Elements[Index];
    }
#endif
} Rr_Vec3;

typedef union Rr_Vec4
{
    struct
    {
        union
        {
            Rr_Vec3 XYZ;
            struct
            {
                Rr_F32 X, Y, Z;
            };
        };

        Rr_F32 W;
    };
    struct
    {
        union
        {
            Rr_Vec3 RGB;
            struct
            {
                Rr_F32 R, G, B;
            };
        };

        Rr_F32 A;
    };

    struct
    {
        Rr_Vec2 XY;
        Rr_F32 _Ignored0;
        Rr_F32 _Ignored1;
    };

    struct
    {
        Rr_F32 _Ignored2;
        Rr_Vec2 YZ;
        Rr_F32 _Ignored3;
    };

    struct
    {
        Rr_F32 _Ignored4;
        Rr_F32 _Ignored5;
        Rr_Vec2 ZW;
    };

    Rr_F32 Elements[4];

#ifdef RR_MATH__USE_SSE
    __m128 SSE;
#endif

#ifdef RR_MATH__USE_NEON
    Rr_F3232x4_t NEON;
#endif

#ifdef __cplusplus
    inline Rr_F32&
    operator[](Rr_I32 Index)
    {
        return Elements[Index];
    }
    inline const Rr_F32&
    operator[](Rr_I32 Index) const
    {
        return Elements[Index];
    }
#endif
} Rr_Vec4;

typedef union Rr_IntVec2
{
    struct
    {
        Rr_I32 X, Y;
    };

    struct
    {
        Rr_I32 U, V;
    };

    struct
    {
        Rr_I32 Left, Right;
    };

    struct
    {
        Rr_I32 Width, Height;
    };

    Rr_I32 Elements[2];

#ifdef __cplusplus
    inline Rr_I32&
    operator[](Rr_I32 Index)
    {
        return Elements[Index];
    }
    inline const Rr_I32&
    operator[](Rr_I32 Index) const
    {
        return Elements[Index];
    }
#endif
} Rr_IntVec2;

typedef union Rr_IntVec3
{
    struct
    {
        Rr_I32 X, Y, Z;
    };

    struct
    {
        Rr_I32 U, V, W;
    };

    struct
    {
        Rr_I32 R, G, B;
    };

    struct
    {
        Rr_IntVec2 XY;
        Rr_I32 _Ignored0;
    };

    struct
    {
        Rr_I32 _Ignored1;
        Rr_Vec2 YZ;
    };

    struct
    {
        Rr_IntVec2 UV;
        Rr_I32 _Ignored2;
    };

    struct
    {
        Rr_I32 _Ignored3;
        Rr_IntVec2 VW;
    };

    Rr_I32 Elements[3];

#ifdef __cplusplus
    inline Rr_I32&
    operator[](Rr_I32 Index)
    {
        return Elements[Index];
    }
    inline const Rr_I32&
    operator[](Rr_I32 Index) const
    {
        return Elements[Index];
    }
#endif
} Rr_IntVec3;

typedef union Rr_IntVec4
{
    struct
    {
        union
        {
            Rr_IntVec3 XYZ;
            struct
            {
                struct
                {
                    Rr_I32 X, Y;
                };
                union
                {
                    Rr_I32 Z;
                    Rr_I32 Width;
                };
            };
        };

        union
        {
            Rr_I32 W;
            Rr_I32 Height;
        };
    };
    struct
    {
        union
        {
            Rr_IntVec3 RGB;
            struct
            {
                Rr_I32 R, G, B;
            };
        };

        Rr_I32 A;
    };

    struct
    {
        Rr_IntVec2 XY;
        Rr_I32 _Ignored0;
        Rr_I32 _Ignored1;
    };

    struct
    {
        Rr_I32 _Ignored2;
        Rr_IntVec2 YZ;
        Rr_I32 _Ignored3;
    };

    struct
    {
        Rr_I32 _Ignored4;
        Rr_I32 _Ignored5;
        Rr_IntVec2 ZW;
    };

    Rr_I32 Elements[4];

#ifdef __cplusplus
    inline Rr_I32&
    operator[](Rr_I32 Index)
    {
        return Elements[Index];
    }
    inline const Rr_I32&
    operator[](Rr_I32 Index) const
    {
        return Elements[Index];
    }
#endif
} Rr_IntVec4;

typedef union Rr_Mat2
{
    Rr_F32 Elements[2][2];
    Rr_Vec2 Columns[2];

#ifdef __cplusplus
    inline Rr_Vec2&
    operator[](Rr_I32 Index)
    {
        return Columns[Index];
    }
    inline const Rr_Vec2&
    operator[](Rr_I32 Index) const
    {
        return Columns[Index];
    }
#endif
} Rr_Mat2;

typedef union Rr_Mat3
{
    Rr_F32 Elements[3][3];
    Rr_Vec3 Columns[3];

#ifdef __cplusplus
    inline Rr_Vec3&
    operator[](Rr_I32 Index)
    {
        return Columns[Index];
    }
    inline const Rr_Vec3&
    operator[](Rr_I32 Index) const
    {
        return Columns[Index];
    }
#endif
} Rr_Mat3;

typedef union Rr_Mat4
{
    Rr_F32 Elements[4][4];
    Rr_Vec4 Columns[4];

#ifdef __cplusplus
    inline Rr_Vec4&
    operator[](Rr_I32 Index)
    {
        return Columns[Index];
    }
    inline const Rr_Vec4&
    operator[](Rr_I32 Index) const
    {
        return Columns[Index];
    }
#endif
} Rr_Mat4;

typedef union Rr_Quat
{
    struct
    {
        union
        {
            Rr_Vec3 XYZ;
            struct
            {
                Rr_F32 X, Y, Z;
            };
        };

        Rr_F32 W;
    };

    Rr_F32 Elements[4];

#ifdef RR_MATH__USE_SSE
    __m128 SSE;
#endif
#ifdef RR_MATH__USE_NEON
    Rr_F3232x4_t NEON;
#endif
} Rr_Quat;

/*
 * Angle unit conversion functions
 */
static inline Rr_F32
Rr_ToRad(Rr_F32 Angle)
{
#if defined(RR_MATH_USE_RADIANS)
    Rr_F32 Result = Angle;
#elif defined(RR_MATH_USE_DEGREES)
    Rr_F32 Result = Angle * Rr_DegToRad;
#elif defined(RR_MATH_USE_TURNS)
    Rr_F32 Result = Angle * Rr_TurnToRad;
#endif

    return Result;
}

static inline Rr_F32
Rr_ToDeg(Rr_F32 Angle)
{
#if defined(RR_MATH_USE_RADIANS)
    Rr_F32 Result = Angle * Rr_RadToDeg;
#elif defined(RR_MATH_USE_DEGREES)
    Rr_F32 Result = Angle;
#elif defined(RR_MATH_USE_TURNS)
    Rr_F32 Result = Angle * Rr_TurnToDeg;
#endif

    return Result;
}

static inline Rr_F32
Rr_ToTurn(Rr_F32 Angle)
{
#if defined(RR_MATH_USE_RADIANS)
    Rr_F32 Result = Angle * Rr_RadToTurn;
#elif defined(RR_MATH_USE_DEGREES)
    Rr_F32 Result = Angle * Rr_DegToTurn;
#elif defined(RR_MATH_USE_TURNS)
    Rr_F32 Result = Angle;
#endif

    return Result;
}

/*
 * Floating-point math functions
 */

COVERAGE(Rr_SinF, 1)
static inline Rr_F32
Rr_SinF(Rr_F32 Angle)
{
    ASSERT_COVERED(Rr_SinF);
    return Rr_SINF(Rr_ANGLE_USER_TO_INTERNAL(Angle));
}

COVERAGE(Rr_CosF, 1)
static inline Rr_F32
Rr_CosF(Rr_F32 Angle)
{
    ASSERT_COVERED(Rr_CosF);
    return Rr_COSF(Rr_ANGLE_USER_TO_INTERNAL(Angle));
}

COVERAGE(Rr_TanF, 1)
static inline Rr_F32
Rr_TanF(Rr_F32 Angle)
{
    ASSERT_COVERED(Rr_TanF);
    return Rr_TANF(Rr_ANGLE_USER_TO_INTERNAL(Angle));
}

COVERAGE(Rr_ACosF, 1)
static inline Rr_F32
Rr_ACosF(Rr_F32 Arg)
{
    ASSERT_COVERED(Rr_ACosF);
    return Rr_ANGLE_INTERNAL_TO_USER(Rr_ACOSF(Arg));
}

COVERAGE(Rr_SqrtF, 1)
static inline Rr_F32
Rr_SqrtF(Rr_F32 Float)
{
    ASSERT_COVERED(Rr_SqrtF);

    Rr_F32 Result;

#ifdef RR_MATH__USE_SSE
    __m128 In = _mm_set_ss(Float);
    __m128 Out = _mm_sqrt_ss(In);
    Result = _mm_cvtss_f32(Out);
#elif defined(RR_MATH__USE_NEON)
    Rr_F3232x4_t In = vdupq_n_f32(Float);
    Rr_F3232x4_t Out = vsqrtq_f32(In);
    Result = vgetq_lane_f32(Out, 0);
#else
    Result = Rr_SQRTF(Float);
#endif

    return Result;
}

COVERAGE(Rr_InvSqrtF, 1)
static inline Rr_F32
Rr_InvSqrtF(Rr_F32 Float)
{
    ASSERT_COVERED(Rr_InvSqrtF);

    Rr_F32 Result;

    Result = 1.0f / Rr_SqrtF(Float);

    return Result;
}

/*
 * RrFramework functions
 */

static inline Rr_F32
Rr_GetVerticalFoV(const Rr_F32 HorizontalFoV, const Rr_F32 Aspect)
{
    return 2.0f * atanf((tanf(HorizontalFoV / 2.0f) * Aspect));
}

static inline void
Rr_PerspectiveResize(Rr_F32 Aspect, Rr_Mat4* Proj)
{
    if (Proj->Elements[0][0] == 0.0f)
    {
        return;
    }

    Proj->Elements[0][0] = Proj->Elements[1][1] / Aspect;
}

static inline Rr_Mat4
Rr_VulkanMatrix()
{
    Rr_Mat4 Out = { 0 };
    Out.Elements[0][0] = 1.0f;
    Out.Elements[1][1] = -1.0f;
    Out.Elements[2][2] = -1.0f;
    Out.Elements[3][3] = 1.0f;
    return Out;
}

static inline Rr_Mat4
Rr_EulerXYZ(Rr_Vec3 Angles)
{
    Rr_F32 CosX, CosY, CosZ, SinX, SinY, SinZ, CosZSinX, CosXCosZ, SinYSinZ;

    Rr_Mat4 Out;

    SinX = Rr_SinF(Angles.X);
    CosX = Rr_CosF(Angles.X);
    SinY = Rr_SinF(Angles.Y);
    CosY = Rr_CosF(Angles.Y);
    SinZ = Rr_SinF(Angles.Z);
    CosZ = Rr_CosF(Angles.Z);

    CosZSinX = CosZ * SinX;
    CosXCosZ = CosX * CosZ;
    SinYSinZ = SinY * SinZ;

    Out.Elements[0][0] = CosY * CosZ;
    Out.Elements[0][1] = CosZSinX * SinY + CosX * SinZ;
    Out.Elements[0][2] = -CosXCosZ * SinY + SinX * SinZ;
    Out.Elements[1][0] = -CosY * SinZ;
    Out.Elements[1][1] = CosXCosZ - SinX * SinYSinZ;
    Out.Elements[1][2] = CosZSinX + CosX * SinYSinZ;
    Out.Elements[2][0] = SinY;
    Out.Elements[2][1] = -CosY * SinX;
    Out.Elements[2][2] = CosX * CosY;
    Out.Elements[0][3] = 0.0f;
    Out.Elements[1][3] = 0.0f;
    Out.Elements[2][3] = 0.0f;
    Out.Elements[3][0] = 0.0f;
    Out.Elements[3][1] = 0.0f;
    Out.Elements[3][2] = 0.0f;
    Out.Elements[3][3] = 1.0f;

    return Out;
}

/*
 * Utility functions
 */

COVERAGE(Rr_Lerp, 1)
static inline Rr_F32
Rr_Lerp(Rr_F32 A, Rr_F32 Time, Rr_F32 B)
{
    ASSERT_COVERED(Rr_Lerp);
    return (1.0f - Time) * A + Time * B;
}

COVERAGE(Rr_Clamp, 1)
static inline Rr_F32
Rr_Clamp(Rr_F32 Min, Rr_F32 Value, Rr_F32 Max)
{
    ASSERT_COVERED(Rr_Clamp);

    Rr_F32 Result = Value;

    if (Result < Min)
    {
        Result = Min;
    }

    if (Result > Max)
    {
        Result = Max;
    }

    return Result;
}

/*
 * Vector initialization
 */

COVERAGE(Rr_V2, 1)
static inline Rr_Vec2
Rr_V2(Rr_F32 X, Rr_F32 Y)
{
    ASSERT_COVERED(Rr_V2);

    Rr_Vec2 Result;
    Result.X = X;
    Result.Y = Y;

    return Result;
}

COVERAGE(Rr_V3, 1)
static inline Rr_Vec3
Rr_V3(Rr_F32 X, Rr_F32 Y, Rr_F32 Z)
{
    ASSERT_COVERED(Rr_V3);

    Rr_Vec3 Result;
    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;

    return Result;
}

COVERAGE(Rr_V4, 1)
static inline Rr_Vec4
Rr_V4(Rr_F32 X, Rr_F32 Y, Rr_F32 Z, Rr_F32 W)
{
    ASSERT_COVERED(Rr_V4);

    Rr_Vec4 Result;

#ifdef RR_MATH__USE_SSE
    Result.SSE = _mm_setr_ps(X, Y, Z, W);
#elif defined(RR_MATH__USE_NEON)
    Rr_F3232x4_t v = { X, Y, Z, W };
    Result.NEON = v;
#else
    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;
    Result.W = W;
#endif

    return Result;
}

COVERAGE(Rr_V4V, 1)
static inline Rr_Vec4
Rr_V4V(Rr_Vec3 Vector, Rr_F32 W)
{
    ASSERT_COVERED(Rr_V4V);

    Rr_Vec4 Result;

#ifdef RR_MATH__USE_SSE
    Result.SSE = _mm_setr_ps(Vector.X, Vector.Y, Vector.Z, W);
#elif defined(RR_MATH__USE_NEON)
    Rr_F3232x4_t v = { Vector.X, Vector.Y, Vector.Z, W };
    Result.NEON = v;
#else
    Result.XYZ = Vector;
    Result.W = W;
#endif

    return Result;
}

/*
 * Binary vector operations
 */

COVERAGE(Rr_AddV2, 1)
static inline Rr_Vec2
Rr_AddV2(Rr_Vec2 Left, Rr_Vec2 Right)
{
    ASSERT_COVERED(Rr_AddV2);

    Rr_Vec2 Result;
    Result.X = Left.X + Right.X;
    Result.Y = Left.Y + Right.Y;

    return Result;
}

COVERAGE(Rr_AddV3, 1)
static inline Rr_Vec3
Rr_AddV3(Rr_Vec3 Left, Rr_Vec3 Right)
{
    ASSERT_COVERED(Rr_AddV3);

    Rr_Vec3 Result;
    Result.X = Left.X + Right.X;
    Result.Y = Left.Y + Right.Y;
    Result.Z = Left.Z + Right.Z;

    return Result;
}

COVERAGE(Rr_AddV4, 1)
static inline Rr_Vec4
Rr_AddV4(Rr_Vec4 Left, Rr_Vec4 Right)
{
    ASSERT_COVERED(Rr_AddV4);

    Rr_Vec4 Result;

#ifdef RR_MATH__USE_SSE
    Result.SSE = _mm_add_ps(Left.SSE, Right.SSE);
#elif defined(RR_MATH__USE_NEON)
    Result.NEON = vaddq_f32(Left.NEON, Right.NEON);
#else
    Result.X = Left.X + Right.X;
    Result.Y = Left.Y + Right.Y;
    Result.Z = Left.Z + Right.Z;
    Result.W = Left.W + Right.W;
#endif

    return Result;
}

COVERAGE(Rr_SubV2, 1)
static inline Rr_Vec2
Rr_SubV2(Rr_Vec2 Left, Rr_Vec2 Right)
{
    ASSERT_COVERED(Rr_SubV2);

    Rr_Vec2 Result;
    Result.X = Left.X - Right.X;
    Result.Y = Left.Y - Right.Y;

    return Result;
}

COVERAGE(Rr_SubV3, 1)
static inline Rr_Vec3
Rr_SubV3(Rr_Vec3 Left, Rr_Vec3 Right)
{
    ASSERT_COVERED(Rr_SubV3);

    Rr_Vec3 Result;
    Result.X = Left.X - Right.X;
    Result.Y = Left.Y - Right.Y;
    Result.Z = Left.Z - Right.Z;

    return Result;
}

COVERAGE(Rr_SubV4, 1)
static inline Rr_Vec4
Rr_SubV4(Rr_Vec4 Left, Rr_Vec4 Right)
{
    ASSERT_COVERED(Rr_SubV4);

    Rr_Vec4 Result;

#ifdef RR_MATH__USE_SSE
    Result.SSE = _mm_sub_ps(Left.SSE, Right.SSE);
#elif defined(RR_MATH__USE_NEON)
    Result.NEON = vsubq_f32(Left.NEON, Right.NEON);
#else
    Result.X = Left.X - Right.X;
    Result.Y = Left.Y - Right.Y;
    Result.Z = Left.Z - Right.Z;
    Result.W = Left.W - Right.W;
#endif

    return Result;
}

COVERAGE(Rr_MulV2, 1)
static inline Rr_Vec2
Rr_MulV2(Rr_Vec2 Left, Rr_Vec2 Right)
{
    ASSERT_COVERED(Rr_MulV2);

    Rr_Vec2 Result;
    Result.X = Left.X * Right.X;
    Result.Y = Left.Y * Right.Y;

    return Result;
}

COVERAGE(Rr_MulV2F, 1)
static inline Rr_Vec2
Rr_MulV2F(Rr_Vec2 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_MulV2F);

    Rr_Vec2 Result;
    Result.X = Left.X * Right;
    Result.Y = Left.Y * Right;

    return Result;
}

COVERAGE(Rr_MulV3, 1)
static inline Rr_Vec3
Rr_MulV3(Rr_Vec3 Left, Rr_Vec3 Right)
{
    ASSERT_COVERED(Rr_MulV3);

    Rr_Vec3 Result;
    Result.X = Left.X * Right.X;
    Result.Y = Left.Y * Right.Y;
    Result.Z = Left.Z * Right.Z;

    return Result;
}

COVERAGE(Rr_MulV3F, 1)
static inline Rr_Vec3
Rr_MulV3F(Rr_Vec3 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_MulV3F);

    Rr_Vec3 Result;
    Result.X = Left.X * Right;
    Result.Y = Left.Y * Right;
    Result.Z = Left.Z * Right;

    return Result;
}

COVERAGE(Rr_MulV4, 1)
static inline Rr_Vec4
Rr_MulV4(Rr_Vec4 Left, Rr_Vec4 Right)
{
    ASSERT_COVERED(Rr_MulV4);

    Rr_Vec4 Result;

#ifdef RR_MATH__USE_SSE
    Result.SSE = _mm_mul_ps(Left.SSE, Right.SSE);
#elif defined(RR_MATH__USE_NEON)
    Result.NEON = vmulq_f32(Left.NEON, Right.NEON);
#else
    Result.X = Left.X * Right.X;
    Result.Y = Left.Y * Right.Y;
    Result.Z = Left.Z * Right.Z;
    Result.W = Left.W * Right.W;
#endif

    return Result;
}

COVERAGE(Rr_MulV4F, 1)
static inline Rr_Vec4
Rr_MulV4F(Rr_Vec4 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_MulV4F);

    Rr_Vec4 Result;

#ifdef RR_MATH__USE_SSE
    __m128 Scalar = _mm_set1_ps(Right);
    Result.SSE = _mm_mul_ps(Left.SSE, Scalar);
#elif defined(RR_MATH__USE_NEON)
    Result.NEON = vmulq_n_f32(Left.NEON, Right);
#else
    Result.X = Left.X * Right;
    Result.Y = Left.Y * Right;
    Result.Z = Left.Z * Right;
    Result.W = Left.W * Right;
#endif

    return Result;
}

COVERAGE(Rr_DivV2, 1)
static inline Rr_Vec2
Rr_DivV2(Rr_Vec2 Left, Rr_Vec2 Right)
{
    ASSERT_COVERED(Rr_DivV2);

    Rr_Vec2 Result;
    Result.X = Left.X / Right.X;
    Result.Y = Left.Y / Right.Y;

    return Result;
}

COVERAGE(Rr_DivV2F, 1)
static inline Rr_Vec2
Rr_DivV2F(Rr_Vec2 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_DivV2F);

    Rr_Vec2 Result;
    Result.X = Left.X / Right;
    Result.Y = Left.Y / Right;

    return Result;
}

COVERAGE(Rr_DivV3, 1)
static inline Rr_Vec3
Rr_DivV3(Rr_Vec3 Left, Rr_Vec3 Right)
{
    ASSERT_COVERED(Rr_DivV3);

    Rr_Vec3 Result;
    Result.X = Left.X / Right.X;
    Result.Y = Left.Y / Right.Y;
    Result.Z = Left.Z / Right.Z;

    return Result;
}

COVERAGE(Rr_DivV3F, 1)
static inline Rr_Vec3
Rr_DivV3F(Rr_Vec3 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_DivV3F);

    Rr_Vec3 Result;
    Result.X = Left.X / Right;
    Result.Y = Left.Y / Right;
    Result.Z = Left.Z / Right;

    return Result;
}

COVERAGE(Rr_DivV4, 1)
static inline Rr_Vec4
Rr_DivV4(Rr_Vec4 Left, Rr_Vec4 Right)
{
    ASSERT_COVERED(Rr_DivV4);

    Rr_Vec4 Result;

#ifdef RR_MATH__USE_SSE
    Result.SSE = _mm_div_ps(Left.SSE, Right.SSE);
#elif defined(RR_MATH__USE_NEON)
    Result.NEON = vdivq_f32(Left.NEON, Right.NEON);
#else
    Result.X = Left.X / Right.X;
    Result.Y = Left.Y / Right.Y;
    Result.Z = Left.Z / Right.Z;
    Result.W = Left.W / Right.W;
#endif

    return Result;
}

COVERAGE(Rr_DivV4F, 1)
static inline Rr_Vec4
Rr_DivV4F(Rr_Vec4 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_DivV4F);

    Rr_Vec4 Result;

#ifdef RR_MATH__USE_SSE
    __m128 Scalar = _mm_set1_ps(Right);
    Result.SSE = _mm_div_ps(Left.SSE, Scalar);
#elif defined(RR_MATH__USE_NEON)
    Rr_F3232x4_t Scalar = vdupq_n_f32(Right);
    Result.NEON = vdivq_f32(Left.NEON, Scalar);
#else
    Result.X = Left.X / Right;
    Result.Y = Left.Y / Right;
    Result.Z = Left.Z / Right;
    Result.W = Left.W / Right;
#endif

    return Result;
}

COVERAGE(Rr_EqV2, 1)
static inline Rr_Bool
Rr_EqV2(Rr_Vec2 Left, Rr_Vec2 Right)
{
    ASSERT_COVERED(Rr_EqV2);
    return Left.X == Right.X && Left.Y == Right.Y;
}

COVERAGE(Rr_EqV3, 1)
static inline Rr_Bool
Rr_EqV3(Rr_Vec3 Left, Rr_Vec3 Right)
{
    ASSERT_COVERED(Rr_EqV3);
    return Left.X == Right.X && Left.Y == Right.Y && Left.Z == Right.Z;
}

COVERAGE(Rr_EqV4, 1)
static inline Rr_Bool
Rr_EqV4(Rr_Vec4 Left, Rr_Vec4 Right)
{
    ASSERT_COVERED(Rr_EqV4);
    return Left.X == Right.X && Left.Y == Right.Y && Left.Z == Right.Z
        && Left.W == Right.W;
}

static inline Rr_Bool
Rr_EqIV3(Rr_IntVec3 Left, Rr_IntVec3 Right)
{
    return Left.X == Right.X && Left.Y == Right.Y && Left.Z == Right.Z;
}

COVERAGE(Rr_DotV2, 1)
static inline Rr_F32
Rr_DotV2(Rr_Vec2 Left, Rr_Vec2 Right)
{
    ASSERT_COVERED(Rr_DotV2);
    return (Left.X * Right.X) + (Left.Y * Right.Y);
}

COVERAGE(Rr_DotV3, 1)
static inline Rr_F32
Rr_DotV3(Rr_Vec3 Left, Rr_Vec3 Right)
{
    ASSERT_COVERED(Rr_DotV3);
    return (Left.X * Right.X) + (Left.Y * Right.Y) + (Left.Z * Right.Z);
}

COVERAGE(Rr_DotV4, 1)
static inline Rr_F32
Rr_DotV4(Rr_Vec4 Left, Rr_Vec4 Right)
{
    ASSERT_COVERED(Rr_DotV4);

    Rr_F32 Result;

    // NOTE(zak): IN the future if we wanna check what version SSE is support
    // we can use _mm_dp_ps (4.3) but for now we will use the old way.
    // Or a r = _mm_mul_ps(v1, v2), r = _mm_hadd_ps(r, r), r = _mm_hadd_ps(r, r)
    // for SSE3
#ifdef RR_MATH__USE_SSE
    __m128 SSEResultOne = _mm_mul_ps(Left.SSE, Right.SSE);
    __m128 SSEResultTwo =
        _mm_shuffle_ps(SSEResultOne, SSEResultOne, _MM_SHUFFLE(2, 3, 0, 1));
    SSEResultOne = _mm_add_ps(SSEResultOne, SSEResultTwo);
    SSEResultTwo =
        _mm_shuffle_ps(SSEResultOne, SSEResultOne, _MM_SHUFFLE(0, 1, 2, 3));
    SSEResultOne = _mm_add_ps(SSEResultOne, SSEResultTwo);
    _mm_store_ss(&Result, SSEResultOne);
#elif defined(RR_MATH__USE_NEON)
    Rr_F3232x4_t NEONMultiplyResult = vmulq_f32(Left.NEON, Right.NEON);
    Rr_F3232x4_t NEONHalfAdd =
        vpaddq_f32(NEONMultiplyResult, NEONMultiplyResult);
    Rr_F3232x4_t NEONFullAdd = vpaddq_f32(NEONHalfAdd, NEONHalfAdd);
    Result = vgetq_lane_f32(NEONFullAdd, 0);
#else
    Result = ((Left.X * Right.X) + (Left.Z * Right.Z))
        + ((Left.Y * Right.Y) + (Left.W * Right.W));
#endif

    return Result;
}

COVERAGE(Rr_Cross, 1)
static inline Rr_Vec3
Rr_Cross(Rr_Vec3 Left, Rr_Vec3 Right)
{
    ASSERT_COVERED(Rr_Cross);

    Rr_Vec3 Result;
    Result.X = (Left.Y * Right.Z) - (Left.Z * Right.Y);
    Result.Y = (Left.Z * Right.X) - (Left.X * Right.Z);
    Result.Z = (Left.X * Right.Y) - (Left.Y * Right.X);

    return Result;
}

/*
 * Unary vector operations
 */

COVERAGE(Rr_LenSqrV2, 1)
static inline Rr_F32
Rr_LenSqrV2(Rr_Vec2 A)
{
    ASSERT_COVERED(Rr_LenSqrV2);
    return Rr_DotV2(A, A);
}

COVERAGE(Rr_LenSqrV3, 1)
static inline Rr_F32
Rr_LenSqrV3(Rr_Vec3 A)
{
    ASSERT_COVERED(Rr_LenSqrV3);
    return Rr_DotV3(A, A);
}

COVERAGE(Rr_LenSqrV4, 1)
static inline Rr_F32
Rr_LenSqrV4(Rr_Vec4 A)
{
    ASSERT_COVERED(Rr_LenSqrV4);
    return Rr_DotV4(A, A);
}

COVERAGE(Rr_LenV2, 1)
static inline Rr_F32
Rr_LenV2(Rr_Vec2 A)
{
    ASSERT_COVERED(Rr_LenV2);
    return Rr_SqrtF(Rr_LenSqrV2(A));
}

COVERAGE(Rr_LenV3, 1)
static inline Rr_F32
Rr_LenV3(Rr_Vec3 A)
{
    ASSERT_COVERED(Rr_LenV3);
    return Rr_SqrtF(Rr_LenSqrV3(A));
}

COVERAGE(Rr_LenV4, 1)
static inline Rr_F32
Rr_LenV4(Rr_Vec4 A)
{
    ASSERT_COVERED(Rr_LenV4);
    return Rr_SqrtF(Rr_LenSqrV4(A));
}

COVERAGE(Rr_NormV2, 1)
static inline Rr_Vec2
Rr_NormV2(Rr_Vec2 A)
{
    ASSERT_COVERED(Rr_NormV2);
    return Rr_MulV2F(A, Rr_InvSqrtF(Rr_DotV2(A, A)));
}

COVERAGE(Rr_NormV3, 1)
static inline Rr_Vec3
Rr_NormV3(Rr_Vec3 A)
{
    ASSERT_COVERED(Rr_NormV3);
    return Rr_MulV3F(A, Rr_InvSqrtF(Rr_DotV3(A, A)));
}

COVERAGE(Rr_NormV4, 1)
static inline Rr_Vec4
Rr_NormV4(Rr_Vec4 A)
{
    ASSERT_COVERED(Rr_NormV4);
    return Rr_MulV4F(A, Rr_InvSqrtF(Rr_DotV4(A, A)));
}

/*
 * Utility vector functions
 */

COVERAGE(Rr_LerpV2, 1)
static inline Rr_Vec2
Rr_LerpV2(Rr_Vec2 A, Rr_F32 Time, Rr_Vec2 B)
{
    ASSERT_COVERED(Rr_LerpV2);
    return Rr_AddV2(Rr_MulV2F(A, 1.0f - Time), Rr_MulV2F(B, Time));
}

COVERAGE(Rr_LerpV3, 1)
static inline Rr_Vec3
Rr_LerpV3(Rr_Vec3 A, Rr_F32 Time, Rr_Vec3 B)
{
    ASSERT_COVERED(Rr_LerpV3);
    return Rr_AddV3(Rr_MulV3F(A, 1.0f - Time), Rr_MulV3F(B, Time));
}

COVERAGE(Rr_LerpV4, 1)
static inline Rr_Vec4
Rr_LerpV4(Rr_Vec4 A, Rr_F32 Time, Rr_Vec4 B)
{
    ASSERT_COVERED(Rr_LerpV4);
    return Rr_AddV4(Rr_MulV4F(A, 1.0f - Time), Rr_MulV4F(B, Time));
}

/*
 * SSE stuff
 */

COVERAGE(Rr_LinearCombineV4M4, 1)
static inline Rr_Vec4
Rr_LinearCombineV4M4(Rr_Vec4 Left, Rr_Mat4 Right)
{
    ASSERT_COVERED(Rr_LinearCombineV4M4);

    Rr_Vec4 Result;
#ifdef RR_MATH__USE_SSE
    Result.SSE = _mm_mul_ps(
        _mm_shuffle_ps(Left.SSE, Left.SSE, 0x00), Right.Columns[0].SSE);
    Result.SSE = _mm_add_ps(
        Result.SSE,
        _mm_mul_ps(
            _mm_shuffle_ps(Left.SSE, Left.SSE, 0x55), Right.Columns[1].SSE));
    Result.SSE = _mm_add_ps(
        Result.SSE,
        _mm_mul_ps(
            _mm_shuffle_ps(Left.SSE, Left.SSE, 0xaa), Right.Columns[2].SSE));
    Result.SSE = _mm_add_ps(
        Result.SSE,
        _mm_mul_ps(
            _mm_shuffle_ps(Left.SSE, Left.SSE, 0xff), Right.Columns[3].SSE));
#elif defined(RR_MATH__USE_NEON)
    Result.NEON = vmulq_laneq_f32(Right.Columns[0].NEON, Left.NEON, 0);
    Result.NEON =
        vfmaq_laneq_f32(Result.NEON, Right.Columns[1].NEON, Left.NEON, 1);
    Result.NEON =
        vfmaq_laneq_f32(Result.NEON, Right.Columns[2].NEON, Left.NEON, 2);
    Result.NEON =
        vfmaq_laneq_f32(Result.NEON, Right.Columns[3].NEON, Left.NEON, 3);
#else
    Result.X = Left.Elements[0] * Right.Columns[0].X;
    Result.Y = Left.Elements[0] * Right.Columns[0].Y;
    Result.Z = Left.Elements[0] * Right.Columns[0].Z;
    Result.W = Left.Elements[0] * Right.Columns[0].W;

    Result.X += Left.Elements[1] * Right.Columns[1].X;
    Result.Y += Left.Elements[1] * Right.Columns[1].Y;
    Result.Z += Left.Elements[1] * Right.Columns[1].Z;
    Result.W += Left.Elements[1] * Right.Columns[1].W;

    Result.X += Left.Elements[2] * Right.Columns[2].X;
    Result.Y += Left.Elements[2] * Right.Columns[2].Y;
    Result.Z += Left.Elements[2] * Right.Columns[2].Z;
    Result.W += Left.Elements[2] * Right.Columns[2].W;

    Result.X += Left.Elements[3] * Right.Columns[3].X;
    Result.Y += Left.Elements[3] * Right.Columns[3].Y;
    Result.Z += Left.Elements[3] * Right.Columns[3].Z;
    Result.W += Left.Elements[3] * Right.Columns[3].W;
#endif

    return Result;
}

/*
 * 2x2 Matrices
 */

COVERAGE(Rr_M2, 1)
static inline Rr_Mat2
Rr_M2(void)
{
    ASSERT_COVERED(Rr_M2);
    Rr_Mat2 Result = { 0 };
    return Result;
}

COVERAGE(Rr_M2D, 1)
static inline Rr_Mat2
Rr_M2D(Rr_F32 Diagonal)
{
    ASSERT_COVERED(Rr_M2D);

    Rr_Mat2 Result = { 0 };
    Result.Elements[0][0] = Diagonal;
    Result.Elements[1][1] = Diagonal;

    return Result;
}

COVERAGE(Rr_TransposeM2, 1)
static inline Rr_Mat2
Rr_TransposeM2(Rr_Mat2 Matrix)
{
    ASSERT_COVERED(Rr_TransposeM2);

    Rr_Mat2 Result = Matrix;

    Result.Elements[0][1] = Matrix.Elements[1][0];
    Result.Elements[1][0] = Matrix.Elements[0][1];

    return Result;
}

COVERAGE(Rr_AddM2, 1)
static inline Rr_Mat2
Rr_AddM2(Rr_Mat2 Left, Rr_Mat2 Right)
{
    ASSERT_COVERED(Rr_AddM2);

    Rr_Mat2 Result;

    Result.Elements[0][0] = Left.Elements[0][0] + Right.Elements[0][0];
    Result.Elements[0][1] = Left.Elements[0][1] + Right.Elements[0][1];
    Result.Elements[1][0] = Left.Elements[1][0] + Right.Elements[1][0];
    Result.Elements[1][1] = Left.Elements[1][1] + Right.Elements[1][1];

    return Result;
}

COVERAGE(Rr_SubM2, 1)
static inline Rr_Mat2
Rr_SubM2(Rr_Mat2 Left, Rr_Mat2 Right)
{
    ASSERT_COVERED(Rr_SubM2);

    Rr_Mat2 Result;

    Result.Elements[0][0] = Left.Elements[0][0] - Right.Elements[0][0];
    Result.Elements[0][1] = Left.Elements[0][1] - Right.Elements[0][1];
    Result.Elements[1][0] = Left.Elements[1][0] - Right.Elements[1][0];
    Result.Elements[1][1] = Left.Elements[1][1] - Right.Elements[1][1];

    return Result;
}

COVERAGE(Rr_MulM2V2, 1)
static inline Rr_Vec2
Rr_MulM2V2(Rr_Mat2 Matrix, Rr_Vec2 Vector)
{
    ASSERT_COVERED(Rr_MulM2V2);

    Rr_Vec2 Result;

    Result.X = Vector.Elements[0] * Matrix.Columns[0].X;
    Result.Y = Vector.Elements[0] * Matrix.Columns[0].Y;

    Result.X += Vector.Elements[1] * Matrix.Columns[1].X;
    Result.Y += Vector.Elements[1] * Matrix.Columns[1].Y;

    return Result;
}

COVERAGE(Rr_MulM2, 1)
static inline Rr_Mat2
Rr_MulM2(Rr_Mat2 Left, Rr_Mat2 Right)
{
    ASSERT_COVERED(Rr_MulM2);

    Rr_Mat2 Result;
    Result.Columns[0] = Rr_MulM2V2(Left, Right.Columns[0]);
    Result.Columns[1] = Rr_MulM2V2(Left, Right.Columns[1]);

    return Result;
}

COVERAGE(Rr_MulM2F, 1)
static inline Rr_Mat2
Rr_MulM2F(Rr_Mat2 Matrix, Rr_F32 Scalar)
{
    ASSERT_COVERED(Rr_MulM2F);

    Rr_Mat2 Result;

    Result.Elements[0][0] = Matrix.Elements[0][0] * Scalar;
    Result.Elements[0][1] = Matrix.Elements[0][1] * Scalar;
    Result.Elements[1][0] = Matrix.Elements[1][0] * Scalar;
    Result.Elements[1][1] = Matrix.Elements[1][1] * Scalar;

    return Result;
}

COVERAGE(Rr_DivM2F, 1)
static inline Rr_Mat2
Rr_DivM2F(Rr_Mat2 Matrix, Rr_F32 Scalar)
{
    ASSERT_COVERED(Rr_DivM2F);

    Rr_Mat2 Result;

    Result.Elements[0][0] = Matrix.Elements[0][0] / Scalar;
    Result.Elements[0][1] = Matrix.Elements[0][1] / Scalar;
    Result.Elements[1][0] = Matrix.Elements[1][0] / Scalar;
    Result.Elements[1][1] = Matrix.Elements[1][1] / Scalar;

    return Result;
}

COVERAGE(Rr_DeterminantM2, 1)
static inline Rr_F32
Rr_DeterminantM2(Rr_Mat2 Matrix)
{
    ASSERT_COVERED(Rr_DeterminantM2);
    return Matrix.Elements[0][0] * Matrix.Elements[1][1]
        - Matrix.Elements[0][1] * Matrix.Elements[1][0];
}

COVERAGE(Rr_InvGeneralM2, 1)
static inline Rr_Mat2
Rr_InvGeneralM2(Rr_Mat2 Matrix)
{
    ASSERT_COVERED(Rr_InvGeneralM2);

    Rr_Mat2 Result;
    Rr_F32 InvDeterminant = 1.0f / Rr_DeterminantM2(Matrix);
    Result.Elements[0][0] = InvDeterminant * +Matrix.Elements[1][1];
    Result.Elements[1][1] = InvDeterminant * +Matrix.Elements[0][0];
    Result.Elements[0][1] = InvDeterminant * -Matrix.Elements[0][1];
    Result.Elements[1][0] = InvDeterminant * -Matrix.Elements[1][0];

    return Result;
}

/*
 * 3x3 Matrices
 */

COVERAGE(Rr_M3, 1)
static inline Rr_Mat3
Rr_M3(void)
{
    ASSERT_COVERED(Rr_M3);
    Rr_Mat3 Result = { 0 };
    return Result;
}

COVERAGE(Rr_M3D, 1)
static inline Rr_Mat3
Rr_M3D(Rr_F32 Diagonal)
{
    ASSERT_COVERED(Rr_M3D);

    Rr_Mat3 Result = { 0 };
    Result.Elements[0][0] = Diagonal;
    Result.Elements[1][1] = Diagonal;
    Result.Elements[2][2] = Diagonal;

    return Result;
}

COVERAGE(Rr_TransposeM3, 1)
static inline Rr_Mat3
Rr_TransposeM3(Rr_Mat3 Matrix)
{
    ASSERT_COVERED(Rr_TransposeM3);

    Rr_Mat3 Result = Matrix;

    Result.Elements[0][1] = Matrix.Elements[1][0];
    Result.Elements[0][2] = Matrix.Elements[2][0];
    Result.Elements[1][0] = Matrix.Elements[0][1];
    Result.Elements[1][2] = Matrix.Elements[2][1];
    Result.Elements[2][1] = Matrix.Elements[1][2];
    Result.Elements[2][0] = Matrix.Elements[0][2];

    return Result;
}

COVERAGE(Rr_AddM3, 1)
static inline Rr_Mat3
Rr_AddM3(Rr_Mat3 Left, Rr_Mat3 Right)
{
    ASSERT_COVERED(Rr_AddM3);

    Rr_Mat3 Result;

    Result.Elements[0][0] = Left.Elements[0][0] + Right.Elements[0][0];
    Result.Elements[0][1] = Left.Elements[0][1] + Right.Elements[0][1];
    Result.Elements[0][2] = Left.Elements[0][2] + Right.Elements[0][2];
    Result.Elements[1][0] = Left.Elements[1][0] + Right.Elements[1][0];
    Result.Elements[1][1] = Left.Elements[1][1] + Right.Elements[1][1];
    Result.Elements[1][2] = Left.Elements[1][2] + Right.Elements[1][2];
    Result.Elements[2][0] = Left.Elements[2][0] + Right.Elements[2][0];
    Result.Elements[2][1] = Left.Elements[2][1] + Right.Elements[2][1];
    Result.Elements[2][2] = Left.Elements[2][2] + Right.Elements[2][2];

    return Result;
}

COVERAGE(Rr_SubM3, 1)
static inline Rr_Mat3
Rr_SubM3(Rr_Mat3 Left, Rr_Mat3 Right)
{
    ASSERT_COVERED(Rr_SubM3);

    Rr_Mat3 Result;

    Result.Elements[0][0] = Left.Elements[0][0] - Right.Elements[0][0];
    Result.Elements[0][1] = Left.Elements[0][1] - Right.Elements[0][1];
    Result.Elements[0][2] = Left.Elements[0][2] - Right.Elements[0][2];
    Result.Elements[1][0] = Left.Elements[1][0] - Right.Elements[1][0];
    Result.Elements[1][1] = Left.Elements[1][1] - Right.Elements[1][1];
    Result.Elements[1][2] = Left.Elements[1][2] - Right.Elements[1][2];
    Result.Elements[2][0] = Left.Elements[2][0] - Right.Elements[2][0];
    Result.Elements[2][1] = Left.Elements[2][1] - Right.Elements[2][1];
    Result.Elements[2][2] = Left.Elements[2][2] - Right.Elements[2][2];

    return Result;
}

COVERAGE(Rr_MulM3V3, 1)
static inline Rr_Vec3
Rr_MulM3V3(Rr_Mat3 Matrix, Rr_Vec3 Vector)
{
    ASSERT_COVERED(Rr_MulM3V3);

    Rr_Vec3 Result;

    Result.X = Vector.Elements[0] * Matrix.Columns[0].X;
    Result.Y = Vector.Elements[0] * Matrix.Columns[0].Y;
    Result.Z = Vector.Elements[0] * Matrix.Columns[0].Z;

    Result.X += Vector.Elements[1] * Matrix.Columns[1].X;
    Result.Y += Vector.Elements[1] * Matrix.Columns[1].Y;
    Result.Z += Vector.Elements[1] * Matrix.Columns[1].Z;

    Result.X += Vector.Elements[2] * Matrix.Columns[2].X;
    Result.Y += Vector.Elements[2] * Matrix.Columns[2].Y;
    Result.Z += Vector.Elements[2] * Matrix.Columns[2].Z;

    return Result;
}

COVERAGE(Rr_MulM3, 1)
static inline Rr_Mat3
Rr_MulM3(Rr_Mat3 Left, Rr_Mat3 Right)
{
    ASSERT_COVERED(Rr_MulM3);

    Rr_Mat3 Result;
    Result.Columns[0] = Rr_MulM3V3(Left, Right.Columns[0]);
    Result.Columns[1] = Rr_MulM3V3(Left, Right.Columns[1]);
    Result.Columns[2] = Rr_MulM3V3(Left, Right.Columns[2]);

    return Result;
}

COVERAGE(Rr_MulM3F, 1)
static inline Rr_Mat3
Rr_MulM3F(Rr_Mat3 Matrix, Rr_F32 Scalar)
{
    ASSERT_COVERED(Rr_MulM3F);

    Rr_Mat3 Result;

    Result.Elements[0][0] = Matrix.Elements[0][0] * Scalar;
    Result.Elements[0][1] = Matrix.Elements[0][1] * Scalar;
    Result.Elements[0][2] = Matrix.Elements[0][2] * Scalar;
    Result.Elements[1][0] = Matrix.Elements[1][0] * Scalar;
    Result.Elements[1][1] = Matrix.Elements[1][1] * Scalar;
    Result.Elements[1][2] = Matrix.Elements[1][2] * Scalar;
    Result.Elements[2][0] = Matrix.Elements[2][0] * Scalar;
    Result.Elements[2][1] = Matrix.Elements[2][1] * Scalar;
    Result.Elements[2][2] = Matrix.Elements[2][2] * Scalar;

    return Result;
}

COVERAGE(Rr_DivM3, 1)
static inline Rr_Mat3
Rr_DivM3F(Rr_Mat3 Matrix, Rr_F32 Scalar)
{
    ASSERT_COVERED(Rr_DivM3);

    Rr_Mat3 Result;

    Result.Elements[0][0] = Matrix.Elements[0][0] / Scalar;
    Result.Elements[0][1] = Matrix.Elements[0][1] / Scalar;
    Result.Elements[0][2] = Matrix.Elements[0][2] / Scalar;
    Result.Elements[1][0] = Matrix.Elements[1][0] / Scalar;
    Result.Elements[1][1] = Matrix.Elements[1][1] / Scalar;
    Result.Elements[1][2] = Matrix.Elements[1][2] / Scalar;
    Result.Elements[2][0] = Matrix.Elements[2][0] / Scalar;
    Result.Elements[2][1] = Matrix.Elements[2][1] / Scalar;
    Result.Elements[2][2] = Matrix.Elements[2][2] / Scalar;

    return Result;
}

COVERAGE(Rr_DeterminantM3, 1)
static inline Rr_F32
Rr_DeterminantM3(Rr_Mat3 Matrix)
{
    ASSERT_COVERED(Rr_DeterminantM3);

    Rr_Mat3 Cross;
    Cross.Columns[0] = Rr_Cross(Matrix.Columns[1], Matrix.Columns[2]);
    Cross.Columns[1] = Rr_Cross(Matrix.Columns[2], Matrix.Columns[0]);
    Cross.Columns[2] = Rr_Cross(Matrix.Columns[0], Matrix.Columns[1]);

    return Rr_DotV3(Cross.Columns[2], Matrix.Columns[2]);
}

COVERAGE(Rr_InvGeneralM3, 1)
static inline Rr_Mat3
Rr_InvGeneralM3(Rr_Mat3 Matrix)
{
    ASSERT_COVERED(Rr_InvGeneralM3);

    Rr_Mat3 Cross;
    Cross.Columns[0] = Rr_Cross(Matrix.Columns[1], Matrix.Columns[2]);
    Cross.Columns[1] = Rr_Cross(Matrix.Columns[2], Matrix.Columns[0]);
    Cross.Columns[2] = Rr_Cross(Matrix.Columns[0], Matrix.Columns[1]);

    Rr_F32 InvDeterminant =
        1.0f / Rr_DotV3(Cross.Columns[2], Matrix.Columns[2]);

    Rr_Mat3 Result;
    Result.Columns[0] = Rr_MulV3F(Cross.Columns[0], InvDeterminant);
    Result.Columns[1] = Rr_MulV3F(Cross.Columns[1], InvDeterminant);
    Result.Columns[2] = Rr_MulV3F(Cross.Columns[2], InvDeterminant);

    return Rr_TransposeM3(Result);
}

/*
 * 4x4 Matrices
 */

COVERAGE(Rr_M4, 1)
static inline Rr_Mat4
Rr_M4(void)
{
    ASSERT_COVERED(Rr_M4);
    Rr_Mat4 Result = { 0 };
    return Result;
}

COVERAGE(Rr_M4D, 1)
static inline Rr_Mat4
Rr_M4D(Rr_F32 Diagonal)
{
    ASSERT_COVERED(Rr_M4D);

    Rr_Mat4 Result = { 0 };
    Result.Elements[0][0] = Diagonal;
    Result.Elements[1][1] = Diagonal;
    Result.Elements[2][2] = Diagonal;
    Result.Elements[3][3] = Diagonal;

    return Result;
}

COVERAGE(Rr_TransposeM4, 1)
static inline Rr_Mat4
Rr_TransposeM4(Rr_Mat4 Matrix)
{
    ASSERT_COVERED(Rr_TransposeM4);

    Rr_Mat4 Result;
#ifdef RR_MATH__USE_SSE
    Result = Matrix;
    _MM_TRANSPOSE4_PS(
        Result.Columns[0].SSE,
        Result.Columns[1].SSE,
        Result.Columns[2].SSE,
        Result.Columns[3].SSE);
#elif defined(RR_MATH__USE_NEON)
    Rr_F3232x4x4_t Transposed = vld4q_f32((Rr_F32*)Matrix.Columns);
    Result.Columns[0].NEON = Transposed.val[0];
    Result.Columns[1].NEON = Transposed.val[1];
    Result.Columns[2].NEON = Transposed.val[2];
    Result.Columns[3].NEON = Transposed.val[3];
#else
    Result.Elements[0][0] = Matrix.Elements[0][0];
    Result.Elements[0][1] = Matrix.Elements[1][0];
    Result.Elements[0][2] = Matrix.Elements[2][0];
    Result.Elements[0][3] = Matrix.Elements[3][0];
    Result.Elements[1][0] = Matrix.Elements[0][1];
    Result.Elements[1][1] = Matrix.Elements[1][1];
    Result.Elements[1][2] = Matrix.Elements[2][1];
    Result.Elements[1][3] = Matrix.Elements[3][1];
    Result.Elements[2][0] = Matrix.Elements[0][2];
    Result.Elements[2][1] = Matrix.Elements[1][2];
    Result.Elements[2][2] = Matrix.Elements[2][2];
    Result.Elements[2][3] = Matrix.Elements[3][2];
    Result.Elements[3][0] = Matrix.Elements[0][3];
    Result.Elements[3][1] = Matrix.Elements[1][3];
    Result.Elements[3][2] = Matrix.Elements[2][3];
    Result.Elements[3][3] = Matrix.Elements[3][3];
#endif

    return Result;
}

COVERAGE(Rr_AddM4, 1)
static inline Rr_Mat4
Rr_AddM4(Rr_Mat4 Left, Rr_Mat4 Right)
{
    ASSERT_COVERED(Rr_AddM4);

    Rr_Mat4 Result;

    Result.Columns[0] = Rr_AddV4(Left.Columns[0], Right.Columns[0]);
    Result.Columns[1] = Rr_AddV4(Left.Columns[1], Right.Columns[1]);
    Result.Columns[2] = Rr_AddV4(Left.Columns[2], Right.Columns[2]);
    Result.Columns[3] = Rr_AddV4(Left.Columns[3], Right.Columns[3]);

    return Result;
}

COVERAGE(Rr_SubM4, 1)
static inline Rr_Mat4
Rr_SubM4(Rr_Mat4 Left, Rr_Mat4 Right)
{
    ASSERT_COVERED(Rr_SubM4);

    Rr_Mat4 Result;

    Result.Columns[0] = Rr_SubV4(Left.Columns[0], Right.Columns[0]);
    Result.Columns[1] = Rr_SubV4(Left.Columns[1], Right.Columns[1]);
    Result.Columns[2] = Rr_SubV4(Left.Columns[2], Right.Columns[2]);
    Result.Columns[3] = Rr_SubV4(Left.Columns[3], Right.Columns[3]);

    return Result;
}

COVERAGE(Rr_MulM4, 1)
static inline Rr_Mat4
Rr_MulM4(Rr_Mat4 Left, Rr_Mat4 Right)
{
    ASSERT_COVERED(Rr_MulM4);

    Rr_Mat4 Result;
    Result.Columns[0] = Rr_LinearCombineV4M4(Right.Columns[0], Left);
    Result.Columns[1] = Rr_LinearCombineV4M4(Right.Columns[1], Left);
    Result.Columns[2] = Rr_LinearCombineV4M4(Right.Columns[2], Left);
    Result.Columns[3] = Rr_LinearCombineV4M4(Right.Columns[3], Left);

    return Result;
}

COVERAGE(Rr_MulM4F, 1)
static inline Rr_Mat4
Rr_MulM4F(Rr_Mat4 Matrix, Rr_F32 Scalar)
{
    ASSERT_COVERED(Rr_MulM4F);

    Rr_Mat4 Result;

#ifdef RR_MATH__USE_SSE
    __m128 SSEScalar = _mm_set1_ps(Scalar);
    Result.Columns[0].SSE = _mm_mul_ps(Matrix.Columns[0].SSE, SSEScalar);
    Result.Columns[1].SSE = _mm_mul_ps(Matrix.Columns[1].SSE, SSEScalar);
    Result.Columns[2].SSE = _mm_mul_ps(Matrix.Columns[2].SSE, SSEScalar);
    Result.Columns[3].SSE = _mm_mul_ps(Matrix.Columns[3].SSE, SSEScalar);
#elif defined(RR_MATH__USE_NEON)
    Result.Columns[0].NEON = vmulq_n_f32(Matrix.Columns[0].NEON, Scalar);
    Result.Columns[1].NEON = vmulq_n_f32(Matrix.Columns[1].NEON, Scalar);
    Result.Columns[2].NEON = vmulq_n_f32(Matrix.Columns[2].NEON, Scalar);
    Result.Columns[3].NEON = vmulq_n_f32(Matrix.Columns[3].NEON, Scalar);
#else
    Result.Elements[0][0] = Matrix.Elements[0][0] * Scalar;
    Result.Elements[0][1] = Matrix.Elements[0][1] * Scalar;
    Result.Elements[0][2] = Matrix.Elements[0][2] * Scalar;
    Result.Elements[0][3] = Matrix.Elements[0][3] * Scalar;
    Result.Elements[1][0] = Matrix.Elements[1][0] * Scalar;
    Result.Elements[1][1] = Matrix.Elements[1][1] * Scalar;
    Result.Elements[1][2] = Matrix.Elements[1][2] * Scalar;
    Result.Elements[1][3] = Matrix.Elements[1][3] * Scalar;
    Result.Elements[2][0] = Matrix.Elements[2][0] * Scalar;
    Result.Elements[2][1] = Matrix.Elements[2][1] * Scalar;
    Result.Elements[2][2] = Matrix.Elements[2][2] * Scalar;
    Result.Elements[2][3] = Matrix.Elements[2][3] * Scalar;
    Result.Elements[3][0] = Matrix.Elements[3][0] * Scalar;
    Result.Elements[3][1] = Matrix.Elements[3][1] * Scalar;
    Result.Elements[3][2] = Matrix.Elements[3][2] * Scalar;
    Result.Elements[3][3] = Matrix.Elements[3][3] * Scalar;
#endif

    return Result;
}

COVERAGE(Rr_MulM4V4, 1)
static inline Rr_Vec4
Rr_MulM4V4(Rr_Mat4 Matrix, Rr_Vec4 Vector)
{
    ASSERT_COVERED(Rr_MulM4V4);
    return Rr_LinearCombineV4M4(Vector, Matrix);
}

COVERAGE(Rr_DivM4F, 1)
static inline Rr_Mat4
Rr_DivM4F(Rr_Mat4 Matrix, Rr_F32 Scalar)
{
    ASSERT_COVERED(Rr_DivM4F);

    Rr_Mat4 Result;

#ifdef RR_MATH__USE_SSE
    __m128 SSEScalar = _mm_set1_ps(Scalar);
    Result.Columns[0].SSE = _mm_div_ps(Matrix.Columns[0].SSE, SSEScalar);
    Result.Columns[1].SSE = _mm_div_ps(Matrix.Columns[1].SSE, SSEScalar);
    Result.Columns[2].SSE = _mm_div_ps(Matrix.Columns[2].SSE, SSEScalar);
    Result.Columns[3].SSE = _mm_div_ps(Matrix.Columns[3].SSE, SSEScalar);
#elif defined(RR_MATH__USE_NEON)
    Rr_F3232x4_t NEONScalar = vdupq_n_f32(Scalar);
    Result.Columns[0].NEON = vdivq_f32(Matrix.Columns[0].NEON, NEONScalar);
    Result.Columns[1].NEON = vdivq_f32(Matrix.Columns[1].NEON, NEONScalar);
    Result.Columns[2].NEON = vdivq_f32(Matrix.Columns[2].NEON, NEONScalar);
    Result.Columns[3].NEON = vdivq_f32(Matrix.Columns[3].NEON, NEONScalar);
#else
    Result.Elements[0][0] = Matrix.Elements[0][0] / Scalar;
    Result.Elements[0][1] = Matrix.Elements[0][1] / Scalar;
    Result.Elements[0][2] = Matrix.Elements[0][2] / Scalar;
    Result.Elements[0][3] = Matrix.Elements[0][3] / Scalar;
    Result.Elements[1][0] = Matrix.Elements[1][0] / Scalar;
    Result.Elements[1][1] = Matrix.Elements[1][1] / Scalar;
    Result.Elements[1][2] = Matrix.Elements[1][2] / Scalar;
    Result.Elements[1][3] = Matrix.Elements[1][3] / Scalar;
    Result.Elements[2][0] = Matrix.Elements[2][0] / Scalar;
    Result.Elements[2][1] = Matrix.Elements[2][1] / Scalar;
    Result.Elements[2][2] = Matrix.Elements[2][2] / Scalar;
    Result.Elements[2][3] = Matrix.Elements[2][3] / Scalar;
    Result.Elements[3][0] = Matrix.Elements[3][0] / Scalar;
    Result.Elements[3][1] = Matrix.Elements[3][1] / Scalar;
    Result.Elements[3][2] = Matrix.Elements[3][2] / Scalar;
    Result.Elements[3][3] = Matrix.Elements[3][3] / Scalar;
#endif

    return Result;
}

COVERAGE(Rr_DeterminantM4, 1)
static inline Rr_F32
Rr_DeterminantM4(Rr_Mat4 Matrix)
{
    ASSERT_COVERED(Rr_DeterminantM4);

    Rr_Vec3 C01 = Rr_Cross(Matrix.Columns[0].XYZ, Matrix.Columns[1].XYZ);
    Rr_Vec3 C23 = Rr_Cross(Matrix.Columns[2].XYZ, Matrix.Columns[3].XYZ);
    Rr_Vec3 B10 = Rr_SubV3(
        Rr_MulV3F(Matrix.Columns[0].XYZ, Matrix.Columns[1].W),
        Rr_MulV3F(Matrix.Columns[1].XYZ, Matrix.Columns[0].W));
    Rr_Vec3 B32 = Rr_SubV3(
        Rr_MulV3F(Matrix.Columns[2].XYZ, Matrix.Columns[3].W),
        Rr_MulV3F(Matrix.Columns[3].XYZ, Matrix.Columns[2].W));

    return Rr_DotV3(C01, B32) + Rr_DotV3(C23, B10);
}

COVERAGE(Rr_InvGeneralM4, 1)
// Returns a general-purpose inverse of an Rr_Mat4. Note that special-purpose
// inverses of many transformations are available and will be more efficient.
static inline Rr_Mat4
Rr_InvGeneralM4(Rr_Mat4 Matrix)
{
    ASSERT_COVERED(Rr_InvGeneralM4);

    Rr_Vec3 C01 = Rr_Cross(Matrix.Columns[0].XYZ, Matrix.Columns[1].XYZ);
    Rr_Vec3 C23 = Rr_Cross(Matrix.Columns[2].XYZ, Matrix.Columns[3].XYZ);
    Rr_Vec3 B10 = Rr_SubV3(
        Rr_MulV3F(Matrix.Columns[0].XYZ, Matrix.Columns[1].W),
        Rr_MulV3F(Matrix.Columns[1].XYZ, Matrix.Columns[0].W));
    Rr_Vec3 B32 = Rr_SubV3(
        Rr_MulV3F(Matrix.Columns[2].XYZ, Matrix.Columns[3].W),
        Rr_MulV3F(Matrix.Columns[3].XYZ, Matrix.Columns[2].W));

    Rr_F32 InvDeterminant = 1.0f / (Rr_DotV3(C01, B32) + Rr_DotV3(C23, B10));
    C01 = Rr_MulV3F(C01, InvDeterminant);
    C23 = Rr_MulV3F(C23, InvDeterminant);
    B10 = Rr_MulV3F(B10, InvDeterminant);
    B32 = Rr_MulV3F(B32, InvDeterminant);

    Rr_Mat4 Result;
    Result.Columns[0] = Rr_V4V(
        Rr_AddV3(
            Rr_Cross(Matrix.Columns[1].XYZ, B32),
            Rr_MulV3F(C23, Matrix.Columns[1].W)),
        -Rr_DotV3(Matrix.Columns[1].XYZ, C23));
    Result.Columns[1] = Rr_V4V(
        Rr_SubV3(
            Rr_Cross(B32, Matrix.Columns[0].XYZ),
            Rr_MulV3F(C23, Matrix.Columns[0].W)),
        +Rr_DotV3(Matrix.Columns[0].XYZ, C23));
    Result.Columns[2] = Rr_V4V(
        Rr_AddV3(
            Rr_Cross(Matrix.Columns[3].XYZ, B10),
            Rr_MulV3F(C01, Matrix.Columns[3].W)),
        -Rr_DotV3(Matrix.Columns[3].XYZ, C01));
    Result.Columns[3] = Rr_V4V(
        Rr_SubV3(
            Rr_Cross(B10, Matrix.Columns[2].XYZ),
            Rr_MulV3F(C01, Matrix.Columns[2].W)),
        +Rr_DotV3(Matrix.Columns[2].XYZ, C01));

    return Rr_TransposeM4(Result);
}

/*
 * Common graphics transformations
 */

COVERAGE(Rr_Orthographic_RH_NO, 1)
// Produces a right-handed orthographic projection matrix with Z ranging from -1
// to 1 (the GL convention). Left, Right, Bottom, and Top specify the
// coordinates of their respective clipping planes. Near and Far specify the
// distances to the near and far clipping planes.
static inline Rr_Mat4
Rr_Orthographic_RH_NO(
    Rr_F32 Left,
    Rr_F32 Right,
    Rr_F32 Bottom,
    Rr_F32 Top,
    Rr_F32 Near,
    Rr_F32 Far)
{
    ASSERT_COVERED(Rr_Orthographic_RH_NO);

    Rr_Mat4 Result = { 0 };

    Result.Elements[0][0] = 2.0f / (Right - Left);
    Result.Elements[1][1] = 2.0f / (Top - Bottom);
    Result.Elements[2][2] = 2.0f / (Near - Far);
    Result.Elements[3][3] = 1.0f;

    Result.Elements[3][0] = (Left + Right) / (Left - Right);
    Result.Elements[3][1] = (Bottom + Top) / (Bottom - Top);
    Result.Elements[3][2] = (Near + Far) / (Near - Far);

    return Result;
}

COVERAGE(Rr_Orthographic_RH_ZO, 1)
// Produces a right-handed orthographic projection matrix with Z ranging from 0
// to 1 (the DirectX convention). Left, Right, Bottom, and Top specify the
// coordinates of their respective clipping planes. Near and Far specify the
// distances to the near and far clipping planes.
static inline Rr_Mat4
Rr_Orthographic_RH_ZO(
    Rr_F32 Left,
    Rr_F32 Right,
    Rr_F32 Bottom,
    Rr_F32 Top,
    Rr_F32 Near,
    Rr_F32 Far)
{
    ASSERT_COVERED(Rr_Orthographic_RH_ZO);

    Rr_Mat4 Result = { 0 };

    Result.Elements[0][0] = 2.0f / (Right - Left);
    Result.Elements[1][1] = 2.0f / (Top - Bottom);
    Result.Elements[2][2] = 1.0f / (Near - Far);
    Result.Elements[3][3] = 1.0f;

    Result.Elements[3][0] = (Left + Right) / (Left - Right);
    Result.Elements[3][1] = (Bottom + Top) / (Bottom - Top);
    Result.Elements[3][2] = (Near) / (Near - Far);

    return Result;
}

COVERAGE(Rr_Orthographic_LH_NO, 1)
// Produces a left-handed orthographic projection matrix with Z ranging from -1
// to 1 (the GL convention). Left, Right, Bottom, and Top specify the
// coordinates of their respective clipping planes. Near and Far specify the
// distances to the near and far clipping planes.
static inline Rr_Mat4
Rr_Orthographic_LH_NO(
    Rr_F32 Left,
    Rr_F32 Right,
    Rr_F32 Bottom,
    Rr_F32 Top,
    Rr_F32 Near,
    Rr_F32 Far)
{
    ASSERT_COVERED(Rr_Orthographic_LH_NO);

    Rr_Mat4 Result = Rr_Orthographic_RH_NO(Left, Right, Bottom, Top, Near, Far);
    Result.Elements[2][2] = -Result.Elements[2][2];

    return Result;
}

COVERAGE(Rr_Orthographic_LH_ZO, 1)
// Produces a left-handed orthographic projection matrix with Z ranging from 0
// to 1 (the DirectX convention). Left, Right, Bottom, and Top specify the
// coordinates of their respective clipping planes. Near and Far specify the
// distances to the near and far clipping planes.
static inline Rr_Mat4
Rr_Orthographic_LH_ZO(
    Rr_F32 Left,
    Rr_F32 Right,
    Rr_F32 Bottom,
    Rr_F32 Top,
    Rr_F32 Near,
    Rr_F32 Far)
{
    ASSERT_COVERED(Rr_Orthographic_LH_ZO);

    Rr_Mat4 Result = Rr_Orthographic_RH_ZO(Left, Right, Bottom, Top, Near, Far);
    Result.Elements[2][2] = -Result.Elements[2][2];

    return Result;
}

COVERAGE(Rr_InvOrthographic, 1)
// Returns an inverse for the given orthographic projection matrix. Works for
// all orthographic projection matrices, regardless of handedness or NDC
// convention.
static inline Rr_Mat4
Rr_InvOrthographic(Rr_Mat4 OrthoMatrix)
{
    ASSERT_COVERED(Rr_InvOrthographic);

    Rr_Mat4 Result = { 0 };
    Result.Elements[0][0] = 1.0f / OrthoMatrix.Elements[0][0];
    Result.Elements[1][1] = 1.0f / OrthoMatrix.Elements[1][1];
    Result.Elements[2][2] = 1.0f / OrthoMatrix.Elements[2][2];
    Result.Elements[3][3] = 1.0f;

    Result.Elements[3][0] = -OrthoMatrix.Elements[3][0] * Result.Elements[0][0];
    Result.Elements[3][1] = -OrthoMatrix.Elements[3][1] * Result.Elements[1][1];
    Result.Elements[3][2] = -OrthoMatrix.Elements[3][2] * Result.Elements[2][2];

    return Result;
}

COVERAGE(Rr_Perspective_RH_NO, 1)
static inline Rr_Mat4
Rr_Perspective_RH_NO(Rr_F32 FOV, Rr_F32 AspectRatio, Rr_F32 Near, Rr_F32 Far)
{
    ASSERT_COVERED(Rr_Perspective_RH_NO);

    Rr_Mat4 Result = { 0 };

    // See
    // https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/gluPerspective.xml

    Rr_F32 Cotangent = 1.0f / Rr_TanF(FOV / 2.0f);
    Result.Elements[0][0] = Cotangent / AspectRatio;
    Result.Elements[1][1] = Cotangent;
    Result.Elements[2][3] = -1.0f;

    Result.Elements[2][2] = (Near + Far) / (Near - Far);
    Result.Elements[3][2] = (2.0f * Near * Far) / (Near - Far);

    return Result;
}

COVERAGE(Rr_Perspective_RH_ZO, 1)
static inline Rr_Mat4
Rr_Perspective_RH_ZO(Rr_F32 FOV, Rr_F32 AspectRatio, Rr_F32 Near, Rr_F32 Far)
{
    ASSERT_COVERED(Rr_Perspective_RH_ZO);

    Rr_Mat4 Result = { 0 };

    // See
    // https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/gluPerspective.xml

    Rr_F32 Cotangent = 1.0f / Rr_TanF(FOV / 2.0f);
    Result.Elements[0][0] = Cotangent / AspectRatio;
    Result.Elements[1][1] = Cotangent;
    Result.Elements[2][3] = -1.0f;

    Result.Elements[2][2] = (Far) / (Near - Far);
    Result.Elements[3][2] = (Near * Far) / (Near - Far);

    return Result;
}

COVERAGE(Rr_Perspective_LH_NO, 1)
static inline Rr_Mat4
Rr_Perspective_LH_NO(Rr_F32 FOV, Rr_F32 AspectRatio, Rr_F32 Near, Rr_F32 Far)
{
    ASSERT_COVERED(Rr_Perspective_LH_NO);

    Rr_Mat4 Result = Rr_Perspective_RH_NO(FOV, AspectRatio, Near, Far);
    Result.Elements[2][2] = -Result.Elements[2][2];
    Result.Elements[2][3] = -Result.Elements[2][3];

    return Result;
}

COVERAGE(Rr_Perspective_LH_ZO, 1)
static inline Rr_Mat4
Rr_Perspective_LH_ZO(Rr_F32 FOV, Rr_F32 AspectRatio, Rr_F32 Near, Rr_F32 Far)
{
    ASSERT_COVERED(Rr_Perspective_LH_ZO);

    Rr_Mat4 Result = Rr_Perspective_RH_ZO(FOV, AspectRatio, Near, Far);
    Result.Elements[2][2] = -Result.Elements[2][2];
    Result.Elements[2][3] = -Result.Elements[2][3];

    return Result;
}

COVERAGE(Rr_InvPerspective_RH, 1)
static inline Rr_Mat4
Rr_InvPerspective_RH(Rr_Mat4 PerspectiveMatrix)
{
    ASSERT_COVERED(Rr_InvPerspective_RH);

    Rr_Mat4 Result = { 0 };
    Result.Elements[0][0] = 1.0f / PerspectiveMatrix.Elements[0][0];
    Result.Elements[1][1] = 1.0f / PerspectiveMatrix.Elements[1][1];
    Result.Elements[2][2] = 0.0f;

    Result.Elements[2][3] = 1.0f / PerspectiveMatrix.Elements[3][2];
    Result.Elements[3][3] =
        PerspectiveMatrix.Elements[2][2] * Result.Elements[2][3];
    Result.Elements[3][2] = PerspectiveMatrix.Elements[2][3];

    return Result;
}

COVERAGE(Rr_InvPerspective_LH, 1)
static inline Rr_Mat4
Rr_InvPerspective_LH(Rr_Mat4 PerspectiveMatrix)
{
    ASSERT_COVERED(Rr_InvPerspective_LH);

    Rr_Mat4 Result = { 0 };
    Result.Elements[0][0] = 1.0f / PerspectiveMatrix.Elements[0][0];
    Result.Elements[1][1] = 1.0f / PerspectiveMatrix.Elements[1][1];
    Result.Elements[2][2] = 0.0f;

    Result.Elements[2][3] = 1.0f / PerspectiveMatrix.Elements[3][2];
    Result.Elements[3][3] =
        PerspectiveMatrix.Elements[2][2] * -Result.Elements[2][3];
    Result.Elements[3][2] = PerspectiveMatrix.Elements[2][3];

    return Result;
}

COVERAGE(Rr_Translate, 1)
static inline Rr_Mat4
Rr_Translate(Rr_Vec3 Translation)
{
    ASSERT_COVERED(Rr_Translate);

    Rr_Mat4 Result = Rr_M4D(1.0f);
    Result.Elements[3][0] = Translation.X;
    Result.Elements[3][1] = Translation.Y;
    Result.Elements[3][2] = Translation.Z;

    return Result;
}

COVERAGE(Rr_InvTranslate, 1)
static inline Rr_Mat4
Rr_InvTranslate(Rr_Mat4 TranslationMatrix)
{
    ASSERT_COVERED(Rr_InvTranslate);

    Rr_Mat4 Result = TranslationMatrix;
    Result.Elements[3][0] = -Result.Elements[3][0];
    Result.Elements[3][1] = -Result.Elements[3][1];
    Result.Elements[3][2] = -Result.Elements[3][2];

    return Result;
}

COVERAGE(Rr_Rotate_RH, 1)
static inline Rr_Mat4
Rr_Rotate_RH(Rr_F32 Angle, Rr_Vec3 Axis)
{
    ASSERT_COVERED(Rr_Rotate_RH);

    Rr_Mat4 Result = Rr_M4D(1.0f);

    Axis = Rr_NormV3(Axis);

    Rr_F32 SinTheta = Rr_SinF(Angle);
    Rr_F32 CosTheta = Rr_CosF(Angle);
    Rr_F32 CosValue = 1.0f - CosTheta;

    Result.Elements[0][0] = (Axis.X * Axis.X * CosValue) + CosTheta;
    Result.Elements[0][1] = (Axis.X * Axis.Y * CosValue) + (Axis.Z * SinTheta);
    Result.Elements[0][2] = (Axis.X * Axis.Z * CosValue) - (Axis.Y * SinTheta);

    Result.Elements[1][0] = (Axis.Y * Axis.X * CosValue) - (Axis.Z * SinTheta);
    Result.Elements[1][1] = (Axis.Y * Axis.Y * CosValue) + CosTheta;
    Result.Elements[1][2] = (Axis.Y * Axis.Z * CosValue) + (Axis.X * SinTheta);

    Result.Elements[2][0] = (Axis.Z * Axis.X * CosValue) + (Axis.Y * SinTheta);
    Result.Elements[2][1] = (Axis.Z * Axis.Y * CosValue) - (Axis.X * SinTheta);
    Result.Elements[2][2] = (Axis.Z * Axis.Z * CosValue) + CosTheta;

    return Result;
}

COVERAGE(Rr_Rotate_LH, 1)
static inline Rr_Mat4
Rr_Rotate_LH(Rr_F32 Angle, Rr_Vec3 Axis)
{
    ASSERT_COVERED(Rr_Rotate_LH);
    /* NOTE(lcf): Matrix will be inverse/transpose of RH. */
    return Rr_Rotate_RH(-Angle, Axis);
}

COVERAGE(Rr_InvRotate, 1)
static inline Rr_Mat4
Rr_InvRotate(Rr_Mat4 RotationMatrix)
{
    ASSERT_COVERED(Rr_InvRotate);
    return Rr_TransposeM4(RotationMatrix);
}

COVERAGE(Rr_Scale, 1)
static inline Rr_Mat4
Rr_Scale(Rr_Vec3 Scale)
{
    ASSERT_COVERED(Rr_Scale);

    Rr_Mat4 Result = Rr_M4D(1.0f);
    Result.Elements[0][0] = Scale.X;
    Result.Elements[1][1] = Scale.Y;
    Result.Elements[2][2] = Scale.Z;

    return Result;
}

COVERAGE(Rr_InvScale, 1)
static inline Rr_Mat4
Rr_InvScale(Rr_Mat4 ScaleMatrix)
{
    ASSERT_COVERED(Rr_InvScale);

    Rr_Mat4 Result = ScaleMatrix;
    Result.Elements[0][0] = 1.0f / Result.Elements[0][0];
    Result.Elements[1][1] = 1.0f / Result.Elements[1][1];
    Result.Elements[2][2] = 1.0f / Result.Elements[2][2];

    return Result;
}

static inline Rr_Mat4
_Rr_LookAt(Rr_Vec3 F, Rr_Vec3 S, Rr_Vec3 U, Rr_Vec3 Eye)
{
    Rr_Mat4 Result;

    Result.Elements[0][0] = S.X;
    Result.Elements[0][1] = U.X;
    Result.Elements[0][2] = -F.X;
    Result.Elements[0][3] = 0.0f;

    Result.Elements[1][0] = S.Y;
    Result.Elements[1][1] = U.Y;
    Result.Elements[1][2] = -F.Y;
    Result.Elements[1][3] = 0.0f;

    Result.Elements[2][0] = S.Z;
    Result.Elements[2][1] = U.Z;
    Result.Elements[2][2] = -F.Z;
    Result.Elements[2][3] = 0.0f;

    Result.Elements[3][0] = -Rr_DotV3(S, Eye);
    Result.Elements[3][1] = -Rr_DotV3(U, Eye);
    Result.Elements[3][2] = Rr_DotV3(F, Eye);
    Result.Elements[3][3] = 1.0f;

    return Result;
}

COVERAGE(Rr_LookAt_RH, 1)
static inline Rr_Mat4
Rr_LookAt_RH(Rr_Vec3 Eye, Rr_Vec3 Center, Rr_Vec3 Up)
{
    ASSERT_COVERED(Rr_LookAt_RH);

    Rr_Vec3 F = Rr_NormV3(Rr_SubV3(Center, Eye));
    Rr_Vec3 S = Rr_NormV3(Rr_Cross(F, Up));
    Rr_Vec3 U = Rr_Cross(S, F);

    return _Rr_LookAt(F, S, U, Eye);
}

COVERAGE(Rr_LookAt_LH, 1)
static inline Rr_Mat4
Rr_LookAt_LH(Rr_Vec3 Eye, Rr_Vec3 Center, Rr_Vec3 Up)
{
    ASSERT_COVERED(Rr_LookAt_LH);

    Rr_Vec3 F = Rr_NormV3(Rr_SubV3(Eye, Center));
    Rr_Vec3 S = Rr_NormV3(Rr_Cross(F, Up));
    Rr_Vec3 U = Rr_Cross(S, F);

    return _Rr_LookAt(F, S, U, Eye);
}

COVERAGE(Rr_InvLookAt, 1)
static inline Rr_Mat4
Rr_InvLookAt(Rr_Mat4 Matrix)
{
    ASSERT_COVERED(Rr_InvLookAt);
    Rr_Mat4 Result;

    Rr_Mat3 Rotation = { 0 };
    Rotation.Columns[0] = Matrix.Columns[0].XYZ;
    Rotation.Columns[1] = Matrix.Columns[1].XYZ;
    Rotation.Columns[2] = Matrix.Columns[2].XYZ;
    Rotation = Rr_TransposeM3(Rotation);

    Result.Columns[0] = Rr_V4V(Rotation.Columns[0], 0.0f);
    Result.Columns[1] = Rr_V4V(Rotation.Columns[1], 0.0f);
    Result.Columns[2] = Rr_V4V(Rotation.Columns[2], 0.0f);
    Result.Columns[3] = Rr_MulV4F(Matrix.Columns[3], -1.0f);
    Result.Elements[3][0] = -1.0f * Matrix.Elements[3][0]
        / (Rotation.Elements[0][0] + Rotation.Elements[0][1]
           + Rotation.Elements[0][2]);
    Result.Elements[3][1] = -1.0f * Matrix.Elements[3][1]
        / (Rotation.Elements[1][0] + Rotation.Elements[1][1]
           + Rotation.Elements[1][2]);
    Result.Elements[3][2] = -1.0f * Matrix.Elements[3][2]
        / (Rotation.Elements[2][0] + Rotation.Elements[2][1]
           + Rotation.Elements[2][2]);
    Result.Elements[3][3] = 1.0f;

    return Result;
}

/*
 * Quaternion operations
 */

COVERAGE(Rr_Q, 1)
static inline Rr_Quat
Rr_Q(Rr_F32 X, Rr_F32 Y, Rr_F32 Z, Rr_F32 W)
{
    ASSERT_COVERED(Rr_Q);

    Rr_Quat Result;

#ifdef RR_MATH__USE_SSE
    Result.SSE = _mm_setr_ps(X, Y, Z, W);
#elif defined(RR_MATH__USE_NEON)
    Rr_F3232x4_t v = { X, Y, Z, W };
    Result.NEON = v;
#else
    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;
    Result.W = W;
#endif

    return Result;
}

COVERAGE(Rr_QV4, 1)
static inline Rr_Quat
Rr_QV4(Rr_Vec4 Vector)
{
    ASSERT_COVERED(Rr_QV4);

    Rr_Quat Result;

#ifdef RR_MATH__USE_SSE
    Result.SSE = Vector.SSE;
#elif defined(RR_MATH__USE_NEON)
    Result.NEON = Vector.NEON;
#else
    Result.X = Vector.X;
    Result.Y = Vector.Y;
    Result.Z = Vector.Z;
    Result.W = Vector.W;
#endif

    return Result;
}

COVERAGE(Rr_AddQ, 1)
static inline Rr_Quat
Rr_AddQ(Rr_Quat Left, Rr_Quat Right)
{
    ASSERT_COVERED(Rr_AddQ);

    Rr_Quat Result;

#ifdef RR_MATH__USE_SSE
    Result.SSE = _mm_add_ps(Left.SSE, Right.SSE);
#elif defined(RR_MATH__USE_NEON)
    Result.NEON = vaddq_f32(Left.NEON, Right.NEON);
#else

    Result.X = Left.X + Right.X;
    Result.Y = Left.Y + Right.Y;
    Result.Z = Left.Z + Right.Z;
    Result.W = Left.W + Right.W;
#endif

    return Result;
}

COVERAGE(Rr_SubQ, 1)
static inline Rr_Quat
Rr_SubQ(Rr_Quat Left, Rr_Quat Right)
{
    ASSERT_COVERED(Rr_SubQ);

    Rr_Quat Result;

#ifdef RR_MATH__USE_SSE
    Result.SSE = _mm_sub_ps(Left.SSE, Right.SSE);
#elif defined(RR_MATH__USE_NEON)
    Result.NEON = vsubq_f32(Left.NEON, Right.NEON);
#else
    Result.X = Left.X - Right.X;
    Result.Y = Left.Y - Right.Y;
    Result.Z = Left.Z - Right.Z;
    Result.W = Left.W - Right.W;
#endif

    return Result;
}

COVERAGE(Rr_MulQ, 1)
static inline Rr_Quat
Rr_MulQ(Rr_Quat Left, Rr_Quat Right)
{
    ASSERT_COVERED(Rr_MulQ);

    Rr_Quat Result;

#ifdef RR_MATH__USE_SSE
    __m128 SSEResultOne = _mm_xor_ps(
        _mm_shuffle_ps(Left.SSE, Left.SSE, _MM_SHUFFLE(0, 0, 0, 0)),
        _mm_setr_ps(0.f, -0.f, 0.f, -0.f));
    __m128 SSEResultTwo =
        _mm_shuffle_ps(Right.SSE, Right.SSE, _MM_SHUFFLE(0, 1, 2, 3));
    __m128 SSEResultThree = _mm_mul_ps(SSEResultTwo, SSEResultOne);

    SSEResultOne = _mm_xor_ps(
        _mm_shuffle_ps(Left.SSE, Left.SSE, _MM_SHUFFLE(1, 1, 1, 1)),
        _mm_setr_ps(0.f, 0.f, -0.f, -0.f));
    SSEResultTwo =
        _mm_shuffle_ps(Right.SSE, Right.SSE, _MM_SHUFFLE(1, 0, 3, 2));
    SSEResultThree =
        _mm_add_ps(SSEResultThree, _mm_mul_ps(SSEResultTwo, SSEResultOne));

    SSEResultOne = _mm_xor_ps(
        _mm_shuffle_ps(Left.SSE, Left.SSE, _MM_SHUFFLE(2, 2, 2, 2)),
        _mm_setr_ps(-0.f, 0.f, 0.f, -0.f));
    SSEResultTwo =
        _mm_shuffle_ps(Right.SSE, Right.SSE, _MM_SHUFFLE(2, 3, 0, 1));
    SSEResultThree =
        _mm_add_ps(SSEResultThree, _mm_mul_ps(SSEResultTwo, SSEResultOne));

    SSEResultOne = _mm_shuffle_ps(Left.SSE, Left.SSE, _MM_SHUFFLE(3, 3, 3, 3));
    SSEResultTwo =
        _mm_shuffle_ps(Right.SSE, Right.SSE, _MM_SHUFFLE(3, 2, 1, 0));
    Result.SSE =
        _mm_add_ps(SSEResultThree, _mm_mul_ps(SSEResultTwo, SSEResultOne));
#elif defined(RR_MATH__USE_NEON)
    Rr_F3232x4_t Right1032 = vrev64q_f32(Right.NEON);
    Rr_F3232x4_t Right3210 =
        vcombine_f32(vget_high_f32(Right1032), vget_low_f32(Right1032));
    Rr_F3232x4_t Right2301 = vrev64q_f32(Right3210);

    Rr_F3232x4_t FirstSign = { 1.0f, -1.0f, 1.0f, -1.0f };
    Result.NEON = vmulq_f32(
        Right3210, vmulq_f32(vdupq_laneq_f32(Left.NEON, 0), FirstSign));
    Rr_F3232x4_t SecondSign = { 1.0f, 1.0f, -1.0f, -1.0f };
    Result.NEON = vfmaq_f32(
        Result.NEON,
        Right2301,
        vmulq_f32(vdupq_laneq_f32(Left.NEON, 1), SecondSign));
    Rr_F3232x4_t ThirdSign = { -1.0f, 1.0f, 1.0f, -1.0f };
    Result.NEON = vfmaq_f32(
        Result.NEON,
        Right1032,
        vmulq_f32(vdupq_laneq_f32(Left.NEON, 2), ThirdSign));
    Result.NEON = vfmaq_laneq_f32(Result.NEON, Right.NEON, Left.NEON, 3);

#else
    Result.X = Right.Elements[3] * +Left.Elements[0];
    Result.Y = Right.Elements[2] * -Left.Elements[0];
    Result.Z = Right.Elements[1] * +Left.Elements[0];
    Result.W = Right.Elements[0] * -Left.Elements[0];

    Result.X += Right.Elements[2] * +Left.Elements[1];
    Result.Y += Right.Elements[3] * +Left.Elements[1];
    Result.Z += Right.Elements[0] * -Left.Elements[1];
    Result.W += Right.Elements[1] * -Left.Elements[1];

    Result.X += Right.Elements[1] * -Left.Elements[2];
    Result.Y += Right.Elements[0] * +Left.Elements[2];
    Result.Z += Right.Elements[3] * +Left.Elements[2];
    Result.W += Right.Elements[2] * -Left.Elements[2];

    Result.X += Right.Elements[0] * +Left.Elements[3];
    Result.Y += Right.Elements[1] * +Left.Elements[3];
    Result.Z += Right.Elements[2] * +Left.Elements[3];
    Result.W += Right.Elements[3] * +Left.Elements[3];
#endif

    return Result;
}

COVERAGE(Rr_MulQF, 1)
static inline Rr_Quat
Rr_MulQF(Rr_Quat Left, Rr_F32 Multiplicative)
{
    ASSERT_COVERED(Rr_MulQF);

    Rr_Quat Result;

#ifdef RR_MATH__USE_SSE
    __m128 Scalar = _mm_set1_ps(Multiplicative);
    Result.SSE = _mm_mul_ps(Left.SSE, Scalar);
#elif defined(RR_MATH__USE_NEON)
    Result.NEON = vmulq_n_f32(Left.NEON, Multiplicative);
#else
    Result.X = Left.X * Multiplicative;
    Result.Y = Left.Y * Multiplicative;
    Result.Z = Left.Z * Multiplicative;
    Result.W = Left.W * Multiplicative;
#endif

    return Result;
}

COVERAGE(Rr_DivQF, 1)
static inline Rr_Quat
Rr_DivQF(Rr_Quat Left, Rr_F32 Divnd)
{
    ASSERT_COVERED(Rr_DivQF);

    Rr_Quat Result;

#ifdef RR_MATH__USE_SSE
    __m128 Scalar = _mm_set1_ps(Divnd);
    Result.SSE = _mm_div_ps(Left.SSE, Scalar);
#elif defined(RR_MATH__USE_NEON)
    Rr_F3232x4_t Scalar = vdupq_n_f32(Divnd);
    Result.NEON = vdivq_f32(Left.NEON, Scalar);
#else
    Result.X = Left.X / Divnd;
    Result.Y = Left.Y / Divnd;
    Result.Z = Left.Z / Divnd;
    Result.W = Left.W / Divnd;
#endif

    return Result;
}

COVERAGE(Rr_DotQ, 1)
static inline Rr_F32
Rr_DotQ(Rr_Quat Left, Rr_Quat Right)
{
    ASSERT_COVERED(Rr_DotQ);

    Rr_F32 Result;

#ifdef RR_MATH__USE_SSE
    __m128 SSEResultOne = _mm_mul_ps(Left.SSE, Right.SSE);
    __m128 SSEResultTwo =
        _mm_shuffle_ps(SSEResultOne, SSEResultOne, _MM_SHUFFLE(2, 3, 0, 1));
    SSEResultOne = _mm_add_ps(SSEResultOne, SSEResultTwo);
    SSEResultTwo =
        _mm_shuffle_ps(SSEResultOne, SSEResultOne, _MM_SHUFFLE(0, 1, 2, 3));
    SSEResultOne = _mm_add_ps(SSEResultOne, SSEResultTwo);
    _mm_store_ss(&Result, SSEResultOne);
#elif defined(RR_MATH__USE_NEON)
    Rr_F3232x4_t NEONMultiplyResult = vmulq_f32(Left.NEON, Right.NEON);
    Rr_F3232x4_t NEONHalfAdd =
        vpaddq_f32(NEONMultiplyResult, NEONMultiplyResult);
    Rr_F3232x4_t NEONFullAdd = vpaddq_f32(NEONHalfAdd, NEONHalfAdd);
    Result = vgetq_lane_f32(NEONFullAdd, 0);
#else
    Result = ((Left.X * Right.X) + (Left.Z * Right.Z))
        + ((Left.Y * Right.Y) + (Left.W * Right.W));
#endif

    return Result;
}

COVERAGE(Rr_InvQ, 1)
static inline Rr_Quat
Rr_InvQ(Rr_Quat Left)
{
    ASSERT_COVERED(Rr_InvQ);

    Rr_Quat Result;
    Result.X = -Left.X;
    Result.Y = -Left.Y;
    Result.Z = -Left.Z;
    Result.W = Left.W;

    return Rr_DivQF(Result, (Rr_DotQ(Left, Left)));
}

COVERAGE(Rr_NormQ, 1)
static inline Rr_Quat
Rr_NormQ(Rr_Quat Quat)
{
    ASSERT_COVERED(Rr_NormQ);

    /* NOTE(lcf): Take advantage of SSE implementation in Rr_NormV4 */
    Rr_Vec4 Vec = { Quat.X, Quat.Y, Quat.Z, Quat.W };
    Vec = Rr_NormV4(Vec);
    Rr_Quat Result = { Vec.X, Vec.Y, Vec.Z, Vec.W };

    return Result;
}

static inline Rr_Quat
_Rr_MixQ(Rr_Quat Left, Rr_F32 MixLeft, Rr_Quat Right, Rr_F32 MixRight)
{
    Rr_Quat Result;

#ifdef RR_MATH__USE_SSE
    __m128 ScalarLeft = _mm_set1_ps(MixLeft);
    __m128 ScalarRight = _mm_set1_ps(MixRight);
    __m128 SSEResultOne = _mm_mul_ps(Left.SSE, ScalarLeft);
    __m128 SSEResultTwo = _mm_mul_ps(Right.SSE, ScalarRight);
    Result.SSE = _mm_add_ps(SSEResultOne, SSEResultTwo);
#elif defined(RR_MATH__USE_NEON)
    Rr_F3232x4_t ScaledLeft = vmulq_n_f32(Left.NEON, MixLeft);
    Rr_F3232x4_t ScaledRight = vmulq_n_f32(Right.NEON, MixRight);
    Result.NEON = vaddq_f32(ScaledLeft, ScaledRight);
#else
    Result.X = Left.X * MixLeft + Right.X * MixRight;
    Result.Y = Left.Y * MixLeft + Right.Y * MixRight;
    Result.Z = Left.Z * MixLeft + Right.Z * MixRight;
    Result.W = Left.W * MixLeft + Right.W * MixRight;
#endif

    return Result;
}

COVERAGE(Rr_NLerp, 1)
static inline Rr_Quat
Rr_NLerp(Rr_Quat Left, Rr_F32 Time, Rr_Quat Right)
{
    ASSERT_COVERED(Rr_NLerp);

    Rr_Quat Result = _Rr_MixQ(Left, 1.0f - Time, Right, Time);
    Result = Rr_NormQ(Result);

    return Result;
}

COVERAGE(Rr_SLerp, 1)
static inline Rr_Quat
Rr_SLerp(Rr_Quat Left, Rr_F32 Time, Rr_Quat Right)
{
    ASSERT_COVERED(Rr_SLerp);

    Rr_Quat Result;

    Rr_F32 Cos_Theta = Rr_DotQ(Left, Right);

    if (Cos_Theta < 0.0f)
    { /* NOTE(lcf): Take shortest path on Hyper-sphere */
        Cos_Theta = -Cos_Theta;
        Right = Rr_Q(-Right.X, -Right.Y, -Right.Z, -Right.W);
    }

    /* NOTE(lcf): Use Normalized Linear interpolation when vectors are roughly
     * not L.I. */
    if (Cos_Theta > 0.9995f)
    {
        Result = Rr_NLerp(Left, Time, Right);
    }
    else
    {
        Rr_F32 Angle = Rr_ACosF(Cos_Theta);
        Rr_F32 MixLeft = Rr_SinF((1.0f - Time) * Angle);
        Rr_F32 MixRight = Rr_SinF(Time * Angle);

        Result = _Rr_MixQ(Left, MixLeft, Right, MixRight);
        Result = Rr_NormQ(Result);
    }

    return Result;
}

COVERAGE(Rr_QToM4, 1)
static inline Rr_Mat4
Rr_QToM4(Rr_Quat Left)
{
    ASSERT_COVERED(Rr_QToM4);

    Rr_Mat4 Result;

    Rr_Quat NormalizedQ = Rr_NormQ(Left);

    Rr_F32 XX, YY, ZZ, XY, XZ, YZ, WX, WY, WZ;

    XX = NormalizedQ.X * NormalizedQ.X;
    YY = NormalizedQ.Y * NormalizedQ.Y;
    ZZ = NormalizedQ.Z * NormalizedQ.Z;
    XY = NormalizedQ.X * NormalizedQ.Y;
    XZ = NormalizedQ.X * NormalizedQ.Z;
    YZ = NormalizedQ.Y * NormalizedQ.Z;
    WX = NormalizedQ.W * NormalizedQ.X;
    WY = NormalizedQ.W * NormalizedQ.Y;
    WZ = NormalizedQ.W * NormalizedQ.Z;

    Result.Elements[0][0] = 1.0f - 2.0f * (YY + ZZ);
    Result.Elements[0][1] = 2.0f * (XY + WZ);
    Result.Elements[0][2] = 2.0f * (XZ - WY);
    Result.Elements[0][3] = 0.0f;

    Result.Elements[1][0] = 2.0f * (XY - WZ);
    Result.Elements[1][1] = 1.0f - 2.0f * (XX + ZZ);
    Result.Elements[1][2] = 2.0f * (YZ + WX);
    Result.Elements[1][3] = 0.0f;

    Result.Elements[2][0] = 2.0f * (XZ + WY);
    Result.Elements[2][1] = 2.0f * (YZ - WX);
    Result.Elements[2][2] = 1.0f - 2.0f * (XX + YY);
    Result.Elements[2][3] = 0.0f;

    Result.Elements[3][0] = 0.0f;
    Result.Elements[3][1] = 0.0f;
    Result.Elements[3][2] = 0.0f;
    Result.Elements[3][3] = 1.0f;

    return Result;
}

// This method taken from Mike Day at Insomniac Games.
// https://d3cw3dd2w32x2b.cloudfront.net/wp-content/uploads/2015/01/matrix-to-quat.pdf
//
// Note that as mentioned at the top of the paper, the paper assumes the matrix
// would be *post*-multiplied to a vector to rotate it, meaning the matrix is
// the transpose of what we're dealing with. But, because our matrices are
// stored in column-major order, the indices *appear* to match the paper.
//
// For example, m12 in the paper is row 1, column 2. We need to transpose it to
// row 2, column 1. But, because the column comes first when referencing
// elements, it looks like M.Elements[1][2].
//
// Don't be confused! Or if you must be confused, at least trust this
// comment. :)
COVERAGE(Rr_M4ToQ_RH, 4)
static inline Rr_Quat
Rr_M4ToQ_RH(Rr_Mat4 M)
{
    Rr_F32 T;
    Rr_Quat Q;

    if (M.Elements[2][2] < 0.0f)
    {
        if (M.Elements[0][0] > M.Elements[1][1])
        {
            ASSERT_COVERED(Rr_M4ToQ_RH);

            T = 1 + M.Elements[0][0] - M.Elements[1][1] - M.Elements[2][2];
            Q = Rr_Q(
                T,
                M.Elements[0][1] + M.Elements[1][0],
                M.Elements[2][0] + M.Elements[0][2],
                M.Elements[1][2] - M.Elements[2][1]);
        }
        else
        {
            ASSERT_COVERED(Rr_M4ToQ_RH);

            T = 1 - M.Elements[0][0] + M.Elements[1][1] - M.Elements[2][2];
            Q = Rr_Q(
                M.Elements[0][1] + M.Elements[1][0],
                T,
                M.Elements[1][2] + M.Elements[2][1],
                M.Elements[2][0] - M.Elements[0][2]);
        }
    }
    else
    {
        if (M.Elements[0][0] < -M.Elements[1][1])
        {
            ASSERT_COVERED(Rr_M4ToQ_RH);

            T = 1 - M.Elements[0][0] - M.Elements[1][1] + M.Elements[2][2];
            Q = Rr_Q(
                M.Elements[2][0] + M.Elements[0][2],
                M.Elements[1][2] + M.Elements[2][1],
                T,
                M.Elements[0][1] - M.Elements[1][0]);
        }
        else
        {
            ASSERT_COVERED(Rr_M4ToQ_RH);

            T = 1 + M.Elements[0][0] + M.Elements[1][1] + M.Elements[2][2];
            Q = Rr_Q(
                M.Elements[1][2] - M.Elements[2][1],
                M.Elements[2][0] - M.Elements[0][2],
                M.Elements[0][1] - M.Elements[1][0],
                T);
        }
    }

    Q = Rr_MulQF(Q, 0.5f / Rr_SqrtF(T));

    return Q;
}

COVERAGE(Rr_M4ToQ_LH, 4)
static inline Rr_Quat
Rr_M4ToQ_LH(Rr_Mat4 M)
{
    Rr_F32 T;
    Rr_Quat Q;

    if (M.Elements[2][2] < 0.0f)
    {
        if (M.Elements[0][0] > M.Elements[1][1])
        {
            ASSERT_COVERED(Rr_M4ToQ_LH);

            T = 1 + M.Elements[0][0] - M.Elements[1][1] - M.Elements[2][2];
            Q = Rr_Q(
                T,
                M.Elements[0][1] + M.Elements[1][0],
                M.Elements[2][0] + M.Elements[0][2],
                M.Elements[2][1] - M.Elements[1][2]);
        }
        else
        {
            ASSERT_COVERED(Rr_M4ToQ_LH);

            T = 1 - M.Elements[0][0] + M.Elements[1][1] - M.Elements[2][2];
            Q = Rr_Q(
                M.Elements[0][1] + M.Elements[1][0],
                T,
                M.Elements[1][2] + M.Elements[2][1],
                M.Elements[0][2] - M.Elements[2][0]);
        }
    }
    else
    {
        if (M.Elements[0][0] < -M.Elements[1][1])
        {
            ASSERT_COVERED(Rr_M4ToQ_LH);

            T = 1 - M.Elements[0][0] - M.Elements[1][1] + M.Elements[2][2];
            Q = Rr_Q(
                M.Elements[2][0] + M.Elements[0][2],
                M.Elements[1][2] + M.Elements[2][1],
                T,
                M.Elements[1][0] - M.Elements[0][1]);
        }
        else
        {
            ASSERT_COVERED(Rr_M4ToQ_LH);

            T = 1 + M.Elements[0][0] + M.Elements[1][1] + M.Elements[2][2];
            Q = Rr_Q(
                M.Elements[2][1] - M.Elements[1][2],
                M.Elements[0][2] - M.Elements[2][0],
                M.Elements[1][0] - M.Elements[0][2],
                T);
        }
    }

    Q = Rr_MulQF(Q, 0.5f / Rr_SqrtF(T));

    return Q;
}

COVERAGE(Rr_QFromAxisAngle_RH, 1)
static inline Rr_Quat
Rr_QFromAxisAngle_RH(Rr_Vec3 Axis, Rr_F32 Angle)
{
    ASSERT_COVERED(Rr_QFromAxisAngle_RH);

    Rr_Quat Result;

    Rr_Vec3 AxisNormalized = Rr_NormV3(Axis);
    Rr_F32 SineOfRotation = Rr_SinF(Angle / 2.0f);

    Result.XYZ = Rr_MulV3F(AxisNormalized, SineOfRotation);
    Result.W = Rr_CosF(Angle / 2.0f);

    return Result;
}

COVERAGE(Rr_QFromAxisAngle_LH, 1)
static inline Rr_Quat
Rr_QFromAxisAngle_LH(Rr_Vec3 Axis, Rr_F32 Angle)
{
    ASSERT_COVERED(Rr_QFromAxisAngle_LH);

    return Rr_QFromAxisAngle_RH(Axis, -Angle);
}

COVERAGE(Rr_QFromNormPair, 1)
static inline Rr_Quat
Rr_QFromNormPair(Rr_Vec3 Left, Rr_Vec3 Right)
{
    ASSERT_COVERED(Rr_QFromNormPair);

    Rr_Quat Result;

    Result.XYZ = Rr_Cross(Left, Right);
    Result.W = 1.0f + Rr_DotV3(Left, Right);

    return Rr_NormQ(Result);
}

COVERAGE(Rr_QFromVecPair, 1)
static inline Rr_Quat
Rr_QFromVecPair(Rr_Vec3 Left, Rr_Vec3 Right)
{
    ASSERT_COVERED(Rr_QFromVecPair);

    return Rr_QFromNormPair(Rr_NormV3(Left), Rr_NormV3(Right));
}

COVERAGE(Rr_RotateV2, 1)
static inline Rr_Vec2
Rr_RotateV2(Rr_Vec2 V, Rr_F32 Angle)
{
    ASSERT_COVERED(Rr_RotateV2)

    Rr_F32 sinA = Rr_SinF(Angle);
    Rr_F32 cosA = Rr_CosF(Angle);

    return Rr_V2(V.X * cosA - V.Y * sinA, V.X * sinA + V.Y * cosA);
}

// implementation from
// https://blog.molecular-matters.com/2013/05/24/a-faster-quaternion-vector-multiplication/
COVERAGE(Rr_RotateV3Q, 1)
static inline Rr_Vec3
Rr_RotateV3Q(Rr_Vec3 V, Rr_Quat Q)
{
    ASSERT_COVERED(Rr_RotateV3Q);

    Rr_Vec3 t = Rr_MulV3F(Rr_Cross(Q.XYZ, V), 2);
    return Rr_AddV3(V, Rr_AddV3(Rr_MulV3F(t, Q.W), Rr_Cross(Q.XYZ, t)));
}

COVERAGE(Rr_RotateV3AxisAngle_LH, 1)
static inline Rr_Vec3
Rr_RotateV3AxisAngle_LH(Rr_Vec3 V, Rr_Vec3 Axis, Rr_F32 Angle)
{
    ASSERT_COVERED(Rr_RotateV3AxisAngle_LH);

    return Rr_RotateV3Q(V, Rr_QFromAxisAngle_LH(Axis, Angle));
}

COVERAGE(Rr_RotateV3AxisAngle_RH, 1)
static inline Rr_Vec3
Rr_RotateV3AxisAngle_RH(Rr_Vec3 V, Rr_Vec3 Axis, Rr_F32 Angle)
{
    ASSERT_COVERED(Rr_RotateV3AxisAngle_RH);

    return Rr_RotateV3Q(V, Rr_QFromAxisAngle_RH(Axis, Angle));
}

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

COVERAGE(Rr_LenV2CPP, 1)
static inline Rr_F32
Rr_Len(Rr_Vec2 A)
{
    ASSERT_COVERED(Rr_LenV2CPP);
    return Rr_LenV2(A);
}

COVERAGE(Rr_LenV3CPP, 1)
static inline Rr_F32
Rr_Len(Rr_Vec3 A)
{
    ASSERT_COVERED(Rr_LenV3CPP);
    return Rr_LenV3(A);
}

COVERAGE(Rr_LenV4CPP, 1)
static inline Rr_F32
Rr_Len(Rr_Vec4 A)
{
    ASSERT_COVERED(Rr_LenV4CPP);
    return Rr_LenV4(A);
}

COVERAGE(Rr_LenSqrV2CPP, 1)
static inline Rr_F32
Rr_LenSqr(Rr_Vec2 A)
{
    ASSERT_COVERED(Rr_LenSqrV2CPP);
    return Rr_LenSqrV2(A);
}

COVERAGE(Rr_LenSqrV3CPP, 1)
static inline Rr_F32
Rr_LenSqr(Rr_Vec3 A)
{
    ASSERT_COVERED(Rr_LenSqrV3CPP);
    return Rr_LenSqrV3(A);
}

COVERAGE(Rr_LenSqrV4CPP, 1)
static inline Rr_F32
Rr_LenSqr(Rr_Vec4 A)
{
    ASSERT_COVERED(Rr_LenSqrV4CPP);
    return Rr_LenSqrV4(A);
}

COVERAGE(Rr_NormV2CPP, 1)
static inline Rr_Vec2
Rr_Norm(Rr_Vec2 A)
{
    ASSERT_COVERED(Rr_NormV2CPP);
    return Rr_NormV2(A);
}

COVERAGE(Rr_NormV3CPP, 1)
static inline Rr_Vec3
Rr_Norm(Rr_Vec3 A)
{
    ASSERT_COVERED(Rr_NormV3CPP);
    return Rr_NormV3(A);
}

COVERAGE(Rr_NormV4CPP, 1)
static inline Rr_Vec4
Rr_Norm(Rr_Vec4 A)
{
    ASSERT_COVERED(Rr_NormV4CPP);
    return Rr_NormV4(A);
}

COVERAGE(Rr_NormQCPP, 1)
static inline Rr_Quat
Rr_Norm(Rr_Quat A)
{
    ASSERT_COVERED(Rr_NormQCPP);
    return Rr_NormQ(A);
}

COVERAGE(Rr_DotV2CPP, 1)
static inline Rr_F32
Rr_Dot(Rr_Vec2 Left, Rr_Vec2 VecTwo)
{
    ASSERT_COVERED(Rr_DotV2CPP);
    return Rr_DotV2(Left, VecTwo);
}

COVERAGE(Rr_DotV3CPP, 1)
static inline Rr_F32
Rr_Dot(Rr_Vec3 Left, Rr_Vec3 VecTwo)
{
    ASSERT_COVERED(Rr_DotV3CPP);
    return Rr_DotV3(Left, VecTwo);
}

COVERAGE(Rr_DotV4CPP, 1)
static inline Rr_F32
Rr_Dot(Rr_Vec4 Left, Rr_Vec4 VecTwo)
{
    ASSERT_COVERED(Rr_DotV4CPP);
    return Rr_DotV4(Left, VecTwo);
}

COVERAGE(Rr_LerpV2CPP, 1)
static inline Rr_Vec2
Rr_Lerp(Rr_Vec2 Left, Rr_F32 Time, Rr_Vec2 Right)
{
    ASSERT_COVERED(Rr_LerpV2CPP);
    return Rr_LerpV2(Left, Time, Right);
}

COVERAGE(Rr_LerpV3CPP, 1)
static inline Rr_Vec3
Rr_Lerp(Rr_Vec3 Left, Rr_F32 Time, Rr_Vec3 Right)
{
    ASSERT_COVERED(Rr_LerpV3CPP);
    return Rr_LerpV3(Left, Time, Right);
}

COVERAGE(Rr_LerpV4CPP, 1)
static inline Rr_Vec4
Rr_Lerp(Rr_Vec4 Left, Rr_F32 Time, Rr_Vec4 Right)
{
    ASSERT_COVERED(Rr_LerpV4CPP);
    return Rr_LerpV4(Left, Time, Right);
}

COVERAGE(Rr_TransposeM2CPP, 1)
static inline Rr_Mat2
Rr_Transpose(Rr_Mat2 Matrix)
{
    ASSERT_COVERED(Rr_TransposeM2CPP);
    return Rr_TransposeM2(Matrix);
}

COVERAGE(Rr_TransposeM3CPP, 1)
static inline Rr_Mat3
Rr_Transpose(Rr_Mat3 Matrix)
{
    ASSERT_COVERED(Rr_TransposeM3CPP);
    return Rr_TransposeM3(Matrix);
}

COVERAGE(Rr_TransposeM4CPP, 1)
static inline Rr_Mat4
Rr_Transpose(Rr_Mat4 Matrix)
{
    ASSERT_COVERED(Rr_TransposeM4CPP);
    return Rr_TransposeM4(Matrix);
}

COVERAGE(Rr_DeterminantM2CPP, 1)
static inline Rr_F32
Rr_Determinant(Rr_Mat2 Matrix)
{
    ASSERT_COVERED(Rr_DeterminantM2CPP);
    return Rr_DeterminantM2(Matrix);
}

COVERAGE(Rr_DeterminantM3CPP, 1)
static inline Rr_F32
Rr_Determinant(Rr_Mat3 Matrix)
{
    ASSERT_COVERED(Rr_DeterminantM3CPP);
    return Rr_DeterminantM3(Matrix);
}

COVERAGE(Rr_DeterminantM4CPP, 1)
static inline Rr_F32
Rr_Determinant(Rr_Mat4 Matrix)
{
    ASSERT_COVERED(Rr_DeterminantM4CPP);
    return Rr_DeterminantM4(Matrix);
}

COVERAGE(Rr_InvGeneralM2CPP, 1)
static inline Rr_Mat2
Rr_InvGeneral(Rr_Mat2 Matrix)
{
    ASSERT_COVERED(Rr_InvGeneralM2CPP);
    return Rr_InvGeneralM2(Matrix);
}

COVERAGE(Rr_InvGeneralM3CPP, 1)
static inline Rr_Mat3
Rr_InvGeneral(Rr_Mat3 Matrix)
{
    ASSERT_COVERED(Rr_InvGeneralM3CPP);
    return Rr_InvGeneralM3(Matrix);
}

COVERAGE(Rr_InvGeneralM4CPP, 1)
static inline Rr_Mat4
Rr_InvGeneral(Rr_Mat4 Matrix)
{
    ASSERT_COVERED(Rr_InvGeneralM4CPP);
    return Rr_InvGeneralM4(Matrix);
}

COVERAGE(Rr_DotQCPP, 1)
static inline Rr_F32
Rr_Dot(Rr_Quat QuatOne, Rr_Quat QuatTwo)
{
    ASSERT_COVERED(Rr_DotQCPP);
    return Rr_DotQ(QuatOne, QuatTwo);
}

COVERAGE(Rr_AddV2CPP, 1)
static inline Rr_Vec2
Rr_Add(Rr_Vec2 Left, Rr_Vec2 Right)
{
    ASSERT_COVERED(Rr_AddV2CPP);
    return Rr_AddV2(Left, Right);
}

COVERAGE(Rr_AddV3CPP, 1)
static inline Rr_Vec3
Rr_Add(Rr_Vec3 Left, Rr_Vec3 Right)
{
    ASSERT_COVERED(Rr_AddV3CPP);
    return Rr_AddV3(Left, Right);
}

COVERAGE(Rr_AddV4CPP, 1)
static inline Rr_Vec4
Rr_Add(Rr_Vec4 Left, Rr_Vec4 Right)
{
    ASSERT_COVERED(Rr_AddV4CPP);
    return Rr_AddV4(Left, Right);
}

COVERAGE(Rr_AddM2CPP, 1)
static inline Rr_Mat2
Rr_Add(Rr_Mat2 Left, Rr_Mat2 Right)
{
    ASSERT_COVERED(Rr_AddM2CPP);
    return Rr_AddM2(Left, Right);
}

COVERAGE(Rr_AddM3CPP, 1)
static inline Rr_Mat3
Rr_Add(Rr_Mat3 Left, Rr_Mat3 Right)
{
    ASSERT_COVERED(Rr_AddM3CPP);
    return Rr_AddM3(Left, Right);
}

COVERAGE(Rr_AddM4CPP, 1)
static inline Rr_Mat4
Rr_Add(Rr_Mat4 Left, Rr_Mat4 Right)
{
    ASSERT_COVERED(Rr_AddM4CPP);
    return Rr_AddM4(Left, Right);
}

COVERAGE(Rr_AddQCPP, 1)
static inline Rr_Quat
Rr_Add(Rr_Quat Left, Rr_Quat Right)
{
    ASSERT_COVERED(Rr_AddQCPP);
    return Rr_AddQ(Left, Right);
}

COVERAGE(Rr_SubV2CPP, 1)
static inline Rr_Vec2
Rr_Sub(Rr_Vec2 Left, Rr_Vec2 Right)
{
    ASSERT_COVERED(Rr_SubV2CPP);
    return Rr_SubV2(Left, Right);
}

COVERAGE(Rr_SubV3CPP, 1)
static inline Rr_Vec3
Rr_Sub(Rr_Vec3 Left, Rr_Vec3 Right)
{
    ASSERT_COVERED(Rr_SubV3CPP);
    return Rr_SubV3(Left, Right);
}

COVERAGE(Rr_SubV4CPP, 1)
static inline Rr_Vec4
Rr_Sub(Rr_Vec4 Left, Rr_Vec4 Right)
{
    ASSERT_COVERED(Rr_SubV4CPP);
    return Rr_SubV4(Left, Right);
}

COVERAGE(Rr_SubM2CPP, 1)
static inline Rr_Mat2
Rr_Sub(Rr_Mat2 Left, Rr_Mat2 Right)
{
    ASSERT_COVERED(Rr_SubM2CPP);
    return Rr_SubM2(Left, Right);
}

COVERAGE(Rr_SubM3CPP, 1)
static inline Rr_Mat3
Rr_Sub(Rr_Mat3 Left, Rr_Mat3 Right)
{
    ASSERT_COVERED(Rr_SubM3CPP);
    return Rr_SubM3(Left, Right);
}

COVERAGE(Rr_SubM4CPP, 1)
static inline Rr_Mat4
Rr_Sub(Rr_Mat4 Left, Rr_Mat4 Right)
{
    ASSERT_COVERED(Rr_SubM4CPP);
    return Rr_SubM4(Left, Right);
}

COVERAGE(Rr_SubQCPP, 1)
static inline Rr_Quat
Rr_Sub(Rr_Quat Left, Rr_Quat Right)
{
    ASSERT_COVERED(Rr_SubQCPP);
    return Rr_SubQ(Left, Right);
}

COVERAGE(Rr_MulV2CPP, 1)
static inline Rr_Vec2
Rr_Mul(Rr_Vec2 Left, Rr_Vec2 Right)
{
    ASSERT_COVERED(Rr_MulV2CPP);
    return Rr_MulV2(Left, Right);
}

COVERAGE(Rr_MulV2FCPP, 1)
static inline Rr_Vec2
Rr_Mul(Rr_Vec2 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_MulV2FCPP);
    return Rr_MulV2F(Left, Right);
}

COVERAGE(Rr_MulV3CPP, 1)
static inline Rr_Vec3
Rr_Mul(Rr_Vec3 Left, Rr_Vec3 Right)
{
    ASSERT_COVERED(Rr_MulV3CPP);
    return Rr_MulV3(Left, Right);
}

COVERAGE(Rr_MulV3FCPP, 1)
static inline Rr_Vec3
Rr_Mul(Rr_Vec3 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_MulV3FCPP);
    return Rr_MulV3F(Left, Right);
}

COVERAGE(Rr_MulV4CPP, 1)
static inline Rr_Vec4
Rr_Mul(Rr_Vec4 Left, Rr_Vec4 Right)
{
    ASSERT_COVERED(Rr_MulV4CPP);
    return Rr_MulV4(Left, Right);
}

COVERAGE(Rr_MulV4FCPP, 1)
static inline Rr_Vec4
Rr_Mul(Rr_Vec4 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_MulV4FCPP);
    return Rr_MulV4F(Left, Right);
}

COVERAGE(Rr_MulM2CPP, 1)
static inline Rr_Mat2
Rr_Mul(Rr_Mat2 Left, Rr_Mat2 Right)
{
    ASSERT_COVERED(Rr_MulM2CPP);
    return Rr_MulM2(Left, Right);
}

COVERAGE(Rr_MulM3CPP, 1)
static inline Rr_Mat3
Rr_Mul(Rr_Mat3 Left, Rr_Mat3 Right)
{
    ASSERT_COVERED(Rr_MulM3CPP);
    return Rr_MulM3(Left, Right);
}

COVERAGE(Rr_MulM4CPP, 1)
static inline Rr_Mat4
Rr_Mul(Rr_Mat4 Left, Rr_Mat4 Right)
{
    ASSERT_COVERED(Rr_MulM4CPP);
    return Rr_MulM4(Left, Right);
}

COVERAGE(Rr_MulM2FCPP, 1)
static inline Rr_Mat2
Rr_Mul(Rr_Mat2 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_MulM2FCPP);
    return Rr_MulM2F(Left, Right);
}

COVERAGE(Rr_MulM3FCPP, 1)
static inline Rr_Mat3
Rr_Mul(Rr_Mat3 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_MulM3FCPP);
    return Rr_MulM3F(Left, Right);
}

COVERAGE(Rr_MulM4FCPP, 1)
static inline Rr_Mat4
Rr_Mul(Rr_Mat4 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_MulM4FCPP);
    return Rr_MulM4F(Left, Right);
}

COVERAGE(Rr_MulM2V2CPP, 1)
static inline Rr_Vec2
Rr_Mul(Rr_Mat2 Matrix, Rr_Vec2 Vector)
{
    ASSERT_COVERED(Rr_MulM2V2CPP);
    return Rr_MulM2V2(Matrix, Vector);
}

COVERAGE(Rr_MulM3V3CPP, 1)
static inline Rr_Vec3
Rr_Mul(Rr_Mat3 Matrix, Rr_Vec3 Vector)
{
    ASSERT_COVERED(Rr_MulM3V3CPP);
    return Rr_MulM3V3(Matrix, Vector);
}

COVERAGE(Rr_MulM4V4CPP, 1)
static inline Rr_Vec4
Rr_Mul(Rr_Mat4 Matrix, Rr_Vec4 Vector)
{
    ASSERT_COVERED(Rr_MulM4V4CPP);
    return Rr_MulM4V4(Matrix, Vector);
}

COVERAGE(Rr_MulQCPP, 1)
static inline Rr_Quat
Rr_Mul(Rr_Quat Left, Rr_Quat Right)
{
    ASSERT_COVERED(Rr_MulQCPP);
    return Rr_MulQ(Left, Right);
}

COVERAGE(Rr_MulQFCPP, 1)
static inline Rr_Quat
Rr_Mul(Rr_Quat Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_MulQFCPP);
    return Rr_MulQF(Left, Right);
}

COVERAGE(Rr_DivV2CPP, 1)
static inline Rr_Vec2
Rr_Div(Rr_Vec2 Left, Rr_Vec2 Right)
{
    ASSERT_COVERED(Rr_DivV2CPP);
    return Rr_DivV2(Left, Right);
}

COVERAGE(Rr_DivV2FCPP, 1)
static inline Rr_Vec2
Rr_Div(Rr_Vec2 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_DivV2FCPP);
    return Rr_DivV2F(Left, Right);
}

COVERAGE(Rr_DivV3CPP, 1)
static inline Rr_Vec3
Rr_Div(Rr_Vec3 Left, Rr_Vec3 Right)
{
    ASSERT_COVERED(Rr_DivV3CPP);
    return Rr_DivV3(Left, Right);
}

COVERAGE(Rr_DivV3FCPP, 1)
static inline Rr_Vec3
Rr_Div(Rr_Vec3 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_DivV3FCPP);
    return Rr_DivV3F(Left, Right);
}

COVERAGE(Rr_DivV4CPP, 1)
static inline Rr_Vec4
Rr_Div(Rr_Vec4 Left, Rr_Vec4 Right)
{
    ASSERT_COVERED(Rr_DivV4CPP);
    return Rr_DivV4(Left, Right);
}

COVERAGE(Rr_DivV4FCPP, 1)
static inline Rr_Vec4
Rr_Div(Rr_Vec4 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_DivV4FCPP);
    return Rr_DivV4F(Left, Right);
}

COVERAGE(Rr_DivM2FCPP, 1)
static inline Rr_Mat2
Rr_Div(Rr_Mat2 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_DivM2FCPP);
    return Rr_DivM2F(Left, Right);
}

COVERAGE(Rr_DivM3FCPP, 1)
static inline Rr_Mat3
Rr_Div(Rr_Mat3 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_DivM3FCPP);
    return Rr_DivM3F(Left, Right);
}

COVERAGE(Rr_DivM4FCPP, 1)
static inline Rr_Mat4
Rr_Div(Rr_Mat4 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_DivM4FCPP);
    return Rr_DivM4F(Left, Right);
}

COVERAGE(Rr_DivQFCPP, 1)
static inline Rr_Quat
Rr_Div(Rr_Quat Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_DivQFCPP);
    return Rr_DivQF(Left, Right);
}

COVERAGE(Rr_EqV2CPP, 1)
static inline Rr_Bool
Rr_Eq(Rr_Vec2 Left, Rr_Vec2 Right)
{
    ASSERT_COVERED(Rr_EqV2CPP);
    return Rr_EqV2(Left, Right);
}

COVERAGE(Rr_EqV3CPP, 1)
static inline Rr_Bool
Rr_Eq(Rr_Vec3 Left, Rr_Vec3 Right)
{
    ASSERT_COVERED(Rr_EqV3CPP);
    return Rr_EqV3(Left, Right);
}

COVERAGE(Rr_EqV4CPP, 1)
static inline Rr_Bool
Rr_Eq(Rr_Vec4 Left, Rr_Vec4 Right)
{
    ASSERT_COVERED(Rr_EqV4CPP);
    return Rr_EqV4(Left, Right);
}

COVERAGE(Rr_AddV2Op, 1)
static inline Rr_Vec2
operator+(Rr_Vec2 Left, Rr_Vec2 Right)
{
    ASSERT_COVERED(Rr_AddV2Op);
    return Rr_AddV2(Left, Right);
}

COVERAGE(Rr_AddV3Op, 1)
static inline Rr_Vec3
operator+(Rr_Vec3 Left, Rr_Vec3 Right)
{
    ASSERT_COVERED(Rr_AddV3Op);
    return Rr_AddV3(Left, Right);
}

COVERAGE(Rr_AddV4Op, 1)
static inline Rr_Vec4
operator+(Rr_Vec4 Left, Rr_Vec4 Right)
{
    ASSERT_COVERED(Rr_AddV4Op);
    return Rr_AddV4(Left, Right);
}

COVERAGE(Rr_AddM2Op, 1)
static inline Rr_Mat2
operator+(Rr_Mat2 Left, Rr_Mat2 Right)
{
    ASSERT_COVERED(Rr_AddM2Op);
    return Rr_AddM2(Left, Right);
}

COVERAGE(Rr_AddM3Op, 1)
static inline Rr_Mat3
operator+(Rr_Mat3 Left, Rr_Mat3 Right)
{
    ASSERT_COVERED(Rr_AddM3Op);
    return Rr_AddM3(Left, Right);
}

COVERAGE(Rr_AddM4Op, 1)
static inline Rr_Mat4
operator+(Rr_Mat4 Left, Rr_Mat4 Right)
{
    ASSERT_COVERED(Rr_AddM4Op);
    return Rr_AddM4(Left, Right);
}

COVERAGE(Rr_AddQOp, 1)
static inline Rr_Quat
operator+(Rr_Quat Left, Rr_Quat Right)
{
    ASSERT_COVERED(Rr_AddQOp);
    return Rr_AddQ(Left, Right);
}

COVERAGE(Rr_SubV2Op, 1)
static inline Rr_Vec2
operator-(Rr_Vec2 Left, Rr_Vec2 Right)
{
    ASSERT_COVERED(Rr_SubV2Op);
    return Rr_SubV2(Left, Right);
}

COVERAGE(Rr_SubV3Op, 1)
static inline Rr_Vec3
operator-(Rr_Vec3 Left, Rr_Vec3 Right)
{
    ASSERT_COVERED(Rr_SubV3Op);
    return Rr_SubV3(Left, Right);
}

COVERAGE(Rr_SubV4Op, 1)
static inline Rr_Vec4
operator-(Rr_Vec4 Left, Rr_Vec4 Right)
{
    ASSERT_COVERED(Rr_SubV4Op);
    return Rr_SubV4(Left, Right);
}

COVERAGE(Rr_SubM2Op, 1)
static inline Rr_Mat2
operator-(Rr_Mat2 Left, Rr_Mat2 Right)
{
    ASSERT_COVERED(Rr_SubM2Op);
    return Rr_SubM2(Left, Right);
}

COVERAGE(Rr_SubM3Op, 1)
static inline Rr_Mat3
operator-(Rr_Mat3 Left, Rr_Mat3 Right)
{
    ASSERT_COVERED(Rr_SubM3Op);
    return Rr_SubM3(Left, Right);
}

COVERAGE(Rr_SubM4Op, 1)
static inline Rr_Mat4
operator-(Rr_Mat4 Left, Rr_Mat4 Right)
{
    ASSERT_COVERED(Rr_SubM4Op);
    return Rr_SubM4(Left, Right);
}

COVERAGE(Rr_SubQOp, 1)
static inline Rr_Quat
operator-(Rr_Quat Left, Rr_Quat Right)
{
    ASSERT_COVERED(Rr_SubQOp);
    return Rr_SubQ(Left, Right);
}

COVERAGE(Rr_MulV2Op, 1)
static inline Rr_Vec2
operator*(Rr_Vec2 Left, Rr_Vec2 Right)
{
    ASSERT_COVERED(Rr_MulV2Op);
    return Rr_MulV2(Left, Right);
}

COVERAGE(Rr_MulV3Op, 1)
static inline Rr_Vec3
operator*(Rr_Vec3 Left, Rr_Vec3 Right)
{
    ASSERT_COVERED(Rr_MulV3Op);
    return Rr_MulV3(Left, Right);
}

COVERAGE(Rr_MulV4Op, 1)
static inline Rr_Vec4
operator*(Rr_Vec4 Left, Rr_Vec4 Right)
{
    ASSERT_COVERED(Rr_MulV4Op);
    return Rr_MulV4(Left, Right);
}

COVERAGE(Rr_MulM2Op, 1)
static inline Rr_Mat2
operator*(Rr_Mat2 Left, Rr_Mat2 Right)
{
    ASSERT_COVERED(Rr_MulM2Op);
    return Rr_MulM2(Left, Right);
}

COVERAGE(Rr_MulM3Op, 1)
static inline Rr_Mat3
operator*(Rr_Mat3 Left, Rr_Mat3 Right)
{
    ASSERT_COVERED(Rr_MulM3Op);
    return Rr_MulM3(Left, Right);
}

COVERAGE(Rr_MulM4Op, 1)
static inline Rr_Mat4
operator*(Rr_Mat4 Left, Rr_Mat4 Right)
{
    ASSERT_COVERED(Rr_MulM4Op);
    return Rr_MulM4(Left, Right);
}

COVERAGE(Rr_MulQOp, 1)
static inline Rr_Quat
operator*(Rr_Quat Left, Rr_Quat Right)
{
    ASSERT_COVERED(Rr_MulQOp);
    return Rr_MulQ(Left, Right);
}

COVERAGE(Rr_MulV2FOp, 1)
static inline Rr_Vec2
operator*(Rr_Vec2 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_MulV2FOp);
    return Rr_MulV2F(Left, Right);
}

COVERAGE(Rr_MulV3FOp, 1)
static inline Rr_Vec3
operator*(Rr_Vec3 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_MulV3FOp);
    return Rr_MulV3F(Left, Right);
}

COVERAGE(Rr_MulV4FOp, 1)
static inline Rr_Vec4
operator*(Rr_Vec4 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_MulV4FOp);
    return Rr_MulV4F(Left, Right);
}

COVERAGE(Rr_MulM2FOp, 1)
static inline Rr_Mat2
operator*(Rr_Mat2 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_MulM2FOp);
    return Rr_MulM2F(Left, Right);
}

COVERAGE(Rr_MulM3FOp, 1)
static inline Rr_Mat3
operator*(Rr_Mat3 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_MulM3FOp);
    return Rr_MulM3F(Left, Right);
}

COVERAGE(Rr_MulM4FOp, 1)
static inline Rr_Mat4
operator*(Rr_Mat4 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_MulM4FOp);
    return Rr_MulM4F(Left, Right);
}

COVERAGE(Rr_MulQFOp, 1)
static inline Rr_Quat
operator*(Rr_Quat Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_MulQFOp);
    return Rr_MulQF(Left, Right);
}

COVERAGE(Rr_MulV2FOpLeft, 1)
static inline Rr_Vec2
operator*(Rr_F32 Left, Rr_Vec2 Right)
{
    ASSERT_COVERED(Rr_MulV2FOpLeft);
    return Rr_MulV2F(Right, Left);
}

COVERAGE(Rr_MulV3FOpLeft, 1)
static inline Rr_Vec3
operator*(Rr_F32 Left, Rr_Vec3 Right)
{
    ASSERT_COVERED(Rr_MulV3FOpLeft);
    return Rr_MulV3F(Right, Left);
}

COVERAGE(Rr_MulV4FOpLeft, 1)
static inline Rr_Vec4
operator*(Rr_F32 Left, Rr_Vec4 Right)
{
    ASSERT_COVERED(Rr_MulV4FOpLeft);
    return Rr_MulV4F(Right, Left);
}

COVERAGE(Rr_MulM2FOpLeft, 1)
static inline Rr_Mat2
operator*(Rr_F32 Left, Rr_Mat2 Right)
{
    ASSERT_COVERED(Rr_MulM2FOpLeft);
    return Rr_MulM2F(Right, Left);
}

COVERAGE(Rr_MulM3FOpLeft, 1)
static inline Rr_Mat3
operator*(Rr_F32 Left, Rr_Mat3 Right)
{
    ASSERT_COVERED(Rr_MulM3FOpLeft);
    return Rr_MulM3F(Right, Left);
}

COVERAGE(Rr_MulM4FOpLeft, 1)
static inline Rr_Mat4
operator*(Rr_F32 Left, Rr_Mat4 Right)
{
    ASSERT_COVERED(Rr_MulM4FOpLeft);
    return Rr_MulM4F(Right, Left);
}

COVERAGE(Rr_MulQFOpLeft, 1)
static inline Rr_Quat
operator*(Rr_F32 Left, Rr_Quat Right)
{
    ASSERT_COVERED(Rr_MulQFOpLeft);
    return Rr_MulQF(Right, Left);
}

COVERAGE(Rr_MulM2V2Op, 1)
static inline Rr_Vec2
operator*(Rr_Mat2 Matrix, Rr_Vec2 Vector)
{
    ASSERT_COVERED(Rr_MulM2V2Op);
    return Rr_MulM2V2(Matrix, Vector);
}

COVERAGE(Rr_MulM3V3Op, 1)
static inline Rr_Vec3
operator*(Rr_Mat3 Matrix, Rr_Vec3 Vector)
{
    ASSERT_COVERED(Rr_MulM3V3Op);
    return Rr_MulM3V3(Matrix, Vector);
}

COVERAGE(Rr_MulM4V4Op, 1)
static inline Rr_Vec4
operator*(Rr_Mat4 Matrix, Rr_Vec4 Vector)
{
    ASSERT_COVERED(Rr_MulM4V4Op);
    return Rr_MulM4V4(Matrix, Vector);
}

COVERAGE(Rr_DivV2Op, 1)
static inline Rr_Vec2
operator/(Rr_Vec2 Left, Rr_Vec2 Right)
{
    ASSERT_COVERED(Rr_DivV2Op);
    return Rr_DivV2(Left, Right);
}

COVERAGE(Rr_DivV3Op, 1)
static inline Rr_Vec3
operator/(Rr_Vec3 Left, Rr_Vec3 Right)
{
    ASSERT_COVERED(Rr_DivV3Op);
    return Rr_DivV3(Left, Right);
}

COVERAGE(Rr_DivV4Op, 1)
static inline Rr_Vec4
operator/(Rr_Vec4 Left, Rr_Vec4 Right)
{
    ASSERT_COVERED(Rr_DivV4Op);
    return Rr_DivV4(Left, Right);
}

COVERAGE(Rr_DivV2FOp, 1)
static inline Rr_Vec2
operator/(Rr_Vec2 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_DivV2FOp);
    return Rr_DivV2F(Left, Right);
}

COVERAGE(Rr_DivV3FOp, 1)
static inline Rr_Vec3
operator/(Rr_Vec3 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_DivV3FOp);
    return Rr_DivV3F(Left, Right);
}

COVERAGE(Rr_DivV4FOp, 1)
static inline Rr_Vec4
operator/(Rr_Vec4 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_DivV4FOp);
    return Rr_DivV4F(Left, Right);
}

COVERAGE(Rr_DivM4FOp, 1)
static inline Rr_Mat4
operator/(Rr_Mat4 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_DivM4FOp);
    return Rr_DivM4F(Left, Right);
}

COVERAGE(Rr_DivM3FOp, 1)
static inline Rr_Mat3
operator/(Rr_Mat3 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_DivM3FOp);
    return Rr_DivM3F(Left, Right);
}

COVERAGE(Rr_DivM2FOp, 1)
static inline Rr_Mat2
operator/(Rr_Mat2 Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_DivM2FOp);
    return Rr_DivM2F(Left, Right);
}

COVERAGE(Rr_DivQFOp, 1)
static inline Rr_Quat
operator/(Rr_Quat Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_DivQFOp);
    return Rr_DivQF(Left, Right);
}

COVERAGE(Rr_AddV2Assign, 1)
static inline Rr_Vec2&
operator+=(Rr_Vec2& Left, Rr_Vec2 Right)
{
    ASSERT_COVERED(Rr_AddV2Assign);
    return Left = Left + Right;
}

COVERAGE(Rr_AddV3Assign, 1)
static inline Rr_Vec3&
operator+=(Rr_Vec3& Left, Rr_Vec3 Right)
{
    ASSERT_COVERED(Rr_AddV3Assign);
    return Left = Left + Right;
}

COVERAGE(Rr_AddV4Assign, 1)
static inline Rr_Vec4&
operator+=(Rr_Vec4& Left, Rr_Vec4 Right)
{
    ASSERT_COVERED(Rr_AddV4Assign);
    return Left = Left + Right;
}

COVERAGE(Rr_AddM2Assign, 1)
static inline Rr_Mat2&
operator+=(Rr_Mat2& Left, Rr_Mat2 Right)
{
    ASSERT_COVERED(Rr_AddM2Assign);
    return Left = Left + Right;
}

COVERAGE(Rr_AddM3Assign, 1)
static inline Rr_Mat3&
operator+=(Rr_Mat3& Left, Rr_Mat3 Right)
{
    ASSERT_COVERED(Rr_AddM3Assign);
    return Left = Left + Right;
}

COVERAGE(Rr_AddM4Assign, 1)
static inline Rr_Mat4&
operator+=(Rr_Mat4& Left, Rr_Mat4 Right)
{
    ASSERT_COVERED(Rr_AddM4Assign);
    return Left = Left + Right;
}

COVERAGE(Rr_AddQAssign, 1)
static inline Rr_Quat&
operator+=(Rr_Quat& Left, Rr_Quat Right)
{
    ASSERT_COVERED(Rr_AddQAssign);
    return Left = Left + Right;
}

COVERAGE(Rr_SubV2Assign, 1)
static inline Rr_Vec2&
operator-=(Rr_Vec2& Left, Rr_Vec2 Right)
{
    ASSERT_COVERED(Rr_SubV2Assign);
    return Left = Left - Right;
}

COVERAGE(Rr_SubV3Assign, 1)
static inline Rr_Vec3&
operator-=(Rr_Vec3& Left, Rr_Vec3 Right)
{
    ASSERT_COVERED(Rr_SubV3Assign);
    return Left = Left - Right;
}

COVERAGE(Rr_SubV4Assign, 1)
static inline Rr_Vec4&
operator-=(Rr_Vec4& Left, Rr_Vec4 Right)
{
    ASSERT_COVERED(Rr_SubV4Assign);
    return Left = Left - Right;
}

COVERAGE(Rr_SubM2Assign, 1)
static inline Rr_Mat2&
operator-=(Rr_Mat2& Left, Rr_Mat2 Right)
{
    ASSERT_COVERED(Rr_SubM2Assign);
    return Left = Left - Right;
}

COVERAGE(Rr_SubM3Assign, 1)
static inline Rr_Mat3&
operator-=(Rr_Mat3& Left, Rr_Mat3 Right)
{
    ASSERT_COVERED(Rr_SubM3Assign);
    return Left = Left - Right;
}

COVERAGE(Rr_SubM4Assign, 1)
static inline Rr_Mat4&
operator-=(Rr_Mat4& Left, Rr_Mat4 Right)
{
    ASSERT_COVERED(Rr_SubM4Assign);
    return Left = Left - Right;
}

COVERAGE(Rr_SubQAssign, 1)
static inline Rr_Quat&
operator-=(Rr_Quat& Left, Rr_Quat Right)
{
    ASSERT_COVERED(Rr_SubQAssign);
    return Left = Left - Right;
}

COVERAGE(Rr_MulV2Assign, 1)
static inline Rr_Vec2&
operator*=(Rr_Vec2& Left, Rr_Vec2 Right)
{
    ASSERT_COVERED(Rr_MulV2Assign);
    return Left = Left * Right;
}

COVERAGE(Rr_MulV3Assign, 1)
static inline Rr_Vec3&
operator*=(Rr_Vec3& Left, Rr_Vec3 Right)
{
    ASSERT_COVERED(Rr_MulV3Assign);
    return Left = Left * Right;
}

COVERAGE(Rr_MulV4Assign, 1)
static inline Rr_Vec4&
operator*=(Rr_Vec4& Left, Rr_Vec4 Right)
{
    ASSERT_COVERED(Rr_MulV4Assign);
    return Left = Left * Right;
}

COVERAGE(Rr_MulV2FAssign, 1)
static inline Rr_Vec2&
operator*=(Rr_Vec2& Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_MulV2FAssign);
    return Left = Left * Right;
}

COVERAGE(Rr_MulV3FAssign, 1)
static inline Rr_Vec3&
operator*=(Rr_Vec3& Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_MulV3FAssign);
    return Left = Left * Right;
}

COVERAGE(Rr_MulV4FAssign, 1)
static inline Rr_Vec4&
operator*=(Rr_Vec4& Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_MulV4FAssign);
    return Left = Left * Right;
}

COVERAGE(Rr_MulM2FAssign, 1)
static inline Rr_Mat2&
operator*=(Rr_Mat2& Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_MulM2FAssign);
    return Left = Left * Right;
}

COVERAGE(Rr_MulM3FAssign, 1)
static inline Rr_Mat3&
operator*=(Rr_Mat3& Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_MulM3FAssign);
    return Left = Left * Right;
}

COVERAGE(Rr_MulM4FAssign, 1)
static inline Rr_Mat4&
operator*=(Rr_Mat4& Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_MulM4FAssign);
    return Left = Left * Right;
}

COVERAGE(Rr_MulQFAssign, 1)
static inline Rr_Quat&
operator*=(Rr_Quat& Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_MulQFAssign);
    return Left = Left * Right;
}

COVERAGE(Rr_DivV2Assign, 1)
static inline Rr_Vec2&
operator/=(Rr_Vec2& Left, Rr_Vec2 Right)
{
    ASSERT_COVERED(Rr_DivV2Assign);
    return Left = Left / Right;
}

COVERAGE(Rr_DivV3Assign, 1)
static inline Rr_Vec3&
operator/=(Rr_Vec3& Left, Rr_Vec3 Right)
{
    ASSERT_COVERED(Rr_DivV3Assign);
    return Left = Left / Right;
}

COVERAGE(Rr_DivV4Assign, 1)
static inline Rr_Vec4&
operator/=(Rr_Vec4& Left, Rr_Vec4 Right)
{
    ASSERT_COVERED(Rr_DivV4Assign);
    return Left = Left / Right;
}

COVERAGE(Rr_DivV2FAssign, 1)
static inline Rr_Vec2&
operator/=(Rr_Vec2& Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_DivV2FAssign);
    return Left = Left / Right;
}

COVERAGE(Rr_DivV3FAssign, 1)
static inline Rr_Vec3&
operator/=(Rr_Vec3& Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_DivV3FAssign);
    return Left = Left / Right;
}

COVERAGE(Rr_DivV4FAssign, 1)
static inline Rr_Vec4&
operator/=(Rr_Vec4& Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_DivV4FAssign);
    return Left = Left / Right;
}

COVERAGE(Rr_DivM4FAssign, 1)
static inline Rr_Mat4&
operator/=(Rr_Mat4& Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_DivM4FAssign);
    return Left = Left / Right;
}

COVERAGE(Rr_DivQFAssign, 1)
static inline Rr_Quat&
operator/=(Rr_Quat& Left, Rr_F32 Right)
{
    ASSERT_COVERED(Rr_DivQFAssign);
    return Left = Left / Right;
}

COVERAGE(Rr_EqV2Op, 1)
static inline Rr_Bool
operator==(Rr_Vec2 Left, Rr_Vec2 Right)
{
    ASSERT_COVERED(Rr_EqV2Op);
    return Rr_EqV2(Left, Right);
}

COVERAGE(Rr_EqV3Op, 1)
static inline Rr_Bool
operator==(Rr_Vec3 Left, Rr_Vec3 Right)
{
    ASSERT_COVERED(Rr_EqV3Op);
    return Rr_EqV3(Left, Right);
}

COVERAGE(Rr_EqV4Op, 1)
static inline Rr_Bool
operator==(Rr_Vec4 Left, Rr_Vec4 Right)
{
    ASSERT_COVERED(Rr_EqV4Op);
    return Rr_EqV4(Left, Right);
}

COVERAGE(Rr_EqV2OpNot, 1)
static inline Rr_Bool
operator!=(Rr_Vec2 Left, Rr_Vec2 Right)
{
    ASSERT_COVERED(Rr_EqV2OpNot);
    return !Rr_EqV2(Left, Right);
}

COVERAGE(Rr_EqV3OpNot, 1)
static inline Rr_Bool
operator!=(Rr_Vec3 Left, Rr_Vec3 Right)
{
    ASSERT_COVERED(Rr_EqV3OpNot);
    return !Rr_EqV3(Left, Right);
}

COVERAGE(Rr_EqV4OpNot, 1)
static inline Rr_Bool
operator!=(Rr_Vec4 Left, Rr_Vec4 Right)
{
    ASSERT_COVERED(Rr_EqV4OpNot);
    return !Rr_EqV4(Left, Right);
}

COVERAGE(Rr_UnaryMinusV2, 1)
static inline Rr_Vec2
operator-(Rr_Vec2 In)
{
    ASSERT_COVERED(Rr_UnaryMinusV2);

    Rr_Vec2 Result;
    Result.X = -In.X;
    Result.Y = -In.Y;

    return Result;
}

COVERAGE(Rr_UnaryMinusV3, 1)
static inline Rr_Vec3
operator-(Rr_Vec3 In)
{
    ASSERT_COVERED(Rr_UnaryMinusV3);

    Rr_Vec3 Result;
    Result.X = -In.X;
    Result.Y = -In.Y;
    Result.Z = -In.Z;

    return Result;
}

COVERAGE(Rr_UnaryMinusV4, 1)
static inline Rr_Vec4
operator-(Rr_Vec4 In)
{
    ASSERT_COVERED(Rr_UnaryMinusV4);

    Rr_Vec4 Result;
    #if RR_MATH__USE_SSE
    Result.SSE = _mm_xor_ps(In.SSE, _mm_set1_ps(-0.0f));
    #elif defined(RR_MATH__USE_NEON)
    Rr_F3232x4_t Zero = vdupq_n_f32(0.0f);
    Result.NEON = vsubq_f32(Zero, In.NEON);
    #else
    Result.X = -In.X;
    Result.Y = -In.Y;
    Result.Z = -In.Z;
    Result.W = -In.W;
    #endif

    return Result;
}

#endif /* __cplusplus*/

#ifdef RR_MATH__USE_C11_GENERICS
    #define Rr_Add(A, B)       \
        _Generic(              \
            (A),               \
            Rr_Vec2: Rr_AddV2, \
            Rr_Vec3: Rr_AddV3, \
            Rr_Vec4: Rr_AddV4, \
            Rr_Mat2: Rr_AddM2, \
            Rr_Mat3: Rr_AddM3, \
            Rr_Mat4: Rr_AddM4, \
            Rr_Quat: Rr_AddQ)(A, B)

    #define Rr_Sub(A, B)       \
        _Generic(              \
            (A),               \
            Rr_Vec2: Rr_SubV2, \
            Rr_Vec3: Rr_SubV3, \
            Rr_Vec4: Rr_SubV4, \
            Rr_Mat2: Rr_SubM2, \
            Rr_Mat3: Rr_SubM3, \
            Rr_Mat4: Rr_SubM4, \
            Rr_Quat: Rr_SubQ)(A, B)

    #define Rr_Mul(A, B)             \
        _Generic(                    \
            (B),                     \
            Rr_F32: _Generic(        \
                (A),                 \
                Rr_Vec2: Rr_MulV2F,  \
                Rr_Vec3: Rr_MulV3F,  \
                Rr_Vec4: Rr_MulV4F,  \
                Rr_Mat2: Rr_MulM2F,  \
                Rr_Mat3: Rr_MulM3F,  \
                Rr_Mat4: Rr_MulM4F,  \
                Rr_Quat: Rr_MulQF),  \
            Rr_Mat2: Rr_MulM2,       \
            Rr_Mat3: Rr_MulM3,       \
            Rr_Mat4: Rr_MulM4,       \
            Rr_Quat: Rr_MulQ,        \
            default: _Generic(       \
                (A),                 \
                Rr_Vec2: Rr_MulV2,   \
                Rr_Vec3: Rr_MulV3,   \
                Rr_Vec4: Rr_MulV4,   \
                Rr_Mat2: Rr_MulM2V2, \
                Rr_Mat3: Rr_MulM3V3, \
                Rr_Mat4: Rr_MulM4V4))(A, B)

    #define Rr_Div(A, B)            \
        _Generic(                   \
            (B),                    \
            Rr_F32: _Generic(       \
                (A),                \
                Rr_Mat2: Rr_DivM2F, \
                Rr_Mat3: Rr_DivM3F, \
                Rr_Mat4: Rr_DivM4F, \
                Rr_Vec2: Rr_DivV2F, \
                Rr_Vec3: Rr_DivV3F, \
                Rr_Vec4: Rr_DivV4F, \
                Rr_Quat: Rr_DivQF), \
            Rr_Mat2: Rr_DivM2,      \
            Rr_Mat3: Rr_DivM3,      \
            Rr_Mat4: Rr_DivM4,      \
            Rr_Quat: Rr_DivQ,       \
            default: _Generic(      \
                (A),                \
                Rr_Vec2: Rr_DivV2,  \
                Rr_Vec3: Rr_DivV3,  \
                Rr_Vec4: Rr_DivV4))(A, B)

    #define Rr_Len(A) \
        _Generic(     \
            (A), Rr_Vec2: Rr_LenV2, Rr_Vec3: Rr_LenV3, Rr_Vec4: Rr_LenV4)(A)

    #define Rr_LenSqr(A)          \
        _Generic(                 \
            (A),                  \
            Rr_Vec2: Rr_LenSqrV2, \
            Rr_Vec3: Rr_LenSqrV3, \
            Rr_Vec4: Rr_LenSqrV4)(A)

    #define Rr_Norm(A)                                                        \
        _Generic(                                                             \
            (A), Rr_Vec2: Rr_NormV2, Rr_Vec3: Rr_NormV3, Rr_Vec4: Rr_NormV4)( \
            A)

    #define Rr_Dot(A, B)                                                   \
        _Generic(                                                          \
            (A), Rr_Vec2: Rr_DotV2, Rr_Vec3: Rr_DotV3, Rr_Vec4: Rr_DotV4)( \
            A, B)

    #define Rr_Lerp(A, T, B)    \
        _Generic(               \
            (A),                \
            Rr_F32: Rr_Lerp,    \
            Rr_Vec2: Rr_LerpV2, \
            Rr_Vec3: Rr_LerpV3, \
            Rr_Vec4: Rr_LerpV4)(A, T, B)

    #define Rr_Eq(A, B)                                                      \
        _Generic((A), Rr_Vec2: Rr_EqV2, Rr_Vec3: Rr_EqV3, Rr_Vec4: Rr_EqV4)( \
            A, B)

    #define Rr_Transpose(M)          \
        _Generic(                    \
            (M),                     \
            Rr_Mat2: Rr_TransposeM2, \
            Rr_Mat3: Rr_TransposeM3, \
            Rr_Mat4: Rr_TransposeM4)(M)

    #define Rr_Determinant(M)          \
        _Generic(                      \
            (M),                       \
            Rr_Mat2: Rr_DeterminantM2, \
            Rr_Mat3: Rr_DeterminantM3, \
            Rr_Mat4: Rr_DeterminantM4)(M)

    #define Rr_InvGeneral(M)          \
        _Generic(                     \
            (M),                      \
            Rr_Mat2: Rr_InvGeneralM2, \
            Rr_Mat3: Rr_InvGeneralM3, \
            Rr_Mat4: Rr_InvGeneralM4)(M)

#endif

#if defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic pop
#endif
