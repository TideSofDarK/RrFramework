#pragma once

#include <Rr/Rr_Input.h>

#include <vector>

enum EInputAction
{
    EIA_UP,
    EIA_DOWN,
    EIA_LEFT,
    EIA_RIGHT,
    EIA_FULLSCREEN,
    EIA_DEBUGOVERLAY,
    EIA_TEST,
    EIA_CAMERA,
    EIA_COUNT,
};

static std::vector<Rr_InputMapping> InputMappings = {
    { RR_SCANCODE_W, RR_SCANCODE_UNKNOWN },
    { RR_SCANCODE_S, RR_SCANCODE_UNKNOWN },
    { RR_SCANCODE_A, RR_SCANCODE_UNKNOWN },
    { RR_SCANCODE_D, RR_SCANCODE_UNKNOWN },
    { RR_SCANCODE_F11, RR_SCANCODE_UNKNOWN },
    { RR_SCANCODE_F1, RR_SCANCODE_UNKNOWN },
    { RR_SCANCODE_F2, RR_SCANCODE_UNKNOWN },
    { RR_SCANCODE_F3, RR_SCANCODE_UNKNOWN },
};
