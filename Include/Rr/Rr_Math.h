/*
 * Copyright (C) 2024-2026 Alexandr Semenov <tidesmain@gmail.com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#ifndef RR_MATH_H
#define RR_MATH_H

#include <Rr/Rr_Defines.h>

#ifndef RR_MATH_NO_SIMD
#if defined(_MSC_VER) && defined(_M_X64)
#define RR_MATH__USE_SSE 1
#elif defined(__SSE2__)
#define RR_MATH__USE_SSE 1
#ifdef __SSE4_1__
#define RR_MATH__USE_SSE4_1 1
#endif
#elif defined(__ARM_NEON)
#define RR_MATH__USE_NEON 1
#endif
#endif

#ifdef RR_MATH__USE_SSE
#include <emmintrin.h>
#include <xmmintrin.h>
#endif

#ifdef RR_MATH__USE_SSE4_1
#include <smmintrin.h>
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
#pragma GCC diagnostic ignored "-Wnested-anon-types"
#pragma GCC diagnostic ignored "-Wgnu-anonymous-struct"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(RR_MATH_USE_DEGREES) && !defined(RR_MATH_USE_TURNS) && \
    !defined(RR_MATH_USE_RADIANS)
#define RR_MATH_USE_RADIANS
#endif

#define RR_PI         3.14159265358979323846
#define RR_PI32       3.14159265359f
#define RR_DEG180     180.0
#define RR_DEG18032   180.0f
#define RR_TURNHALF   0.5
#define RR_TURNHALF32 0.5f

#define RR_RAD_TO_DEG  (RR_STATIC_CAST(float, RR_DEG180 / RR_PI))
#define RR_RAD_TO_TURN (RR_STATIC_CAST(float, RR_TURNHALF / RR_PI))
#define RR_DEG_TO_RAD  (RR_STATIC_CAST(float, RR_PI / RR_DEG180))
#define RR_DEG_TO_TURN (RR_STATIC_CAST(float, RR_TURNHALF / RR_DEG180))
#define RR_TURN_TO_RAD (RR_STATIC_CAST(float, RR_PI / RR_TURNHALF))
#define RR_TURN_TO_DEG (RR_STATIC_CAST(float, RR_DEG180 / RR_TURNHALF))

#if defined(RR_MATH_USE_RADIANS)
#define RR_ANGLE_RAD(a)  (a)
#define RR_ANGLE_DEG(a)  ((a) * RR_DEG_TO_RAD)
#define RR_ANGLE_TURN(a) ((a) * RR_TURN_TO_RAD)
#elif defined(RR_MATH_USE_DEGREES)
#define RR_ANGLE_RAD(a)  ((a) * RR_RAD_TO_DEG)
#define RR_ANGLE_DEG(a)  (a)
#define RR_ANGLE_TURN(a) ((a) * RR_TURN_TO_DEG)
#elif defined(RR_MATH_USE_TURNS)
#define RR_ANGLE_RAD(a)  ((a) * RR_RAD_TO_TURN)
#define RR_ANGLE_DEG(a)  ((a) * RR_DEG_TO_TURN)
#define RR_ANGLE_TURN(a) (a)
#endif

#if !defined(RR_MATH_PROVIDE_MATH_FUNCTIONS)
#include <math.h>
#define RR_SINF  sinf
#define RR_COSF  cosf
#define RR_TANF  tanf
#define RR_SQRTF sqrtf
#define RR_ACOSF acosf
#endif

#if !defined(RR_ANGLE_USER_TO_INTERNAL)
#define RR_ANGLE_USER_TO_INTERNAL(a) (Rr_ToRad(a))
#endif

#if !defined(RR_ANGLE_INTERNAL_TO_USER)
#if defined(RR_MATH_USE_RADIANS)
#define RR_ANGLE_INTERNAL_TO_USER(a) (a)
#elif defined(RR_MATH_USE_DEGREES)
#define RR_ANGLE_INTERNAL_TO_USER(a) ((a) * RR_RAD_TO_DEG)
#elif defined(RR_MATH_USE_TURNS)
#define RR_ANGLE_INTERNAL_TO_USER(a) ((a) * RR_RAD_TO_TURN)
#endif
#endif

#define RR_MIN(A, B)      ((A) > (B) ? (B) : (A))
#define RR_MAX(A, B)      ((A) < (B) ? (B) : (A))
#define RR_CLAMP(A, X, B) (((X) < (A)) ? (A) : ((X) > (B)) ? (B) : (X))
#define RR_ABS(A)         ((A) > 0 ? (A) : -(A))
#define RR_MOD(A, M)      (((A) % (M)) >= 0 ? ((A) % (M)) : (((A) % (M)) + (M)))
#define RR_SQUARE(X)      ((X) * (X))

#ifdef __cplusplus
#define RR_REINTERPRET_CAST(Type, Expression) reinterpret_cast<Type>(Expression)
#define RR_STATIC_CAST(Type, Expression)      static_cast<Type>(Expression)
#define RR_CONST_CAST(Type, Expression)       const_cast<Type>(Expression)
#else
#define RR_REINTERPRET_CAST(Type, Expression) ((Type)(Expression))
#define RR_STATIC_CAST(Type, Expression)      ((Type)(Expression))
#define RR_CONST_CAST(Type, Expression)       ((Type)(Expression))
#endif

typedef union Rr_Vec2
{
    float Elements[2];

    struct
    {
        float X, Y;
    };

    struct
    {
        float U, V;
    };

    struct
    {
        float Width, Height;
    };

#ifdef __cplusplus
    inline float &operator[](int32_t Index)
    {
        return Elements[Index];
    }
    inline const float &operator[](int32_t Index) const
    {
        return Elements[Index];
    }
#endif
} Rr_Vec2;

typedef union Rr_Vec3
{
    float Elements[3];

    struct
    {
        float X, Y, Z;
    };

    struct
    {
        Rr_Vec2 XY;
        float _Ignored0;
    };

    struct
    {
        float _Ignored1;
        Rr_Vec2 YZ;
    };

    struct
    {
        Rr_Vec2 UV;
        float _Ignored2;
    };

    struct
    {
        float _Ignored3;
        Rr_Vec2 VW;
    };

    struct
    {
        float R, G, B;
    };

    struct
    {
        float U, V, W;
    };

#ifdef __cplusplus
    inline float &operator[](int32_t Index)
    {
        return Elements[Index];
    }
    inline const float &operator[](int32_t Index) const
    {
        return Elements[Index];
    }
#endif
} Rr_Vec3;

typedef union Rr_Vec4
{
    float Elements[4];

    struct
    {
        float X, Y, Z, W;
    };

    struct
    {
        Rr_Vec3 XYZ;
        float _Ignored0;
    };

    struct
    {
        Rr_Vec2 XY;
        float _Ignored1;
        float _Ignored2;
    };

    struct
    {
        float _Ignored3;
        Rr_Vec2 YZ;
        float _Ignored4;
    };

    struct
    {
        float _Ignored5;
        float _Ignored6;
        Rr_Vec2 ZW;
    };

    struct
    {
        float _Ignored7;
        float _Ignored8;
        float Width, Height;
    };

    struct
    {
        float R, G, B, A;
    };

    struct
    {
        Rr_Vec3 RGB;
        int32_t _Ignored9;
    };

#ifdef RR_MATH__USE_SSE
    __m128 SSE;
#endif

#ifdef RR_MATH__USE_NEON
    float32x4_t NEON;
#endif

#ifdef __cplusplus
    inline float &operator[](int32_t Index)
    {
        return Elements[Index];
    }
    inline const float &operator[](int32_t Index) const
    {
        return Elements[Index];
    }
#endif
} Rr_Vec4;

typedef union Rr_IntVec2
{
    int32_t Elements[2];

    struct
    {
        int32_t X, Y;
    };

    struct
    {
        int32_t R, G;
    };

    struct
    {
        int32_t Width, Height;
    };

#ifdef __cplusplus
    inline int32_t &operator[](int32_t Index)
    {
        return Elements[Index];
    }
    inline const int32_t &operator[](int32_t Index) const
    {
        return Elements[Index];
    }
#endif
} Rr_IntVec2;

typedef union Rr_IntVec3
{
    int32_t Elements[3];

    struct
    {
        int32_t X, Y, Z;
    };

    struct
    {
        Rr_IntVec2 XY;
        int32_t _Ignored0;
    };

    struct
    {
        int32_t _Ignored1;
        Rr_Vec2 YZ;
    };

    struct
    {
        Rr_IntVec2 UV;
        int32_t _Ignored2;
    };

    struct
    {
        int32_t _Ignored3;
        Rr_IntVec2 VW;
    };

    struct
    {
        int32_t Width, Height, Depth;
    };

    struct
    {
        int32_t R, G, B;
    };

#ifdef __cplusplus
    inline int32_t &operator[](int32_t Index)
    {
        return Elements[Index];
    }
    inline const int32_t &operator[](int32_t Index) const
    {
        return Elements[Index];
    }
#endif
} Rr_IntVec3;

typedef union Rr_IntVec4
{
    int32_t Elements[4];

    struct
    {
        int32_t X, Y, Z, W;
    };

    struct
    {
        Rr_IntVec3 XYZ;
        int32_t _Ignored0;
    };

    struct
    {
        Rr_IntVec2 XY;
        int32_t _Ignored1;
        int32_t _Ignored2;
    };

    struct
    {
        int32_t _Ignored3;
        Rr_IntVec2 YZ;
        int32_t _Ignored4;
    };

    struct
    {
        int32_t _Ignored5;
        int32_t _Ignored6;
        Rr_IntVec2 ZW;
    };

    struct
    {
        int32_t _Ignored7;
        int32_t _Ignored8;
        int32_t Width, Height;
    };

    struct
    {
        int32_t R, G, B, A;
    };

    struct
    {
        Rr_IntVec3 RGB;
        int32_t _Ignored9;
    };

#ifdef RR_MATH__USE_SSE
    __m128i SSE;
#endif

#ifdef RR_MATH__USE_NEON
    int32x4_t NEON;
#endif

#ifdef __cplusplus
    inline int32_t &operator[](int32_t Index)
    {
        return Elements[Index];
    }
    inline const int32_t &operator[](int32_t Index) const
    {
        return Elements[Index];
    }
#endif
} Rr_IntVec4;

typedef union Rr_Mat2
{
    float Elements[2][2];

    Rr_Vec2 Columns[2];

#ifdef __cplusplus
    inline Rr_Vec2 &operator[](int32_t Index)
    {
        return Columns[Index];
    }
    inline const Rr_Vec2 &operator[](int32_t Index) const
    {
        return Columns[Index];
    }
#endif
} Rr_Mat2;

typedef union Rr_Mat3
{
    float Elements[3][3];

    Rr_Vec3 Columns[3];

#ifdef __cplusplus
    inline Rr_Vec3 &operator[](int32_t Index)
    {
        return Columns[Index];
    }
    inline const Rr_Vec3 &operator[](int32_t Index) const
    {
        return Columns[Index];
    }
#endif
} Rr_Mat3;

typedef union Rr_Mat4
{
    float Elements[4][4];

    Rr_Vec4 Columns[4];

#ifdef __cplusplus
    inline Rr_Vec4 &operator[](int32_t Index)
    {
        return Columns[Index];
    }
    inline const Rr_Vec4 &operator[](int32_t Index) const
    {
        return Columns[Index];
    }
#endif
} Rr_Mat4;

typedef union Rr_Quat
{
    float Elements[4];

    struct
    {
        float X, Y, Z, W;
    };

    struct
    {
        Rr_Vec3 XYZ;
        float _Ignored0;
    };

    struct
    {
        Rr_Vec2 XY;
        float _Ignored1;
        float _Ignored2;
    };

    struct
    {
        float _Ignored3;
        Rr_Vec2 YZ;
        float _Ignored4;
    };

    struct
    {
        float _Ignored5;
        float _Ignored6;
        Rr_Vec2 ZW;
    };

#ifdef RR_MATH__USE_SSE
    __m128 SSE;
#endif
#ifdef RR_MATH__USE_NEON
    float32x4_t NEON;
#endif
} Rr_Quat;

typedef struct Rr_IntRect Rr_IntRect;
struct Rr_IntRect
{
    Rr_IntVec2 Offset;
    Rr_IntVec2 Extent;
};

typedef struct Rr_Rect Rr_Rect;
struct Rr_Rect
{
    Rr_Vec2 Offset;
    Rr_Vec2 Extent;
};

/*
 * Angle unit conversion functions
 */
