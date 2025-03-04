#pragma once

#include <Rr/Rr_Math.h>

#ifdef __cplusplus
extern "C" {
#endif

extern float Rr_WrapMax(float X, float Max);

extern float Rr_WrapMinMax(float X, float Min, float Max);

extern uint16_t Rr_FloatToHalf(uint32_t X);

extern void Rr_PackVec4(Rr_Vec4 From, uint32_t *OutA, uint32_t *OutB);

extern Rr_Vec4 Rr_FitRect(Rr_Vec4 Src, Rr_Vec4 Dst);

extern Rr_IntVec4 Rr_FitIntRect(Rr_IntVec4 Src, Rr_IntVec4 Dst);

#ifdef __cplusplus
}
#endif
