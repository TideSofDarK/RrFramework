#pragma once

#include "Rr_Math.h"

#ifdef __cplusplus
extern "C" {
#endif

extern float Rr_WrapMax(float X, float Max);

extern float Rr_WrapMinMax(float X, float Min, float Max);

extern uint16_t Rr_FloatToHalf(uint32_t X);

extern void Rr_PackVec4(Rr_Vec4 From, uint32_t *OutA, uint32_t *OutB);

#ifdef __cplusplus
}
#endif
