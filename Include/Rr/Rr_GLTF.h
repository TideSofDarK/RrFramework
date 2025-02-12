#pragma once

#include <Rr/Rr_App.h>
#include <Rr/Rr_Renderer.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    RR_GLTF_PRIMITIVE_TYPE_INVALID,
    RR_GLTF_PRIMITIVE_TYPE_POSITION,
    RR_GLTF_PRIMITIVE_TYPE_TEXCOORD0,
    RR_GLTF_PRIMITIVE_TYPE_NORMAL,
    RR_GLTF_PRIMITIVE_TYPE_COLOR,
    RR_GLTF_PRIMITIVE_TYPE_TANGENT,
} Rr_GLTFPrimitiveType;

typedef struct Rr_GLTFMaterial Rr_GLTFMaterial;
struct Rr_GLTFMaterial
{
    struct Rr_Image *Image;
};

typedef struct Rr_GLTFAttribute Rr_GLTFAttribute;
struct Rr_GLTFAttribute
{
    Rr_GLTFPrimitiveType Type;
    Rr_Format Format;
    size_t BufferOffset;
    struct Rr_Buffer *Buffer;
};

typedef struct Rr_GLTFPrimitive Rr_GLTFPrimitive;
struct Rr_GLTFPrimitive
{
    size_t VertexCount;
    size_t AttributeCount;
    Rr_GLTFAttribute *Attributes;
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
    size_t MeshCount;
    Rr_GLTFMesh *Meshes;
    size_t BufferCount;
    struct Rr_Buffer **Buffers;
    size_t ImageCount;
    struct Rr_Image **Images;
};

#ifdef __cplusplus
}
#endif
