#include "Rr/RrAsset.h"

RrAsset_Define(DoorFrameOBJ, "../Asset/Test.obj")
    RrAsset_Define(TriangleVERT, "../Asset/triangle.vert.spv")
        RrAsset_Define(TriangleFRAG, "../Asset/triangle.frag.spv")
            RrAsset_Define(TestCOMP, "../Asset/test.comp.spv")
