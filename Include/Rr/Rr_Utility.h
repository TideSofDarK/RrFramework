#pragma once

#include "Rr_Math.h"

#ifdef __cplusplus
extern "C"
{
#endif

extern f32 Rr_WrapMax(f32 X, f32 Max);
extern f32 Rr_WrapMinMax(f32 X, f32 Min, f32 Max);

extern u16 Rr_FloatToHalf(u32 X);
extern void Rr_PackVec4(Rr_Vec4 From, u32* OutA, u32* OutB);

#ifdef __cplusplus
}
#endif
