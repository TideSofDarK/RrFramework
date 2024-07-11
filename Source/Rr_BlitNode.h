#pragma once

#include "Rr/Rr_BlitNode.h"
#include "Rr/Rr_Image.h"
#include "Rr/Rr_Math.h"

typedef struct Rr_BlitNodeInfo Rr_BlitNodeInfo;
struct Rr_BlitNodeInfo
{
    Rr_Image *SrcImage;
    Rr_Image *DstImage;
    Rr_IntVec4 SrcRect;
    Rr_IntVec4 DstRect;
};

typedef struct Rr_BlitNode Rr_BlitNode;
struct Rr_BlitNode
{
    Rr_BlitNodeInfo Info;
};
