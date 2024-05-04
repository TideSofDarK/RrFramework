#pragma once

#include <cglm/mat4.h>

#ifdef __cplusplus
extern "C"
{
#endif

static float Rr_GetVerticalFoV(const float HorizontalFoV, const float Aspect)
{
    return 2.0f * atanf((tanf(HorizontalFoV / 2.0f) * Aspect));
}

static void Rr_Perspective(mat4 Out, const float HorizontalFoV, const float Aspect)
{
    glm_mat4_zero(Out);
    const float FoV = Rr_GetVerticalFoV(HorizontalFoV, 1.0f / Aspect);
    const float Far = 50.0f;
    const float Near = 0.5f;
    Out[0][0] = (1.0f / Aspect) / tanf(FoV / 2.0f);
    Out[1][1] = 1.0f / tanf(FoV / 2.0f);
    Out[2][2] = Far / (Far - Near);
    Out[3][2] = -((Near * Far) / (Far - Near));
    Out[2][3] = 1.0f;
}

static void Rr_VulkanMatrix(mat4 Out)
{
    glm_mat4_zero(Out);
    Out[0][0] = 1.0f;
    Out[1][1] = -1.0f;
    Out[2][2] = -1.0f;
    Out[3][3] = 1.0f;
}

#ifdef __cplusplus
}
#endif