static inline float Rr_ToRad(float Angle)
{
#if defined(RR_MATH_USE_RADIANS)
    float Result = Angle;
#elif defined(RR_MATH_USE_DEGREES)
    float Result = Angle * RR_DEG_TO_RAD;
#elif defined(RR_MATH_USE_TURNS)
    float Result = Angle * RR_TURN_TO_RAD;
#endif

    return Result;
}

static inline float Rr_ToDeg(float Angle)
{
#if defined(RR_MATH_USE_RADIANS)
    float Result = Angle * RR_RAD_TO_DEG;
#elif defined(RR_MATH_USE_DEGREES)
    float Result = Angle;
#elif defined(RR_MATH_USE_TURNS)
    float Result = Angle * RR_TURN_TO_DEG;
#endif

    return Result;
}

static inline float Rr_ToTurn(float Angle)
{
#if defined(RR_MATH_USE_RADIANS)
    float Result = Angle * RR_RAD_TO_TURN;
#elif defined(RR_MATH_USE_DEGREES)
    float Result = Angle * RR_DEG_TO_TURN;
#elif defined(RR_MATH_USE_TURNS)
    float Result = Angle;
#endif

    return Result;
}

/*
 * Floating-point math functions
 */

static inline float Rr_SinF(float Angle)
{
    return RR_SINF(RR_ANGLE_USER_TO_INTERNAL(Angle));
}

static inline float Rr_CosF(float Angle)
{
    return RR_COSF(RR_ANGLE_USER_TO_INTERNAL(Angle));
}

static inline float Rr_TanF(float Angle)
{
    return RR_TANF(RR_ANGLE_USER_TO_INTERNAL(Angle));
}

static inline float Rr_ACosF(float Arg)
{
    return RR_ANGLE_INTERNAL_TO_USER(RR_ACOSF(Arg));
}

static inline float Rr_SqrtF(float Float)
{
    float Result;

#ifdef RR_MATH__USE_SSE
    __m128 In = _mm_set_ss(Float);
    __m128 Out = _mm_sqrt_ss(In);
    Result = _mm_cvtss_f32(Out);
#elif defined(RR_MATH__USE_NEON)
    float32x4_t In = vdupq_n_f32(Float);
    float32x4_t Out = vsqrtq_f32(In);
    Result = vgetq_lane_f32(Out, 0);
#else
    Result = RR_SQRTF(Float);
#endif

    return Result;
}

static inline float Rr_InvSqrtF(float Float)
{
    float Result;

    Result = 1.0f / Rr_SqrtF(Float);

    return Result;
}

/*
 * RrFramework functions
 */

static inline float Rr_GetVerticalFoV(
    const float HorizontalFoV,
    const float Aspect)
{
    return 2.0f * atanf((tanf(HorizontalFoV / 2.0f) * Aspect));
}

static inline void Rr_PerspectiveResize(float Aspect, Rr_Mat4 *Proj)
{
    if (Proj->Elements[0][0] == 0.0f)
    {
        return;
    }

    Proj->Elements[0][0] = Proj->Elements[1][1] / Aspect;
}

static inline Rr_Mat4 Rr_VulkanMatrix(void)
{
    Rr_Mat4 Out = { 0 };
    Out.Elements[0][0] = 1.0f;
    Out.Elements[1][1] = -1.0f;
    Out.Elements[2][2] = -1.0f;
    Out.Elements[3][3] = 1.0f;
    return Out;
}

