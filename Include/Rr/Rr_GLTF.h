#pragma once

#include <Rr/Rr_App.h>
#include <Rr/Rr_Buffer.h>
#include <Rr/Rr_Image.h>
#include <Rr/Rr_Pipeline.h>
#include <Rr/Rr_Renderer.h>

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

typedef enum
{
    RR_GLTF_TEXTURE_TYPE_INVALID,
    RR_GLTF_TEXTURE_TYPE_COLOR,
    RR_GLTF_TEXTURE_TYPE_NORMAL,
    RR_GLTF_TEXTURE_TYPE_METALLIC_ROUGHNESS,
} Rr_GLTFTextureType;

typedef struct Rr_GLTFMaterial Rr_GLTFMaterial;
struct Rr_GLTFMaterial
{
    size_t TextureCount;
    size_t *Textures;
    Rr_GLTFTextureType *TextureTypes;
};

typedef struct Rr_GLTFAttribute Rr_GLTFAttribute;
struct Rr_GLTFAttribute
{
    Rr_GLTFAttributeType Type;
};

typedef struct Rr_GLTFPrimitive Rr_GLTFPrimitive;
struct Rr_GLTFPrimitive
{
    size_t VertexCount;
    size_t AttributeCount;
    Rr_GLTFAttribute *Attributes;
    size_t *VertexBufferOffsets;
    Rr_IndexType IndexType;
    size_t IndexCount;
    size_t IndexBufferOffset;
    Rr_GLTFMaterial *Material;
};

typedef struct Rr_GLTFMesh Rr_GLTFMesh;
struct Rr_GLTFMesh
{
    size_t PrimitiveCount;
    Rr_GLTFPrimitive *Primitives;
    char *Name;
};

typedef struct Rr_GLTFNode Rr_GLTFNode;
struct Rr_GLTFNode
{
    size_t MeshIndex;
    char *Name;
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
    Rr_Buffer *Buffer;
    size_t ImageCount;
    Rr_Image **Images;
    size_t MaterialCount;
    Rr_GLTFMaterial *Materials;
};

typedef struct Rr_GLTFVertexInputBinding Rr_GLTFVertexInputBinding;
struct Rr_GLTFVertexInputBinding
{
    size_t AttributeTypeCount;
    Rr_GLTFAttributeType *AttributeTypes;
};

typedef struct Rr_GLTFTextureMapping Rr_GLTFTextureMapping;
struct Rr_GLTFTextureMapping
{
    Rr_GLTFTextureType TextureType;
    size_t Set;
    size_t Binding;
};

extern Rr_GLTFContext *Rr_CreateGLTFContext(
    Rr_App *App,
    size_t VertexInputBindingCount,
    Rr_VertexInputBinding *VertexInputBindings,
    Rr_GLTFVertexInputBinding *GLTFVertexInputBindings,
    size_t GLTFTextureMappingCount,
    Rr_GLTFTextureMapping *GLTFTextureMappings);

extern void Rr_DestroyGLTFContext(Rr_App *App, Rr_GLTFContext *GLTFContext);

#ifdef __cplusplus
}
#endif
