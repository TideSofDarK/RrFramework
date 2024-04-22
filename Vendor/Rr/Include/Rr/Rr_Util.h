#pragma once

#include <math.h>

static inline size_t Rr_Align(size_t Value, size_t Alignment)
{
    return (Value + Alignment - 1) & ~(Alignment - 1);
}
