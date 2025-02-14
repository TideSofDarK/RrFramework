#pragma once

#include <Rr/Rr_App.h>
#include <Rr/Rr_Renderer.h>
#include <Rr/Rr_Pipeline.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_GLTFContext Rr_GLTFContext;

typedef enum
{
    RR_GLTF_ATTRIBUTE_TYPE_INVALID,
    RR_GLTF_ATTRIBUTE_TYPE_POSITION,
    RR_GLTF_ATTRIBUTE_TYPE_TEXCOORD0,
    RR_GLTF_ATTRIBUTE_TYPE_NORMAL,
    RR_GLTF_ATTRIBUTE_TYPE_COLOR,
    RR_GLTF_ATTRIBUTE_TYPE_TANGENT,
} Rr_GLTFAttributeType;

typedef struct Rr_GLTFMaterial Rr_GLTFMaterial;
struct Rr_GLTFMaterial
{
    struct Rr_Image *Image;
};

typedef struct Rr_GLTFAttribute Rr_GLTFAttribute;
struct Rr_GLTFAttribute
{
    Rr_GLTFAttributeType Type;
    size_t BufferOffset;
    struct Rr_Buffer *Buffer;
};

typedef struct Rr_GLTFPrimitive Rr_GLTFPrimitive;
struct Rr_GLTFPrimitive
{
    size_t VertexCount;
    size_t AttributeCount;
    Rr_GLTFAttribute *Attributes;
    Rr_IndexType IndexType;
    size_t IndexCount;
    size_t IndexBufferOffset;
    struct Rr_Buffer *IndexBuffer;
    Rr_GLTFMaterial *Material;
};

typedef struct Rr_GLTFMesh Rr_GLTFMesh;
struct Rr_GLTFMesh
{
    size_t PrimitiveCount;
    Rr_GLTFPrimitive *Primitives;
    const char *Name;
};

typedef struct Rr_GLTFNode Rr_GLTFNode;
struct Rr_GLTFNode
{
    size_t MeshIndex;
    const char *Name;
};

typedef struct Rr_GLTFScene Rr_GLTFScene;
struct Rr_GLTFScene
{
    size_t NodeCount;
    Rr_GLTFNode *Nodes;
};

typedef struct Rr_GLTFAsset Rr_GLTFAsset;
struct Rr_GLTFAsset
{
    size_t SceneCount;
    Rr_GLTFScene *Scenes;
    size_t MeshCount;
    Rr_GLTFMesh *Meshes;
    size_t BufferCount;
    struct Rr_Buffer **Buffers;
    size_t ImageCount;
    struct Rr_Image **Images;
};

typedef struct Rr_GLTFVertexInputBinding Rr_GLTFVertexInputBinding;
struct Rr_GLTFVertexInputBinding
{
    size_t AttributeCount;
    Rr_GLTFAttributeType *Attributes;
};

extern Rr_GLTFContext *Rr_CreateGLTFContext(Rr_App *App, size_t VertexInputBindingCount, Rr_VertexInputBinding *VertexInputBindings, Rr_GLTFVertexInputBinding *GLTFVertexInputBindings);

extern void Rr_DestroyGLTFContext(Rr_App *App, Rr_GLTFContext *GLTFContext);

#ifdef __cplusplus
}
#endif
