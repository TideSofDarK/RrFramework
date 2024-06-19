#pragma once

#include "Rr_Math.h"

#ifdef __cplusplus
extern "C" {
#endif

extern Rr_F32 Rr_WrapMax(Rr_F32 X, Rr_F32 Max);

extern Rr_F32 Rr_WrapMinMax(Rr_F32 X, Rr_F32 Min, Rr_F32 Max);

extern Rr_U16 Rr_FloatToHalf(Rr_U32 X);

extern void Rr_PackVec4(Rr_Vec4 From, Rr_U32 *OutA, Rr_U32 *OutB);

#ifdef __cplusplus
}
#endif
