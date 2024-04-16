#pragma once

#include <math.h>

static inline float Rr_GetVerticalFoV(float HorizontalFoV)
{
    return 2.0f * atanf((tanf(HorizontalFoV/2.0f) * (9.0f/16.0f)));
}