static inline Rr_Mat4 Rr_EulerXYZ(Rr_Vec3 Angles)
{
    float CosX, CosY, CosZ, SinX, SinY, SinZ, CosZSinX, CosXCosZ, SinYSinZ;

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
 * Utility Functions
 */

static inline float Rr_Lerp(float A, float Time, float B)
{
    return (1.0f - Time) * A + Time * B;
}

static inline float Rr_Damp(float A, float Time, float B)
{
    return Rr_Lerp(A, 1.0f - expf(-Time), B);
}

/*
 * Vector initialization
 */

static inline Rr_Vec2 Rr_V2(float X, float Y)
{
    Rr_Vec2 Result;
    Result.X = X;
    Result.Y = Y;

    return Result;
}

static inline Rr_Vec2 Rr_V2F(float X)
{
    return Rr_V2(X, X);
}

static inline Rr_Vec2 Rr_CastV2(Rr_IntVec2 A)
{
    return Rr_V2((float)A.X, (float)A.Y);
}

static inline Rr_Vec3 Rr_V3(float X, float Y, float Z)
{
    Rr_Vec3 Result;
    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;

    return Result;
}

static inline Rr_Vec3 Rr_V3F(float X)
{
    return Rr_V3(X, X, X);
}

static inline Rr_Vec3 Rr_CastV3(Rr_IntVec3 A)
{
    return Rr_V3((float)A.X, (float)A.Y, (float)A.Z);
}

static inline Rr_Vec4 Rr_V4(float X, float Y, float Z, float W)
{
    Rr_Vec4 Result;

#ifdef RR_MATH__USE_SSE
    Result.SSE = _mm_setr_ps(X, Y, Z, W);
#elif defined(RR_MATH__USE_NEON)
    float32x4_t V = { X, Y, Z, W };
    Result.NEON = V;
#else
    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;
    Result.W = W;
#endif

    return Result;
}

static inline Rr_Vec4 Rr_V4F(float X)
{
    Rr_Vec4 Result;

#ifdef RR_MATH__USE_SSE
    Result.SSE = _mm_set1_ps(X);
#elif defined(RR_MATH__USE_NEON)
    Result.NEON = vdupq_n_f32(X);
#else
    Result.X = X;
    Result.Y = X;
    Result.Z = X;
    Result.W = X;
#endif

    return Result;
}

static inline Rr_Vec4 Rr_V4V(Rr_Vec3 Vector, float W)
{
    Rr_Vec4 Result;

#ifdef RR_MATH__USE_SSE
    Result.SSE = _mm_setr_ps(Vector.X, Vector.Y, Vector.Z, W);
#elif defined(RR_MATH__USE_NEON)
    float32x4_t V = { Vector.X, Vector.Y, Vector.Z, W };
    Result.NEON = V;
#else
    Result.XYZ = Vector;
    Result.W = W;
#endif

    return Result;
}

static inline Rr_Vec4 Rr_CastV4(Rr_IntVec4 A)
{
    return Rr_V4((float)A.X, (float)A.Y, (float)A.Z, (float)A.W);
}

static inline Rr_IntVec2 Rr_IntV2(int32_t X, int32_t Y)
{
    Rr_IntVec2 Result;
    Result.X = X;
    Result.Y = Y;

    return Result;
}

static inline Rr_IntVec2 Rr_IntV2I(int32_t X)
{
    return Rr_IntV2(X, X);
}

static inline Rr_IntVec2 Rr_CastIntV2(Rr_Vec2 A)
{
    return Rr_IntV2((int32_t)A.X, (int32_t)A.Y);
}

static inline Rr_IntVec3 Rr_IntV3(int32_t X, int32_t Y, int32_t Z)
{
    Rr_IntVec3 Result;
    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;

    return Result;
}

static inline Rr_IntVec3 Rr_IntV3I(int32_t X)
{
    return Rr_IntV3(X, X, X);
}

static inline Rr_IntVec3 Rr_CastIntV3(Rr_Vec3 A)
{
    return Rr_IntV3((int32_t)A.X, (int32_t)A.Y, (int32_t)A.Z);
}

static inline Rr_IntVec4 Rr_IntV4(int32_t X, int32_t Y, int32_t Z, int32_t W)
{
    Rr_IntVec4 Result;

#ifdef RR_MATH__USE_SSE
    Result.SSE = _mm_setr_epi32(X, Y, Z, W);
#elif defined(RR_MATH__USE_NEON)
    int32x4_t V = { X, Y, Z, W };
    Result.NEON = V;
#else
    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;
    Result.W = W;
#endif

    return Result;
}

static inline Rr_IntVec4 Rr_IntV4I(int32_t X)
{
    Rr_IntVec4 Result;

#ifdef RR_MATH__USE_SSE
    Result.SSE = _mm_set1_epi32(X);
#elif defined(RR_MATH__USE_NEON)
    Result.NEON = vdupq_n_s32(X);
#else
    Result.X = X;
    Result.Y = X;
    Result.Z = X;
    Result.W = X;
#endif

    return Result;
}

static inline Rr_IntVec4 Rr_IntV4V(Rr_IntVec3 IntVector, int32_t W)
{
    Rr_IntVec4 Result;

#ifdef RR_MATH__USE_SSE
    Result.SSE = _mm_setr_epi32(IntVector.X, IntVector.Y, IntVector.Z, W);
#elif defined(RR_MATH__USE_NEON)
    int32x4_t V = { IntVector.X, IntVector.Y, IntVector.Z, W };
    Result.NEON = V;
#else
    Result.XYZ = IntVector;
    Result.W = W;
#endif

    return Result;
}

static inline Rr_IntVec4 Rr_CastIntV4(Rr_Vec4 A)
{
    return Rr_IntV4((int32_t)A.X, (int32_t)A.Y, (int32_t)A.Z, (int32_t)A.W);
}

/*
 * Binary vector operations
 */

static inline Rr_Vec2 Rr_AddV2(Rr_Vec2 Left, Rr_Vec2 Right)
{
    Rr_Vec2 Result;
    Result.X = Left.X + Right.X;
    Result.Y = Left.Y + Right.Y;

    return Result;
}

static inline Rr_Vec2 Rr_AddV2F(Rr_Vec2 Left, float Right)
{
    Rr_Vec2 Result;
    Result.X = Left.X + Right;
    Result.Y = Left.Y + Right;

    return Result;
}

static inline Rr_Vec3 Rr_AddV3(Rr_Vec3 Left, Rr_Vec3 Right)
{
    Rr_Vec3 Result;
    Result.X = Left.X + Right.X;
    Result.Y = Left.Y + Right.Y;
    Result.Z = Left.Z + Right.Z;

    return Result;
}

static inline Rr_Vec3 Rr_AddV3F(Rr_Vec3 Left, float Right)
{
    Rr_Vec3 Result;
    Result.X = Left.X + Right;
    Result.Y = Left.Y + Right;
    Result.Z = Left.Z + Right;

    return Result;
}

static inline Rr_Vec4 Rr_AddV4(Rr_Vec4 Left, Rr_Vec4 Right)
{
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

static inline Rr_Vec4 Rr_AddV4F(Rr_Vec4 Left, float Right)
{
    Rr_Vec4 Result;

    Result.X = Left.X + Right;
    Result.Y = Left.Y + Right;
    Result.Z = Left.Z + Right;
    Result.W = Left.W + Right;

    return Result;
}

static inline Rr_Vec2 Rr_SubV2(Rr_Vec2 Left, Rr_Vec2 Right)
{
    Rr_Vec2 Result;
    Result.X = Left.X - Right.X;
    Result.Y = Left.Y - Right.Y;

    return Result;
}

static inline Rr_Vec2 Rr_SubV2F(Rr_Vec2 Left, float Right)
{
    Rr_Vec2 Result;
    Result.X = Left.X - Right;
    Result.Y = Left.Y - Right;

    return Result;
}

static inline Rr_Vec3 Rr_SubV3(Rr_Vec3 Left, Rr_Vec3 Right)
{
    Rr_Vec3 Result;
    Result.X = Left.X - Right.X;
    Result.Y = Left.Y - Right.Y;
    Result.Z = Left.Z - Right.Z;

    return Result;
}

static inline Rr_Vec3 Rr_SubV3F(Rr_Vec3 Left, float Right)
{
    Rr_Vec3 Result;
    Result.X = Left.X - Right;
    Result.Y = Left.Y - Right;
    Result.Z = Left.Z - Right;

    return Result;
}

static inline Rr_Vec4 Rr_SubV4(Rr_Vec4 Left, Rr_Vec4 Right)
{
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

static inline Rr_Vec4 Rr_SubV4F(Rr_Vec4 Left, float Right)
{
    Rr_Vec4 Result;

    Result.X = Left.X - Right;
    Result.Y = Left.Y - Right;
    Result.Z = Left.Z - Right;
    Result.W = Left.W - Right;

    return Result;
}

static inline Rr_Vec2 Rr_MulV2(Rr_Vec2 Left, Rr_Vec2 Right)
{
    Rr_Vec2 Result;
    Result.X = Left.X * Right.X;
    Result.Y = Left.Y * Right.Y;

    return Result;
}

static inline Rr_Vec2 Rr_MulV2F(Rr_Vec2 Left, float Right)
{
    Rr_Vec2 Result;
    Result.X = Left.X * Right;
    Result.Y = Left.Y * Right;

    return Result;
}

static inline Rr_Vec3 Rr_MulV3(Rr_Vec3 Left, Rr_Vec3 Right)
{
    Rr_Vec3 Result;
    Result.X = Left.X * Right.X;
    Result.Y = Left.Y * Right.Y;
    Result.Z = Left.Z * Right.Z;

    return Result;
}

static inline Rr_Vec3 Rr_MulV3F(Rr_Vec3 Left, float Right)
{
    Rr_Vec3 Result;
    Result.X = Left.X * Right;
    Result.Y = Left.Y * Right;
    Result.Z = Left.Z * Right;

    return Result;
}

static inline Rr_Vec4 Rr_MulV4(Rr_Vec4 Left, Rr_Vec4 Right)
{
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

static inline Rr_Vec4 Rr_MulV4F(Rr_Vec4 Left, float Right)
{
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

static inline Rr_Vec2 Rr_DivV2(Rr_Vec2 Left, Rr_Vec2 Right)
{
    Rr_Vec2 Result;
    Result.X = Left.X / Right.X;
    Result.Y = Left.Y / Right.Y;

    return Result;
}

static inline Rr_Vec2 Rr_DivV2F(Rr_Vec2 Left, float Right)
{
    Rr_Vec2 Result;
    Result.X = Left.X / Right;
    Result.Y = Left.Y / Right;

    return Result;
}

static inline Rr_Vec3 Rr_DivV3(Rr_Vec3 Left, Rr_Vec3 Right)
{
    Rr_Vec3 Result;
    Result.X = Left.X / Right.X;
    Result.Y = Left.Y / Right.Y;
    Result.Z = Left.Z / Right.Z;

    return Result;
}

static inline Rr_Vec3 Rr_DivV3F(Rr_Vec3 Left, float Right)
{
    Rr_Vec3 Result;
    Result.X = Left.X / Right;
    Result.Y = Left.Y / Right;
    Result.Z = Left.Z / Right;

    return Result;
}

static inline Rr_Vec4 Rr_DivV4(Rr_Vec4 Left, Rr_Vec4 Right)
{
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

static inline Rr_Vec4 Rr_DivV4F(Rr_Vec4 Left, float Right)
{
    Rr_Vec4 Result;

#ifdef RR_MATH__USE_SSE
    __m128 Scalar = _mm_set1_ps(Right);
    Result.SSE = _mm_div_ps(Left.SSE, Scalar);
#elif defined(RR_MATH__USE_NEON)
    float32x4_t Scalar = vdupq_n_f32(Right);
    Result.NEON = vdivq_f32(Left.NEON, Scalar);
#else
    Result.X = Left.X / Right;
    Result.Y = Left.Y / Right;
    Result.Z = Left.Z / Right;
    Result.W = Left.W / Right;
#endif

    return Result;
}

static inline bool Rr_EqV2(Rr_Vec2 Left, Rr_Vec2 Right)
{
    return Left.X == Right.X && Left.Y == Right.Y;
}

static inline bool Rr_EqV3(Rr_Vec3 Left, Rr_Vec3 Right)
{
    return Left.X == Right.X && Left.Y == Right.Y && Left.Z == Right.Z;
}

static inline bool Rr_EqV4(Rr_Vec4 Left, Rr_Vec4 Right)
{
    return Left.X == Right.X && Left.Y == Right.Y && Left.Z == Right.Z &&
           Left.W == Right.W;
}

static inline bool Rr_EqIV3(Rr_IntVec3 Left, Rr_IntVec3 Right)
{
    return Left.X == Right.X && Left.Y == Right.Y && Left.Z == Right.Z;
}

static inline float Rr_DotV2(Rr_Vec2 Left, Rr_Vec2 Right)
{
    return (Left.X * Right.X) + (Left.Y * Right.Y);
}

static inline float Rr_DotV3(Rr_Vec3 Left, Rr_Vec3 Right)
{
    return (Left.X * Right.X) + (Left.Y * Right.Y) + (Left.Z * Right.Z);
}

static inline float Rr_DotV4(Rr_Vec4 Left, Rr_Vec4 Right)
{
    float Result;

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
    float32x4_t NEONMultiplyResult = vmulq_f32(Left.NEON, Right.NEON);
    float32x4_t NEONHalfAdd =
        vpaddq_f32(NEONMultiplyResult, NEONMultiplyResult);
    float32x4_t NEONFullAdd = vpaddq_f32(NEONHalfAdd, NEONHalfAdd);
    Result = vgetq_lane_f32(NEONFullAdd, 0);
#else
    Result = ((Left.X * Right.X) + (Left.Z * Right.Z)) +
             ((Left.Y * Right.Y) + (Left.W * Right.W));
#endif

    return Result;
}

static inline Rr_Vec3 Rr_Cross(Rr_Vec3 Left, Rr_Vec3 Right)
{
    Rr_Vec3 Result;
    Result.X = (Left.Y * Right.Z) - (Left.Z * Right.Y);
    Result.Y = (Left.Z * Right.X) - (Left.X * Right.Z);
    Result.Z = (Left.X * Right.Y) - (Left.Y * Right.X);

    return Result;
}

static inline Rr_Vec2 Rr_FloorV2(Rr_Vec2 A)
{
    Rr_Vec2 Result;

    Result.X = floorf(A.X);
    Result.Y = floorf(A.Y);

    return Result;
}

static inline Rr_Vec3 Rr_FloorV3(Rr_Vec3 A)
{
    Rr_Vec3 Result;

    Result.X = floorf(A.X);
    Result.Y = floorf(A.Y);
    Result.Z = floorf(A.Z);

    return Result;
}

static inline Rr_Vec4 Rr_FloorV4(Rr_Vec4 A)
{
    Rr_Vec4 Result;

#ifdef RR_MATH__USE_SSE4_1
    Result.SSE = _mm_floor_ps(A.SSE);
#elif defined(__ARM_FEATURE_DIRECTED_ROUNDING)
    Result.NEON = vrndmq_f32(A.NEON);
#else
    Result.X = floorf(A.X);
    Result.Y = floorf(A.Y);
    Result.Z = floorf(A.Z);
    Result.W = floorf(A.W);
#endif

    return Result;
}

static inline Rr_Vec2 Rr_MinV2(Rr_Vec2 A, Rr_Vec2 B)
{
    Rr_Vec2 Result;

    Result.X = RR_MIN(A.X, B.X);
    Result.Y = RR_MIN(A.Y, B.Y);

    return Result;
}

static inline Rr_Vec3 Rr_MinV3(Rr_Vec3 A, Rr_Vec3 B)
{
    Rr_Vec3 Result;

    Result.X = RR_MIN(A.X, B.X);
    Result.Y = RR_MIN(A.Y, B.Y);
    Result.Z = RR_MIN(A.Z, B.Z);

    return Result;
}

static inline Rr_Vec4 Rr_MinV4(Rr_Vec4 A, Rr_Vec4 B)
{
    Rr_Vec4 Result;

#ifdef RR_MATH__USE_SSE
    Result.SSE = _mm_min_ps(A.SSE, B.SSE);
#elif defined(RR_MATH__USE_NEON)
    Result.NEON = vminq_f32(A.NEON, B.NEON);
#else
    Result.X = RR_MIN(A.X, B.X);
    Result.Y = RR_MIN(A.Y, B.Y);
    Result.Z = RR_MIN(A.Z, B.Z);
    Result.W = RR_MIN(A.W, B.W);
#endif

    return Result;
}

static inline Rr_Vec2 Rr_MaxV2(Rr_Vec2 A, Rr_Vec2 B)
{
    Rr_Vec2 Result;

    Result.X = RR_MAX(A.X, B.X);
    Result.Y = RR_MAX(A.Y, B.Y);

    return Result;
}

static inline Rr_Vec3 Rr_MaxV3(Rr_Vec3 A, Rr_Vec3 B)
{
    Rr_Vec3 Result;

    Result.X = RR_MAX(A.X, B.X);
    Result.Y = RR_MAX(A.Y, B.Y);
    Result.Z = RR_MAX(A.Z, B.Z);

    return Result;
}

static inline Rr_Vec4 Rr_MaxV4(Rr_Vec4 A, Rr_Vec4 B)
{
    Rr_Vec4 Result;

#ifdef RR_MATH__USE_SSE
    Result.SSE = _mm_max_ps(A.SSE, B.SSE);
#elif defined(RR_MATH__USE_NEON)
    Result.NEON = vmaxq_f32(A.NEON, B.NEON);
#else
    Result.X = RR_MAX(A.X, B.X);
    Result.Y = RR_MAX(A.Y, B.Y);
    Result.Z = RR_MAX(A.Z, B.Z);
    Result.W = RR_MAX(A.W, B.W);
#endif

    return Result;
}

static inline Rr_Vec2 Rr_FracV2(Rr_Vec2 A)
{
    Rr_Vec2 Result;

    Result.X = A.X - floorf(A.X);
    Result.Y = A.Y - floorf(A.Y);

    return Result;
}

static inline Rr_Vec3 Rr_FracV3(Rr_Vec3 A)
{
    Rr_Vec3 Result;

    Result.X = A.X - floorf(A.X);
    Result.Y = A.Y - floorf(A.Y);
    Result.Z = A.Z - floorf(A.Z);

    return Result;
}

static inline Rr_Vec4 Rr_FracV4(Rr_Vec4 A)
{
    Rr_Vec4 Result;

#ifdef RR_MATH__USE_SSE4_1
    Result.SSE = _mm_sub_ps(A.SSE, _mm_floor_ps(A.SSE));
#elif defined(__ARM_FEATURE_DIRECTED_ROUNDING)
    Result.NEON = vsubq_f32(A.NEON, vrndmq_f32(A.NEON));
#else
    Result.X = A.X - floorf(A.X);
    Result.Y = A.Y - floorf(A.Y);
    Result.Z = A.Z - floorf(A.Z);
    Result.W = A.W - floorf(A.W);
#endif

    return Result;
}

/*
 * Unary vector operations
 */

static inline float Rr_LenSqrV2(Rr_Vec2 A)
{
    return Rr_DotV2(A, A);
}

static inline float Rr_LenSqrV3(Rr_Vec3 A)
{
    return Rr_DotV3(A, A);
}

static inline float Rr_LenSqrV4(Rr_Vec4 A)
{
    return Rr_DotV4(A, A);
}

static inline float Rr_LenV2(Rr_Vec2 A)
{
    return Rr_SqrtF(Rr_LenSqrV2(A));
}

static inline float Rr_LenV3(Rr_Vec3 A)
{
    return Rr_SqrtF(Rr_LenSqrV3(A));
}

static inline float Rr_LenV4(Rr_Vec4 A)
{
    return Rr_SqrtF(Rr_LenSqrV4(A));
}

static inline Rr_Vec2 Rr_NormV2(Rr_Vec2 A)
{
    return Rr_MulV2F(A, Rr_InvSqrtF(Rr_DotV2(A, A)));
}

static inline Rr_Vec3 Rr_NormV3(Rr_Vec3 A)
{
    return Rr_MulV3F(A, Rr_InvSqrtF(Rr_DotV3(A, A)));
}

static inline Rr_Vec4 Rr_NormV4(Rr_Vec4 A)
{
    return Rr_MulV4F(A, Rr_InvSqrtF(Rr_DotV4(A, A)));
}

/*
 * Utility vector functions
 */

static inline Rr_Vec2 Rr_LerpV2(Rr_Vec2 A, float Time, Rr_Vec2 B)
{
    return Rr_AddV2(Rr_MulV2F(A, 1.0f - Time), Rr_MulV2F(B, Time));
}

static inline Rr_Vec3 Rr_LerpV3(Rr_Vec3 A, float Time, Rr_Vec3 B)
{
    return Rr_AddV3(Rr_MulV3F(A, 1.0f - Time), Rr_MulV3F(B, Time));
}

static inline Rr_Vec4 Rr_LerpV4(Rr_Vec4 A, float Time, Rr_Vec4 B)
{
    return Rr_AddV4(Rr_MulV4F(A, 1.0f - Time), Rr_MulV4F(B, Time));
}

/*
 * SSE stuff
 */

static inline Rr_Vec4 Rr_LinearCombineV4M4(Rr_Vec4 Left, Rr_Mat4 Right)
{
    Rr_Vec4 Result;
#ifdef RR_MATH__USE_SSE
    Result.SSE = _mm_mul_ps(
        _mm_shuffle_ps(Left.SSE, Left.SSE, 0x00),
        Right.Columns[0].SSE);
    Result.SSE = _mm_add_ps(
        Result.SSE,
        _mm_mul_ps(
            _mm_shuffle_ps(Left.SSE, Left.SSE, 0x55),
            Right.Columns[1].SSE));
    Result.SSE = _mm_add_ps(
        Result.SSE,
        _mm_mul_ps(
            _mm_shuffle_ps(Left.SSE, Left.SSE, 0xaa),
            Right.Columns[2].SSE));
    Result.SSE = _mm_add_ps(
        Result.SSE,
        _mm_mul_ps(
            _mm_shuffle_ps(Left.SSE, Left.SSE, 0xff),
            Right.Columns[3].SSE));
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

static inline Rr_Mat2 Rr_M2(void)
{
    Rr_Mat2 Result = { 0 };
    return Result;
}

static inline Rr_Mat2 Rr_M2D(float Diagonal)
{
    Rr_Mat2 Result = { 0 };
    Result.Elements[0][0] = Diagonal;
    Result.Elements[1][1] = Diagonal;

    return Result;
}

static inline Rr_Mat2 Rr_TransposeM2(Rr_Mat2 Matrix)
{
    Rr_Mat2 Result = Matrix;

    Result.Elements[0][1] = Matrix.Elements[1][0];
    Result.Elements[1][0] = Matrix.Elements[0][1];

    return Result;
}

static inline Rr_Mat2 Rr_AddM2(Rr_Mat2 Left, Rr_Mat2 Right)
{
    Rr_Mat2 Result;

    Result.Elements[0][0] = Left.Elements[0][0] + Right.Elements[0][0];
    Result.Elements[0][1] = Left.Elements[0][1] + Right.Elements[0][1];
    Result.Elements[1][0] = Left.Elements[1][0] + Right.Elements[1][0];
    Result.Elements[1][1] = Left.Elements[1][1] + Right.Elements[1][1];

    return Result;
}

static inline Rr_Mat2 Rr_SubM2(Rr_Mat2 Left, Rr_Mat2 Right)
{
    Rr_Mat2 Result;

    Result.Elements[0][0] = Left.Elements[0][0] - Right.Elements[0][0];
    Result.Elements[0][1] = Left.Elements[0][1] - Right.Elements[0][1];
    Result.Elements[1][0] = Left.Elements[1][0] - Right.Elements[1][0];
    Result.Elements[1][1] = Left.Elements[1][1] - Right.Elements[1][1];

    return Result;
}

static inline Rr_Vec2 Rr_MulM2V2(Rr_Mat2 Matrix, Rr_Vec2 Vector)
{
    Rr_Vec2 Result;

    Result.X = Vector.Elements[0] * Matrix.Columns[0].X;
    Result.Y = Vector.Elements[0] * Matrix.Columns[0].Y;

    Result.X += Vector.Elements[1] * Matrix.Columns[1].X;
    Result.Y += Vector.Elements[1] * Matrix.Columns[1].Y;

    return Result;
}

static inline Rr_Mat2 Rr_MulM2(Rr_Mat2 Left, Rr_Mat2 Right)
{
    Rr_Mat2 Result;
    Result.Columns[0] = Rr_MulM2V2(Left, Right.Columns[0]);
    Result.Columns[1] = Rr_MulM2V2(Left, Right.Columns[1]);

    return Result;
}

static inline Rr_Mat2 Rr_MulM2F(Rr_Mat2 Matrix, float Scalar)
{
    Rr_Mat2 Result;

    Result.Elements[0][0] = Matrix.Elements[0][0] * Scalar;
    Result.Elements[0][1] = Matrix.Elements[0][1] * Scalar;
    Result.Elements[1][0] = Matrix.Elements[1][0] * Scalar;
    Result.Elements[1][1] = Matrix.Elements[1][1] * Scalar;

    return Result;
}

static inline Rr_Mat2 Rr_DivM2F(Rr_Mat2 Matrix, float Scalar)
{
    Rr_Mat2 Result;

    Result.Elements[0][0] = Matrix.Elements[0][0] / Scalar;
    Result.Elements[0][1] = Matrix.Elements[0][1] / Scalar;
    Result.Elements[1][0] = Matrix.Elements[1][0] / Scalar;
    Result.Elements[1][1] = Matrix.Elements[1][1] / Scalar;

    return Result;
}

static inline float Rr_DeterminantM2(Rr_Mat2 Matrix)
{
    return Matrix.Elements[0][0] * Matrix.Elements[1][1] -
           Matrix.Elements[0][1] * Matrix.Elements[1][0];
}

static inline Rr_Mat2 Rr_InvGeneralM2(Rr_Mat2 Matrix)
{
    Rr_Mat2 Result;
    float InvDeterminant = 1.0f / Rr_DeterminantM2(Matrix);
    Result.Elements[0][0] = InvDeterminant * +Matrix.Elements[1][1];
    Result.Elements[1][1] = InvDeterminant * +Matrix.Elements[0][0];
    Result.Elements[0][1] = InvDeterminant * -Matrix.Elements[0][1];
    Result.Elements[1][0] = InvDeterminant * -Matrix.Elements[1][0];

    return Result;
}

/*
 * 3x3 Matrices
 */

static inline Rr_Mat3 Rr_M3(void)
{
    Rr_Mat3 Result = { 0 };
    return Result;
}

static inline Rr_Mat3 Rr_M3D(float Diagonal)
{
    Rr_Mat3 Result = { 0 };
    Result.Elements[0][0] = Diagonal;
    Result.Elements[1][1] = Diagonal;
    Result.Elements[2][2] = Diagonal;

    return Result;
}

static inline Rr_Mat3 Rr_TransposeM3(Rr_Mat3 Matrix)
{
    Rr_Mat3 Result = Matrix;

    Result.Elements[0][1] = Matrix.Elements[1][0];
    Result.Elements[0][2] = Matrix.Elements[2][0];
    Result.Elements[1][0] = Matrix.Elements[0][1];
    Result.Elements[1][2] = Matrix.Elements[2][1];
    Result.Elements[2][1] = Matrix.Elements[1][2];
    Result.Elements[2][0] = Matrix.Elements[0][2];

    return Result;
}

static inline Rr_Mat3 Rr_AddM3(Rr_Mat3 Left, Rr_Mat3 Right)
{
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

static inline Rr_Mat3 Rr_SubM3(Rr_Mat3 Left, Rr_Mat3 Right)
{
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

static inline Rr_Vec3 Rr_MulM3V3(Rr_Mat3 Matrix, Rr_Vec3 Vector)
{
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

static inline Rr_Mat3 Rr_MulM3(Rr_Mat3 Left, Rr_Mat3 Right)
{
    Rr_Mat3 Result;
    Result.Columns[0] = Rr_MulM3V3(Left, Right.Columns[0]);
    Result.Columns[1] = Rr_MulM3V3(Left, Right.Columns[1]);
    Result.Columns[2] = Rr_MulM3V3(Left, Right.Columns[2]);

    return Result;
}

static inline Rr_Mat3 Rr_MulM3F(Rr_Mat3 Matrix, float Scalar)
{
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

static inline Rr_Mat3 Rr_DivM3F(Rr_Mat3 Matrix, float Scalar)
{
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

static inline float Rr_DeterminantM3(Rr_Mat3 Matrix)
{
    Rr_Mat3 Cross;
    Cross.Columns[0] = Rr_Cross(Matrix.Columns[1], Matrix.Columns[2]);
    Cross.Columns[1] = Rr_Cross(Matrix.Columns[2], Matrix.Columns[0]);
    Cross.Columns[2] = Rr_Cross(Matrix.Columns[0], Matrix.Columns[1]);

    return Rr_DotV3(Cross.Columns[2], Matrix.Columns[2]);
}

static inline Rr_Mat3 Rr_InvGeneralM3(Rr_Mat3 Matrix)
{
    Rr_Mat3 Cross;
    Cross.Columns[0] = Rr_Cross(Matrix.Columns[1], Matrix.Columns[2]);
    Cross.Columns[1] = Rr_Cross(Matrix.Columns[2], Matrix.Columns[0]);
    Cross.Columns[2] = Rr_Cross(Matrix.Columns[0], Matrix.Columns[1]);

    float InvDeterminant = 1.0f / Rr_DotV3(Cross.Columns[2], Matrix.Columns[2]);

    Rr_Mat3 Result;
    Result.Columns[0] = Rr_MulV3F(Cross.Columns[0], InvDeterminant);
    Result.Columns[1] = Rr_MulV3F(Cross.Columns[1], InvDeterminant);
    Result.Columns[2] = Rr_MulV3F(Cross.Columns[2], InvDeterminant);

    return Rr_TransposeM3(Result);
}

/*
 * 4x4 Matrices
 */

static inline Rr_Mat4 Rr_M4(void)
{
    Rr_Mat4 Result = { 0 };
    return Result;
}

static inline Rr_Mat4 Rr_M4D(float Diagonal)
{
    Rr_Mat4 Result = { 0 };
    Result.Elements[0][0] = Diagonal;
    Result.Elements[1][1] = Diagonal;
    Result.Elements[2][2] = Diagonal;
    Result.Elements[3][3] = Diagonal;

    return Result;
}

static inline Rr_Mat4 Rr_TransposeM4(Rr_Mat4 Matrix)
{
    Rr_Mat4 Result;
#ifdef RR_MATH__USE_SSE
    Result = Matrix;
    _MM_TRANSPOSE4_PS(
        Result.Columns[0].SSE,
        Result.Columns[1].SSE,
        Result.Columns[2].SSE,
        Result.Columns[3].SSE);
#elif defined(RR_MATH__USE_NEON)
    float32x4x4_t Transposed = vld4q_f32((float *)Matrix.Columns);
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

static inline Rr_Mat4 Rr_AddM4(Rr_Mat4 Left, Rr_Mat4 Right)
{
    Rr_Mat4 Result;

    Result.Columns[0] = Rr_AddV4(Left.Columns[0], Right.Columns[0]);
    Result.Columns[1] = Rr_AddV4(Left.Columns[1], Right.Columns[1]);
    Result.Columns[2] = Rr_AddV4(Left.Columns[2], Right.Columns[2]);
    Result.Columns[3] = Rr_AddV4(Left.Columns[3], Right.Columns[3]);

    return Result;
}

static inline Rr_Mat4 Rr_SubM4(Rr_Mat4 Left, Rr_Mat4 Right)
{
    Rr_Mat4 Result;

    Result.Columns[0] = Rr_SubV4(Left.Columns[0], Right.Columns[0]);
    Result.Columns[1] = Rr_SubV4(Left.Columns[1], Right.Columns[1]);
    Result.Columns[2] = Rr_SubV4(Left.Columns[2], Right.Columns[2]);
    Result.Columns[3] = Rr_SubV4(Left.Columns[3], Right.Columns[3]);

    return Result;
}

static inline Rr_Mat4 Rr_MulM4(Rr_Mat4 Left, Rr_Mat4 Right)
{
    Rr_Mat4 Result;
    Result.Columns[0] = Rr_LinearCombineV4M4(Right.Columns[0], Left);
    Result.Columns[1] = Rr_LinearCombineV4M4(Right.Columns[1], Left);
    Result.Columns[2] = Rr_LinearCombineV4M4(Right.Columns[2], Left);
    Result.Columns[3] = Rr_LinearCombineV4M4(Right.Columns[3], Left);

    return Result;
}

static inline Rr_Mat4 Rr_MulM4F(Rr_Mat4 Matrix, float Scalar)
{
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

static inline Rr_Vec4 Rr_MulM4V4(Rr_Mat4 Matrix, Rr_Vec4 Vector)
{
    return Rr_LinearCombineV4M4(Vector, Matrix);
}

static inline Rr_Mat4 Rr_DivM4F(Rr_Mat4 Matrix, float Scalar)
{
    Rr_Mat4 Result;

#ifdef RR_MATH__USE_SSE
    __m128 SSEScalar = _mm_set1_ps(Scalar);
    Result.Columns[0].SSE = _mm_div_ps(Matrix.Columns[0].SSE, SSEScalar);
    Result.Columns[1].SSE = _mm_div_ps(Matrix.Columns[1].SSE, SSEScalar);
    Result.Columns[2].SSE = _mm_div_ps(Matrix.Columns[2].SSE, SSEScalar);
    Result.Columns[3].SSE = _mm_div_ps(Matrix.Columns[3].SSE, SSEScalar);
#elif defined(RR_MATH__USE_NEON)
    float32x4_t NEONScalar = vdupq_n_f32(Scalar);
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

static inline float Rr_DeterminantM4(Rr_Mat4 Matrix)
{
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

// Returns a general-purpose inverse of an Rr_Mat4. Note that special-purpose
// inverses of many transformations are available and will be more efficient.
static inline Rr_Mat4 Rr_InvGeneralM4(Rr_Mat4 Matrix)
{
    Rr_Vec3 C01 = Rr_Cross(Matrix.Columns[0].XYZ, Matrix.Columns[1].XYZ);
    Rr_Vec3 C23 = Rr_Cross(Matrix.Columns[2].XYZ, Matrix.Columns[3].XYZ);
    Rr_Vec3 B10 = Rr_SubV3(
        Rr_MulV3F(Matrix.Columns[0].XYZ, Matrix.Columns[1].W),
        Rr_MulV3F(Matrix.Columns[1].XYZ, Matrix.Columns[0].W));
    Rr_Vec3 B32 = Rr_SubV3(
        Rr_MulV3F(Matrix.Columns[2].XYZ, Matrix.Columns[3].W),
        Rr_MulV3F(Matrix.Columns[3].XYZ, Matrix.Columns[2].W));

    float InvDeterminant = 1.0f / (Rr_DotV3(C01, B32) + Rr_DotV3(C23, B10));
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

// Produces a right-handed orthographic projection matrix with Z ranging from 0
// to 1 (the DirectX convention). Left, Right, Bottom, and Top specify the
// coordinates of their respective clipping planes. Near and Far specify the
// distances to the near and far clipping planes.
static inline Rr_Mat4 Rr_Orthographic_RH(
    float Left,
    float Right,
    float Bottom,
    float Top,
    float Near,
    float Far)
{
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

// Produces a left-handed orthographic projection matrix with Z ranging from 0
// to 1 (the DirectX convention). Left, Right, Bottom, and Top specify the
// coordinates of their respective clipping planes. Near and Far specify the
// distances to the near and far clipping planes.
static inline Rr_Mat4 Rr_Orthographic_LH(
    float Left,
    float Right,
    float Bottom,
    float Top,
    float Near,
    float Far)
{
    Rr_Mat4 Result = Rr_Orthographic_RH(Left, Right, Bottom, Top, Near, Far);
    Result.Elements[2][2] = -Result.Elements[2][2];

    return Result;
}

// Returns an inverse for the given orthographic projection matrix. Works for
// all orthographic projection matrices, regardless of handedness or NDC
// convention.
static inline Rr_Mat4 Rr_InvOrthographic(Rr_Mat4 OrthoMatrix)
{
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

static inline Rr_Mat4 Rr_Perspective_RH(
    float FOV,
    float AspectRatio,
    float Near,
    float Far)
{
    Rr_Mat4 Result = { 0 };

    // See
    // https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/gluPerspective.xml

    float Cotangent = 1.0f / Rr_TanF(FOV / 2.0f);
    Result.Elements[0][0] = Cotangent / AspectRatio;
    Result.Elements[1][1] = Cotangent;
    Result.Elements[2][3] = -1.0f;

    Result.Elements[2][2] = (Far) / (Near - Far);
    Result.Elements[3][2] = (Near * Far) / (Near - Far);

    return Result;
}

static inline Rr_Mat4 Rr_Perspective_LH(
    float FOV,
    float AspectRatio,
    float Near,
    float Far)
{
    Rr_Mat4 Result = Rr_Perspective_RH(FOV, AspectRatio, Near, Far);
    Result.Elements[2][2] = -Result.Elements[2][2];
    Result.Elements[2][3] = -Result.Elements[2][3];

    return Result;
}

static inline Rr_Mat4 Rr_InvPerspective_RH(Rr_Mat4 PerspectiveMatrix)
{
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

static inline Rr_Mat4 Rr_InvPerspective_LH(Rr_Mat4 PerspectiveMatrix)
{
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

static inline Rr_Mat4 Rr_Translate(Rr_Vec3 Translation)
{
    Rr_Mat4 Result = Rr_M4D(1.0f);
    Result.Elements[3][0] = Translation.X;
    Result.Elements[3][1] = Translation.Y;
    Result.Elements[3][2] = Translation.Z;

    return Result;
}

static inline Rr_Mat4 Rr_InvTranslate(Rr_Mat4 TranslationMatrix)
{
    Rr_Mat4 Result = TranslationMatrix;
    Result.Elements[3][0] = -Result.Elements[3][0];
    Result.Elements[3][1] = -Result.Elements[3][1];
    Result.Elements[3][2] = -Result.Elements[3][2];

    return Result;
}

static inline Rr_Mat4 Rr_Rotate_RH(float Angle, Rr_Vec3 Axis)
{
    Rr_Mat4 Result = Rr_M4D(1.0f);

    Axis = Rr_NormV3(Axis);

    float SinTheta = Rr_SinF(Angle);
    float CosTheta = Rr_CosF(Angle);
    float CosValue = 1.0f - CosTheta;

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

static inline Rr_Mat4 Rr_Rotate_LH(float Angle, Rr_Vec3 Axis)
{
    /* NOTE(lcf): Matrix will be inverse/transpose of RH. */
    return Rr_Rotate_RH(-Angle, Axis);
}

static inline Rr_Mat4 Rr_InvRotate(Rr_Mat4 RotationMatrix)
{
    return Rr_TransposeM4(RotationMatrix);
}

static inline Rr_Mat4 Rr_Scale(Rr_Vec3 Scale)
{
    Rr_Mat4 Result = Rr_M4D(1.0f);
    Result.Elements[0][0] = Scale.X;
    Result.Elements[1][1] = Scale.Y;
    Result.Elements[2][2] = Scale.Z;

    return Result;
}

static inline Rr_Mat4 Rr_InvScale(Rr_Mat4 ScaleMatrix)
{
    Rr_Mat4 Result = ScaleMatrix;
    Result.Elements[0][0] = 1.0f / Result.Elements[0][0];
    Result.Elements[1][1] = 1.0f / Result.Elements[1][1];
    Result.Elements[2][2] = 1.0f / Result.Elements[2][2];

    return Result;
}

static inline Rr_Mat4 _Rr_LookAt(Rr_Vec3 F, Rr_Vec3 S, Rr_Vec3 U, Rr_Vec3 Eye)
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

static inline Rr_Mat4 Rr_LookAt_RH(Rr_Vec3 Eye, Rr_Vec3 Center, Rr_Vec3 Up)
{
    Rr_Vec3 F = Rr_NormV3(Rr_SubV3(Center, Eye));
    Rr_Vec3 S = Rr_NormV3(Rr_Cross(F, Up));
    Rr_Vec3 U = Rr_Cross(S, F);

    return _Rr_LookAt(F, S, U, Eye);
}

static inline Rr_Mat4 Rr_LookAt_LH(Rr_Vec3 Eye, Rr_Vec3 Center, Rr_Vec3 Up)
{
    Rr_Vec3 F = Rr_NormV3(Rr_SubV3(Eye, Center));
    Rr_Vec3 S = Rr_NormV3(Rr_Cross(F, Up));
    Rr_Vec3 U = Rr_Cross(S, F);

    return _Rr_LookAt(F, S, U, Eye);
}

static inline Rr_Mat4 Rr_InvLookAt(Rr_Mat4 Matrix)
{
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
    Result.Elements[3][0] = -1.0f * Matrix.Elements[3][0] /
                            (Rotation.Elements[0][0] + Rotation.Elements[0][1] +
                             Rotation.Elements[0][2]);
    Result.Elements[3][1] = -1.0f * Matrix.Elements[3][1] /
                            (Rotation.Elements[1][0] + Rotation.Elements[1][1] +
                             Rotation.Elements[1][2]);
    Result.Elements[3][2] = -1.0f * Matrix.Elements[3][2] /
                            (Rotation.Elements[2][0] + Rotation.Elements[2][1] +
                             Rotation.Elements[2][2]);
    Result.Elements[3][3] = 1.0f;

    return Result;
}

/*
 * Quaternion operations
 */

static inline Rr_Quat Rr_Q(float X, float Y, float Z, float W)
{
    Rr_Quat Result;

#ifdef RR_MATH__USE_SSE
    Result.SSE = _mm_setr_ps(X, Y, Z, W);
#elif defined(RR_MATH__USE_NEON)
    float32x4_t v = { X, Y, Z, W };
    Result.NEON = v;
#else
    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;
    Result.W = W;
#endif

    return Result;
}

static inline Rr_Quat Rr_QV4(Rr_Vec4 Vector)
{
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

static inline Rr_Quat Rr_AddQ(Rr_Quat Left, Rr_Quat Right)
{
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

static inline Rr_Quat Rr_SubQ(Rr_Quat Left, Rr_Quat Right)
{
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

static inline Rr_Quat Rr_MulQ(Rr_Quat Left, Rr_Quat Right)
{
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
    float32x4_t Right1032 = vrev64q_f32(Right.NEON);
    float32x4_t Right3210 =
        vcombine_f32(vget_high_f32(Right1032), vget_low_f32(Right1032));
    float32x4_t Right2301 = vrev64q_f32(Right3210);

    float32x4_t FirstSign = { 1.0f, -1.0f, 1.0f, -1.0f };
    Result.NEON = vmulq_f32(
        Right3210,
        vmulq_f32(vdupq_laneq_f32(Left.NEON, 0), FirstSign));
    float32x4_t SecondSign = { 1.0f, 1.0f, -1.0f, -1.0f };
    Result.NEON = vfmaq_f32(
        Result.NEON,
        Right2301,
        vmulq_f32(vdupq_laneq_f32(Left.NEON, 1), SecondSign));
    float32x4_t ThirdSign = { -1.0f, 1.0f, 1.0f, -1.0f };
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

static inline Rr_Quat Rr_MulQF(Rr_Quat Left, float Multiplicative)
{
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

static inline Rr_Quat Rr_DivQF(Rr_Quat Left, float Divnd)
{
    Rr_Quat Result;

#ifdef RR_MATH__USE_SSE
    __m128 Scalar = _mm_set1_ps(Divnd);
    Result.SSE = _mm_div_ps(Left.SSE, Scalar);
#elif defined(RR_MATH__USE_NEON)
    float32x4_t Scalar = vdupq_n_f32(Divnd);
    Result.NEON = vdivq_f32(Left.NEON, Scalar);
#else
    Result.X = Left.X / Divnd;
    Result.Y = Left.Y / Divnd;
    Result.Z = Left.Z / Divnd;
    Result.W = Left.W / Divnd;
#endif

    return Result;
}

static inline float Rr_DotQ(Rr_Quat Left, Rr_Quat Right)
{
    float Result;

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
    float32x4_t NEONMultiplyResult = vmulq_f32(Left.NEON, Right.NEON);
    float32x4_t NEONHalfAdd =
        vpaddq_f32(NEONMultiplyResult, NEONMultiplyResult);
    float32x4_t NEONFullAdd = vpaddq_f32(NEONHalfAdd, NEONHalfAdd);
    Result = vgetq_lane_f32(NEONFullAdd, 0);
#else
    Result = ((Left.X * Right.X) + (Left.Z * Right.Z)) +
             ((Left.Y * Right.Y) + (Left.W * Right.W));
#endif

    return Result;
}

static inline Rr_Quat Rr_InvQ(Rr_Quat Left)
{
    Rr_Quat Result;
    Result.X = -Left.X;
    Result.Y = -Left.Y;
    Result.Z = -Left.Z;
    Result.W = Left.W;

    return Rr_DivQF(Result, (Rr_DotQ(Left, Left)));
}

static inline Rr_Quat Rr_NormQ(Rr_Quat Quat)
{
    /* NOTE(lcf): Take advantage of SSE implementation in Rr_NormV4 */
    Rr_Vec4 Vec = { Quat.X, Quat.Y, Quat.Z, Quat.W };
    Vec = Rr_NormV4(Vec);
    Rr_Quat Result = { Vec.X, Vec.Y, Vec.Z, Vec.W };

    return Result;
}

static inline Rr_Quat _Rr_MixQ(
    Rr_Quat Left,
    float MixLeft,
    Rr_Quat Right,
    float MixRight)
{
    Rr_Quat Result;

#ifdef RR_MATH__USE_SSE
    __m128 ScalarLeft = _mm_set1_ps(MixLeft);
    __m128 ScalarRight = _mm_set1_ps(MixRight);
    __m128 SSEResultOne = _mm_mul_ps(Left.SSE, ScalarLeft);
    __m128 SSEResultTwo = _mm_mul_ps(Right.SSE, ScalarRight);
    Result.SSE = _mm_add_ps(SSEResultOne, SSEResultTwo);
#elif defined(RR_MATH__USE_NEON)
    float32x4_t ScaledLeft = vmulq_n_f32(Left.NEON, MixLeft);
    float32x4_t ScaledRight = vmulq_n_f32(Right.NEON, MixRight);
    Result.NEON = vaddq_f32(ScaledLeft, ScaledRight);
#else
    Result.X = Left.X * MixLeft + Right.X * MixRight;
    Result.Y = Left.Y * MixLeft + Right.Y * MixRight;
    Result.Z = Left.Z * MixLeft + Right.Z * MixRight;
    Result.W = Left.W * MixLeft + Right.W * MixRight;
#endif

    return Result;
}

static inline Rr_Quat Rr_NLerp(Rr_Quat Left, float Time, Rr_Quat Right)
{
    Rr_Quat Result = _Rr_MixQ(Left, 1.0f - Time, Right, Time);
    Result = Rr_NormQ(Result);

    return Result;
}

static inline Rr_Quat Rr_SLerp(Rr_Quat Left, float Time, Rr_Quat Right)
{
    Rr_Quat Result;

    float Cos_Theta = Rr_DotQ(Left, Right);

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
        float Angle = Rr_ACosF(Cos_Theta);
        float MixLeft = Rr_SinF((1.0f - Time) * Angle);
        float MixRight = Rr_SinF(Time * Angle);

        Result = _Rr_MixQ(Left, MixLeft, Right, MixRight);
        Result = Rr_NormQ(Result);
    }

    return Result;
}

static inline Rr_Mat4 Rr_QToM4(Rr_Quat Left)
{
    Rr_Mat4 Result;

    Rr_Quat NormalizedQ = Rr_NormQ(Left);

    float XX, YY, ZZ, XY, XZ, YZ, WX, WY, WZ;

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
static inline Rr_Quat Rr_M4ToQ_RH(Rr_Mat4 M)
{
    float T;
    Rr_Quat Q;

    if (M.Elements[2][2] < 0.0f)
    {
        if (M.Elements[0][0] > M.Elements[1][1])
        {
            T = 1 + M.Elements[0][0] - M.Elements[1][1] - M.Elements[2][2];
            Q = Rr_Q(
                T,
                M.Elements[0][1] + M.Elements[1][0],
                M.Elements[2][0] + M.Elements[0][2],
                M.Elements[1][2] - M.Elements[2][1]);
        }
        else
        {
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
            T = 1 - M.Elements[0][0] - M.Elements[1][1] + M.Elements[2][2];
            Q = Rr_Q(
                M.Elements[2][0] + M.Elements[0][2],
                M.Elements[1][2] + M.Elements[2][1],
                T,
                M.Elements[0][1] - M.Elements[1][0]);
        }
        else
        {
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

static inline Rr_Quat Rr_M4ToQ_LH(Rr_Mat4 M)
{
    float T;
    Rr_Quat Q;

    if (M.Elements[2][2] < 0.0f)
    {
        if (M.Elements[0][0] > M.Elements[1][1])
        {
            T = 1 + M.Elements[0][0] - M.Elements[1][1] - M.Elements[2][2];
            Q = Rr_Q(
                T,
                M.Elements[0][1] + M.Elements[1][0],
                M.Elements[2][0] + M.Elements[0][2],
                M.Elements[2][1] - M.Elements[1][2]);
        }
        else
        {
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
            T = 1 - M.Elements[0][0] - M.Elements[1][1] + M.Elements[2][2];
            Q = Rr_Q(
                M.Elements[2][0] + M.Elements[0][2],
                M.Elements[1][2] + M.Elements[2][1],
                T,
                M.Elements[1][0] - M.Elements[0][1]);
        }
        else
        {
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

static inline Rr_Quat Rr_QFromAxisAngle_RH(Rr_Vec3 Axis, float Angle)
{
    Rr_Quat Result;

    Rr_Vec3 AxisNormalized = Rr_NormV3(Axis);
    float SineOfRotation = Rr_SinF(Angle / 2.0f);

    Result.XYZ = Rr_MulV3F(AxisNormalized, SineOfRotation);
    Result.W = Rr_CosF(Angle / 2.0f);

    return Result;
}

static inline Rr_Quat Rr_QFromAxisAngle_LH(Rr_Vec3 Axis, float Angle)
{
    return Rr_QFromAxisAngle_RH(Axis, -Angle);
}

static inline Rr_Quat Rr_QFromNormPair(Rr_Vec3 Left, Rr_Vec3 Right)
{
    Rr_Quat Result;

    Result.XYZ = Rr_Cross(Left, Right);
    Result.W = 1.0f + Rr_DotV3(Left, Right);

    return Rr_NormQ(Result);
}

static inline Rr_Quat Rr_QFromVecPair(Rr_Vec3 Left, Rr_Vec3 Right)
{
    return Rr_QFromNormPair(Rr_NormV3(Left), Rr_NormV3(Right));
}

static inline Rr_Vec2 Rr_RotateV2(Rr_Vec2 V, float Angle)
{
    float sinA = Rr_SinF(Angle);
    float cosA = Rr_CosF(Angle);

    return Rr_V2(V.X * cosA - V.Y * sinA, V.X * sinA + V.Y * cosA);
}

// implementation from
// https://blog.molecular-matters.com/2013/05/24/a-faster-quaternion-vector-multiplication/
static inline Rr_Vec3 Rr_RotateV3Q(Rr_Vec3 V, Rr_Quat Q)
{
    Rr_Vec3 t = Rr_MulV3F(Rr_Cross(Q.XYZ, V), 2);
    return Rr_AddV3(V, Rr_AddV3(Rr_MulV3F(t, Q.W), Rr_Cross(Q.XYZ, t)));
}

static inline Rr_Vec3 Rr_RotateV3AxisAngle_LH(
    Rr_Vec3 V,
    Rr_Vec3 Axis,
    float Angle)
{
    return Rr_RotateV3Q(V, Rr_QFromAxisAngle_LH(Axis, Angle));
}

static inline Rr_Vec3 Rr_RotateV3AxisAngle_RH(
    Rr_Vec3 V,
    Rr_Vec3 Axis,
    float Angle)
{
    return Rr_RotateV3Q(V, Rr_QFromAxisAngle_RH(Axis, Angle));
}

/*
 * Rect Functions
 */

static inline bool Rr_RectContains(Rr_Rect const *Rect, Rr_Vec2 Point)
{
    return Point.X >= Rect->Offset.X &&
           Point.X < Rect->Offset.X + Rect->Extent.X &&
           Point.Y >= Rect->Offset.Y &&
           Point.Y < Rect->Offset.Y + Rect->Extent.Y;
}

static inline Rr_Vec2 Rr_RectCenter(Rr_Rect *Rect)
{
    return Rr_AddV2(Rect->Offset, Rr_MulV2F(Rect->Extent, 0.5f));
}

static inline Rr_Rect Rr_ResizeRect(Rr_Rect *Rect, float Amount)
{
    Rr_Rect Result = *Rect;
    Result.Offset = Rr_SubV2(Rect->Offset, Rr_V2(Amount, Amount));
    Result.Extent = Rr_AddV2(Rect->Extent, Rr_V2(Amount * 2.0f, Amount * 2.0f));
    return Result;
}

static inline Rr_Rect Rr_FitRect(Rr_Rect *Src, Rr_Rect *Dst)
{
    float X = 0;
    float Y = 0;
    float Width = 0;
    float Height = 0;

    float DstWidth = Dst->Extent.Width;
    float DstHeight = Dst->Extent.Height;
    float SrcWidth = Src->Extent.Width;
    float SrcHeight = Src->Extent.Height;
    float DstRatio = DstWidth / DstHeight;
    float SrcRatio = SrcWidth / SrcHeight;

    if (DstRatio > SrcRatio)
    {
        Width = (SrcWidth / SrcHeight) * DstHeight;
        Height = DstHeight;
    }
    else
    {
        Width = DstWidth;
        Height = (SrcHeight / SrcWidth) * DstWidth;
    }
    X = (DstWidth / 2.0f) - (Width / 2.0f);
    Y = (DstHeight / 2.0f) - (Height / 2.0f);

    Rr_Rect Result;
    Result.Offset.X = X;
    Result.Offset.Y = Y;
    Result.Extent.Width = Width;
    Result.Extent.Height = Height;

    return Result;
}

static Rr_IntRect Rr_FitIntRect(Rr_IntRect *Src, Rr_IntRect *Dst)
{
    float X = 0;
    float Y = 0;
    float Width = 0;
    float Height = 0;

    float DstWidth = (float)Dst->Extent.Width;
    float DstHeight = (float)Dst->Extent.Height;
    float SrcWidth = (float)Src->Extent.Width;
    float SrcHeight = (float)Src->Extent.Height;
    float DstRatio = DstWidth / DstHeight;
    float SrcRatio = SrcWidth / SrcHeight;

    if (DstRatio > SrcRatio)
    {
        Width = (SrcWidth / SrcHeight) * DstHeight;
        Height = DstHeight;
    }
    else
    {
        Width = DstWidth;
        Height = (SrcHeight / SrcWidth) * DstWidth;
    }
    X = (DstWidth / 2.0f) - (Width / 2.0f);
    Y = (DstHeight / 2.0f) - (Height / 2.0f);

    Rr_IntRect Result;
    Result.Offset.X = (int32_t)X;
    Result.Offset.Y = (int32_t)Y;
    Result.Extent.Width = (int32_t)Width;
    Result.Extent.Height = (int32_t)Height;

    return Result;
}

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

static inline float Rr_Len(Rr_Vec2 A)
{
    return Rr_LenV2(A);
}

static inline float Rr_Len(Rr_Vec3 A)
{
    return Rr_LenV3(A);
}

static inline float Rr_Len(Rr_Vec4 A)
{
    return Rr_LenV4(A);
}

static inline float Rr_LenSqr(Rr_Vec2 A)
{
    return Rr_LenSqrV2(A);
}

static inline float Rr_LenSqr(Rr_Vec3 A)
{
    return Rr_LenSqrV3(A);
}

static inline float Rr_LenSqr(Rr_Vec4 A)
{
    return Rr_LenSqrV4(A);
}

static inline Rr_Vec2 Rr_Norm(Rr_Vec2 A)
{
    return Rr_NormV2(A);
}

static inline Rr_Vec3 Rr_Norm(Rr_Vec3 A)
{
    return Rr_NormV3(A);
}

static inline Rr_Vec4 Rr_Norm(Rr_Vec4 A)
{
    return Rr_NormV4(A);
}

static inline Rr_Quat Rr_Norm(Rr_Quat A)
{
    return Rr_NormQ(A);
}

static inline float Rr_Dot(Rr_Vec2 Left, Rr_Vec2 VecTwo)
{
    return Rr_DotV2(Left, VecTwo);
}

static inline float Rr_Dot(Rr_Vec3 Left, Rr_Vec3 VecTwo)
{
    return Rr_DotV3(Left, VecTwo);
}

static inline float Rr_Dot(Rr_Vec4 Left, Rr_Vec4 VecTwo)
{
    return Rr_DotV4(Left, VecTwo);
}

static inline Rr_Vec2 Rr_Lerp(Rr_Vec2 Left, float Time, Rr_Vec2 Right)
{
    return Rr_LerpV2(Left, Time, Right);
}

static inline Rr_Vec3 Rr_Lerp(Rr_Vec3 Left, float Time, Rr_Vec3 Right)
{
    return Rr_LerpV3(Left, Time, Right);
}

static inline Rr_Vec4 Rr_Lerp(Rr_Vec4 Left, float Time, Rr_Vec4 Right)
{
    return Rr_LerpV4(Left, Time, Right);
}

static inline Rr_Mat2 Rr_Transpose(Rr_Mat2 Matrix)
{
    return Rr_TransposeM2(Matrix);
}

static inline Rr_Mat3 Rr_Transpose(Rr_Mat3 Matrix)
{
    return Rr_TransposeM3(Matrix);
}

static inline Rr_Mat4 Rr_Transpose(Rr_Mat4 Matrix)
{
    return Rr_TransposeM4(Matrix);
}

static inline float Rr_Determinant(Rr_Mat2 Matrix)
{
    return Rr_DeterminantM2(Matrix);
}

static inline float Rr_Determinant(Rr_Mat3 Matrix)
{
    return Rr_DeterminantM3(Matrix);
}

static inline float Rr_Determinant(Rr_Mat4 Matrix)
{
    return Rr_DeterminantM4(Matrix);
}

static inline Rr_Mat2 Rr_InvGeneral(Rr_Mat2 Matrix)
{
    return Rr_InvGeneralM2(Matrix);
}

static inline Rr_Mat3 Rr_InvGeneral(Rr_Mat3 Matrix)
{
    return Rr_InvGeneralM3(Matrix);
}

static inline Rr_Mat4 Rr_InvGeneral(Rr_Mat4 Matrix)
{
    return Rr_InvGeneralM4(Matrix);
}

static inline float Rr_Dot(Rr_Quat QuatOne, Rr_Quat QuatTwo)
{
    return Rr_DotQ(QuatOne, QuatTwo);
}

static inline Rr_Vec2 Rr_Add(Rr_Vec2 Left, Rr_Vec2 Right)
{
    return Rr_AddV2(Left, Right);
}

static inline Rr_Vec3 Rr_Add(Rr_Vec3 Left, Rr_Vec3 Right)
{
    return Rr_AddV3(Left, Right);
}

static inline Rr_Vec4 Rr_Add(Rr_Vec4 Left, Rr_Vec4 Right)
{
    return Rr_AddV4(Left, Right);
}

static inline Rr_Mat2 Rr_Add(Rr_Mat2 Left, Rr_Mat2 Right)
{
    return Rr_AddM2(Left, Right);
}

static inline Rr_Mat3 Rr_Add(Rr_Mat3 Left, Rr_Mat3 Right)
{
    return Rr_AddM3(Left, Right);
}

static inline Rr_Mat4 Rr_Add(Rr_Mat4 Left, Rr_Mat4 Right)
{
    return Rr_AddM4(Left, Right);
}

static inline Rr_Quat Rr_Add(Rr_Quat Left, Rr_Quat Right)
{
    return Rr_AddQ(Left, Right);
}

static inline Rr_Vec2 Rr_Sub(Rr_Vec2 Left, Rr_Vec2 Right)
{
    return Rr_SubV2(Left, Right);
}

static inline Rr_Vec3 Rr_Sub(Rr_Vec3 Left, Rr_Vec3 Right)
{
    return Rr_SubV3(Left, Right);
}

static inline Rr_Vec4 Rr_Sub(Rr_Vec4 Left, Rr_Vec4 Right)
{
    return Rr_SubV4(Left, Right);
}

static inline Rr_Mat2 Rr_Sub(Rr_Mat2 Left, Rr_Mat2 Right)
{
    return Rr_SubM2(Left, Right);
}

static inline Rr_Mat3 Rr_Sub(Rr_Mat3 Left, Rr_Mat3 Right)
{
    return Rr_SubM3(Left, Right);
}

static inline Rr_Mat4 Rr_Sub(Rr_Mat4 Left, Rr_Mat4 Right)
{
    return Rr_SubM4(Left, Right);
}

static inline Rr_Quat Rr_Sub(Rr_Quat Left, Rr_Quat Right)
{
    return Rr_SubQ(Left, Right);
}

static inline Rr_Vec2 Rr_Mul(Rr_Vec2 Left, Rr_Vec2 Right)
{
    return Rr_MulV2(Left, Right);
}

static inline Rr_Vec2 Rr_Mul(Rr_Vec2 Left, float Right)
{
    return Rr_MulV2F(Left, Right);
}

static inline Rr_Vec3 Rr_Mul(Rr_Vec3 Left, Rr_Vec3 Right)
{
    return Rr_MulV3(Left, Right);
}

static inline Rr_Vec3 Rr_Mul(Rr_Vec3 Left, float Right)
{
    return Rr_MulV3F(Left, Right);
}

static inline Rr_Vec4 Rr_Mul(Rr_Vec4 Left, Rr_Vec4 Right)
{
    return Rr_MulV4(Left, Right);
}

static inline Rr_Vec4 Rr_Mul(Rr_Vec4 Left, float Right)
{
    return Rr_MulV4F(Left, Right);
}

static inline Rr_Mat2 Rr_Mul(Rr_Mat2 Left, Rr_Mat2 Right)
{
    return Rr_MulM2(Left, Right);
}

static inline Rr_Mat3 Rr_Mul(Rr_Mat3 Left, Rr_Mat3 Right)
{
    return Rr_MulM3(Left, Right);
}

static inline Rr_Mat4 Rr_Mul(Rr_Mat4 Left, Rr_Mat4 Right)
{
    return Rr_MulM4(Left, Right);
}

static inline Rr_Mat2 Rr_Mul(Rr_Mat2 Left, float Right)
{
    return Rr_MulM2F(Left, Right);
}

static inline Rr_Mat3 Rr_Mul(Rr_Mat3 Left, float Right)
{
    return Rr_MulM3F(Left, Right);
}

static inline Rr_Mat4 Rr_Mul(Rr_Mat4 Left, float Right)
{
    return Rr_MulM4F(Left, Right);
}

static inline Rr_Vec2 Rr_Mul(Rr_Mat2 Matrix, Rr_Vec2 Vector)
{
    return Rr_MulM2V2(Matrix, Vector);
}

static inline Rr_Vec3 Rr_Mul(Rr_Mat3 Matrix, Rr_Vec3 Vector)
{
    return Rr_MulM3V3(Matrix, Vector);
}

static inline Rr_Vec4 Rr_Mul(Rr_Mat4 Matrix, Rr_Vec4 Vector)
{
    return Rr_MulM4V4(Matrix, Vector);
}

static inline Rr_Quat Rr_Mul(Rr_Quat Left, Rr_Quat Right)
{
    return Rr_MulQ(Left, Right);
}

static inline Rr_Quat Rr_Mul(Rr_Quat Left, float Right)
{
    return Rr_MulQF(Left, Right);
}

static inline Rr_Vec2 Rr_Div(Rr_Vec2 Left, Rr_Vec2 Right)
{
    return Rr_DivV2(Left, Right);
}

static inline Rr_Vec2 Rr_Div(Rr_Vec2 Left, float Right)
{
    return Rr_DivV2F(Left, Right);
}

static inline Rr_Vec3 Rr_Div(Rr_Vec3 Left, Rr_Vec3 Right)
{
    return Rr_DivV3(Left, Right);
}

static inline Rr_Vec3 Rr_Div(Rr_Vec3 Left, float Right)
{
    return Rr_DivV3F(Left, Right);
}

static inline Rr_Vec4 Rr_Div(Rr_Vec4 Left, Rr_Vec4 Right)
{
    return Rr_DivV4(Left, Right);
}

static inline Rr_Vec4 Rr_Div(Rr_Vec4 Left, float Right)
{
    return Rr_DivV4F(Left, Right);
}

static inline Rr_Mat2 Rr_Div(Rr_Mat2 Left, float Right)
{
    return Rr_DivM2F(Left, Right);
}

static inline Rr_Mat3 Rr_Div(Rr_Mat3 Left, float Right)
{
    return Rr_DivM3F(Left, Right);
}

static inline Rr_Mat4 Rr_Div(Rr_Mat4 Left, float Right)
{
    return Rr_DivM4F(Left, Right);
}

static inline Rr_Quat Rr_Div(Rr_Quat Left, float Right)
{
    return Rr_DivQF(Left, Right);
}

static inline bool Rr_Eq(Rr_Vec2 Left, Rr_Vec2 Right)
{
    return Rr_EqV2(Left, Right);
}

static inline bool Rr_Eq(Rr_Vec3 Left, Rr_Vec3 Right)
{
    return Rr_EqV3(Left, Right);
}

static inline bool Rr_Eq(Rr_Vec4 Left, Rr_Vec4 Right)
{
    return Rr_EqV4(Left, Right);
}

static inline Rr_Vec2 operator+(Rr_Vec2 Left, Rr_Vec2 Right)
{
    return Rr_AddV2(Left, Right);
}

static inline Rr_Vec3 operator+(Rr_Vec3 Left, Rr_Vec3 Right)
{
    return Rr_AddV3(Left, Right);
}

static inline Rr_Vec4 operator+(Rr_Vec4 Left, Rr_Vec4 Right)
{
    return Rr_AddV4(Left, Right);
}

static inline Rr_Mat2 operator+(Rr_Mat2 Left, Rr_Mat2 Right)
{
    return Rr_AddM2(Left, Right);
}

static inline Rr_Mat3 operator+(Rr_Mat3 Left, Rr_Mat3 Right)
{
    return Rr_AddM3(Left, Right);
}

static inline Rr_Mat4 operator+(Rr_Mat4 Left, Rr_Mat4 Right)
{
    return Rr_AddM4(Left, Right);
}

static inline Rr_Quat operator+(Rr_Quat Left, Rr_Quat Right)
{
    return Rr_AddQ(Left, Right);
}

static inline Rr_Vec2 operator-(Rr_Vec2 Left, Rr_Vec2 Right)
{
    return Rr_SubV2(Left, Right);
}

static inline Rr_Vec3 operator-(Rr_Vec3 Left, Rr_Vec3 Right)
{
    return Rr_SubV3(Left, Right);
}

static inline Rr_Vec4 operator-(Rr_Vec4 Left, Rr_Vec4 Right)
{
    return Rr_SubV4(Left, Right);
}

static inline Rr_Mat2 operator-(Rr_Mat2 Left, Rr_Mat2 Right)
{
    return Rr_SubM2(Left, Right);
}

static inline Rr_Mat3 operator-(Rr_Mat3 Left, Rr_Mat3 Right)
{
    return Rr_SubM3(Left, Right);
}

static inline Rr_Mat4 operator-(Rr_Mat4 Left, Rr_Mat4 Right)
{
    return Rr_SubM4(Left, Right);
}

static inline Rr_Quat operator-(Rr_Quat Left, Rr_Quat Right)
{
    return Rr_SubQ(Left, Right);
}

static inline Rr_Vec2 operator*(Rr_Vec2 Left, Rr_Vec2 Right)
{
    return Rr_MulV2(Left, Right);
}

static inline Rr_Vec3 operator*(Rr_Vec3 Left, Rr_Vec3 Right)
{
    return Rr_MulV3(Left, Right);
}

static inline Rr_Vec4 operator*(Rr_Vec4 Left, Rr_Vec4 Right)
{
    return Rr_MulV4(Left, Right);
}

static inline Rr_Mat2 operator*(Rr_Mat2 Left, Rr_Mat2 Right)
{
    return Rr_MulM2(Left, Right);
}

static inline Rr_Mat3 operator*(Rr_Mat3 Left, Rr_Mat3 Right)
{
    return Rr_MulM3(Left, Right);
}

static inline Rr_Mat4 operator*(Rr_Mat4 Left, Rr_Mat4 Right)
{
    return Rr_MulM4(Left, Right);
}

static inline Rr_Quat operator*(Rr_Quat Left, Rr_Quat Right)
{
    return Rr_MulQ(Left, Right);
}

static inline Rr_Vec2 operator*(Rr_Vec2 Left, float Right)
{
    return Rr_MulV2F(Left, Right);
}

static inline Rr_Vec3 operator*(Rr_Vec3 Left, float Right)
{
    return Rr_MulV3F(Left, Right);
}

static inline Rr_Vec4 operator*(Rr_Vec4 Left, float Right)
{
    return Rr_MulV4F(Left, Right);
}

static inline Rr_Mat2 operator*(Rr_Mat2 Left, float Right)
{
    return Rr_MulM2F(Left, Right);
}

static inline Rr_Mat3 operator*(Rr_Mat3 Left, float Right)
{
    return Rr_MulM3F(Left, Right);
}

static inline Rr_Mat4 operator*(Rr_Mat4 Left, float Right)
{
    return Rr_MulM4F(Left, Right);
}

static inline Rr_Quat operator*(Rr_Quat Left, float Right)
{
    return Rr_MulQF(Left, Right);
}

static inline Rr_Vec2 operator*(float Left, Rr_Vec2 Right)
{
    return Rr_MulV2F(Right, Left);
}

static inline Rr_Vec3 operator*(float Left, Rr_Vec3 Right)
{
    return Rr_MulV3F(Right, Left);
}

static inline Rr_Vec4 operator*(float Left, Rr_Vec4 Right)
{
    return Rr_MulV4F(Right, Left);
}

static inline Rr_Mat2 operator*(float Left, Rr_Mat2 Right)
{
    return Rr_MulM2F(Right, Left);
}

static inline Rr_Mat3 operator*(float Left, Rr_Mat3 Right)
{
    return Rr_MulM3F(Right, Left);
}

static inline Rr_Mat4 operator*(float Left, Rr_Mat4 Right)
{
    return Rr_MulM4F(Right, Left);
}

static inline Rr_Quat operator*(float Left, Rr_Quat Right)
{
    return Rr_MulQF(Right, Left);
}

static inline Rr_Vec2 operator*(Rr_Mat2 Matrix, Rr_Vec2 Vector)
{
    return Rr_MulM2V2(Matrix, Vector);
}

static inline Rr_Vec3 operator*(Rr_Mat3 Matrix, Rr_Vec3 Vector)
{
    return Rr_MulM3V3(Matrix, Vector);
}

static inline Rr_Vec4 operator*(Rr_Mat4 Matrix, Rr_Vec4 Vector)
{
    return Rr_MulM4V4(Matrix, Vector);
}

static inline Rr_Vec2 operator/(Rr_Vec2 Left, Rr_Vec2 Right)
{
    return Rr_DivV2(Left, Right);
}

static inline Rr_Vec3 operator/(Rr_Vec3 Left, Rr_Vec3 Right)
{
    return Rr_DivV3(Left, Right);
}

static inline Rr_Vec4 operator/(Rr_Vec4 Left, Rr_Vec4 Right)
{
    return Rr_DivV4(Left, Right);
}

static inline Rr_Vec2 operator/(Rr_Vec2 Left, float Right)
{
    return Rr_DivV2F(Left, Right);
}

static inline Rr_Vec3 operator/(Rr_Vec3 Left, float Right)
{
    return Rr_DivV3F(Left, Right);
}

static inline Rr_Vec4 operator/(Rr_Vec4 Left, float Right)
{
    return Rr_DivV4F(Left, Right);
}

static inline Rr_Mat4 operator/(Rr_Mat4 Left, float Right)
{
    return Rr_DivM4F(Left, Right);
}

static inline Rr_Mat3 operator/(Rr_Mat3 Left, float Right)
{
    return Rr_DivM3F(Left, Right);
}

static inline Rr_Mat2 operator/(Rr_Mat2 Left, float Right)
{
    return Rr_DivM2F(Left, Right);
}

static inline Rr_Quat operator/(Rr_Quat Left, float Right)
{
    return Rr_DivQF(Left, Right);
}

static inline Rr_Vec2 &operator+=(Rr_Vec2 &Left, Rr_Vec2 Right)
{
    return Left = Left + Right;
}

static inline Rr_Vec3 &operator+=(Rr_Vec3 &Left, Rr_Vec3 Right)
{
    return Left = Left + Right;
}

static inline Rr_Vec4 &operator+=(Rr_Vec4 &Left, Rr_Vec4 Right)
{
    return Left = Left + Right;
}

static inline Rr_Mat2 &operator+=(Rr_Mat2 &Left, Rr_Mat2 Right)
{
    return Left = Left + Right;
}

static inline Rr_Mat3 &operator+=(Rr_Mat3 &Left, Rr_Mat3 Right)
{
    return Left = Left + Right;
}

static inline Rr_Mat4 &operator+=(Rr_Mat4 &Left, Rr_Mat4 Right)
{
    return Left = Left + Right;
}

static inline Rr_Quat &operator+=(Rr_Quat &Left, Rr_Quat Right)
{
    return Left = Left + Right;
}

static inline Rr_Vec2 &operator-=(Rr_Vec2 &Left, Rr_Vec2 Right)
{
    return Left = Left - Right;
}

static inline Rr_Vec3 &operator-=(Rr_Vec3 &Left, Rr_Vec3 Right)
{
    return Left = Left - Right;
}

static inline Rr_Vec4 &operator-=(Rr_Vec4 &Left, Rr_Vec4 Right)
{
    return Left = Left - Right;
}

static inline Rr_Mat2 &operator-=(Rr_Mat2 &Left, Rr_Mat2 Right)
{
    return Left = Left - Right;
}

static inline Rr_Mat3 &operator-=(Rr_Mat3 &Left, Rr_Mat3 Right)
{
    return Left = Left - Right;
}

static inline Rr_Mat4 &operator-=(Rr_Mat4 &Left, Rr_Mat4 Right)
{
    return Left = Left - Right;
}

static inline Rr_Quat &operator-=(Rr_Quat &Left, Rr_Quat Right)
{
    return Left = Left - Right;
}

static inline Rr_Vec2 &operator*=(Rr_Vec2 &Left, Rr_Vec2 Right)
{
    return Left = Left * Right;
}

static inline Rr_Vec3 &operator*=(Rr_Vec3 &Left, Rr_Vec3 Right)
{
    return Left = Left * Right;
}

static inline Rr_Vec4 &operator*=(Rr_Vec4 &Left, Rr_Vec4 Right)
{
    return Left = Left * Right;
}

static inline Rr_Vec2 &operator*=(Rr_Vec2 &Left, float Right)
{
    return Left = Left * Right;
}

static inline Rr_Vec3 &operator*=(Rr_Vec3 &Left, float Right)
{
    return Left = Left * Right;
}

static inline Rr_Vec4 &operator*=(Rr_Vec4 &Left, float Right)
{
    return Left = Left * Right;
}

static inline Rr_Mat2 &operator*=(Rr_Mat2 &Left, float Right)
{
    return Left = Left * Right;
}

static inline Rr_Mat3 &operator*=(Rr_Mat3 &Left, float Right)
{
    return Left = Left * Right;
}

static inline Rr_Mat4 &operator*=(Rr_Mat4 &Left, float Right)
{
    return Left = Left * Right;
}

static inline Rr_Quat &operator*=(Rr_Quat &Left, float Right)
{
    return Left = Left * Right;
}

static inline Rr_Vec2 &operator/=(Rr_Vec2 &Left, Rr_Vec2 Right)
{
    return Left = Left / Right;
}

static inline Rr_Vec3 &operator/=(Rr_Vec3 &Left, Rr_Vec3 Right)
{
    return Left = Left / Right;
}

static inline Rr_Vec4 &operator/=(Rr_Vec4 &Left, Rr_Vec4 Right)
{
    return Left = Left / Right;
}

static inline Rr_Vec2 &operator/=(Rr_Vec2 &Left, float Right)
{
    return Left = Left / Right;
}

static inline Rr_Vec3 &operator/=(Rr_Vec3 &Left, float Right)
{
    return Left = Left / Right;
}

static inline Rr_Vec4 &operator/=(Rr_Vec4 &Left, float Right)
{
    return Left = Left / Right;
}

static inline Rr_Mat4 &operator/=(Rr_Mat4 &Left, float Right)
{
    return Left = Left / Right;
}

static inline Rr_Quat &operator/=(Rr_Quat &Left, float Right)
{
    return Left = Left / Right;
}

static inline bool operator==(Rr_Vec2 Left, Rr_Vec2 Right)
{
    return Rr_EqV2(Left, Right);
}

static inline bool operator==(Rr_Vec3 Left, Rr_Vec3 Right)
{
    return Rr_EqV3(Left, Right);
}

static inline bool operator==(Rr_Vec4 Left, Rr_Vec4 Right)
{
    return Rr_EqV4(Left, Right);
}

static inline bool operator!=(Rr_Vec2 Left, Rr_Vec2 Right)
{
    return !Rr_EqV2(Left, Right);
}

static inline bool operator!=(Rr_Vec3 Left, Rr_Vec3 Right)
{
    return !Rr_EqV3(Left, Right);
}

static inline bool operator!=(Rr_Vec4 Left, Rr_Vec4 Right)
{
    return !Rr_EqV4(Left, Right);
}

static inline Rr_Vec2 operator-(Rr_Vec2 In)
{
    Rr_Vec2 Result;
    Result.X = -In.X;
    Result.Y = -In.Y;

    return Result;
}

static inline Rr_Vec3 operator-(Rr_Vec3 In)
{
    Rr_Vec3 Result;
    Result.X = -In.X;
    Result.Y = -In.Y;
    Result.Z = -In.Z;

    return Result;
}

static inline Rr_Vec4 operator-(Rr_Vec4 In)
{
    Rr_Vec4 Result;
#if RR_MATH__USE_SSE
    Result.SSE = _mm_xor_ps(In.SSE, _mm_set1_ps(-0.0f));
#elif defined(RR_MATH__USE_NEON)
    float32x4_t Zero = vdupq_n_f32(0.0f);
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

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#endif
