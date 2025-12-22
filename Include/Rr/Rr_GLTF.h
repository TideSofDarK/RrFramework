/*
 * MIT License
 *
 * Copyright (c) 2024-2025 Alexandr Semenov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef RR_GLTF_H
#define RR_GLTF_H

#include <Rr/Rr_App.h>
#include <Rr/Rr_Buffer.h>
#include <Rr/Rr_Image.h>
#include <Rr/Rr_Pipeline.h>
#include <Rr/Rr_Renderer.h>

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
    uint32_t VertexCount;
    size_t AttributeCount;
    Rr_GLTFAttribute *Attributes;
    Rr_GLTFMaterial *Material;
    uint32_t IndexCount;
    int32_t VertexOffset;
    uint32_t FirstIndex;
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
    char *Name;
    Rr_GLTFNode *Parent;
    size_t ChildrenCount;
    Rr_GLTFNode **Children;
    Rr_GLTFMesh *Mesh;
    Rr_Mat4 Transform;
};

typedef struct Rr_GLTFScene Rr_GLTFScene;
struct Rr_GLTFScene
{
    char *Name;
    size_t NodeCount;
    Rr_GLTFNode **Nodes;
};

typedef struct Rr_GLTFAsset Rr_GLTFAsset;
struct Rr_GLTFAsset
{
    size_t SceneCount;
    Rr_GLTFScene *Scenes;
    size_t MeshCount;
    Rr_GLTFMesh *Meshes;
    size_t NodeCount;
    Rr_GLTFNode *Nodes;
    Rr_Buffer *Buffer;
    size_t ImageCount;
    Rr_Image2D **Images;
    size_t MaterialCount;
    Rr_GLTFMaterial *Materials;
    size_t VertexBufferOffset;
    size_t IndexBufferOffset;
    Rr_IndexType IndexType;
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
    size_t Set;
    size_t Binding;
    Rr_GLTFTextureType TextureType;
};

RR_EXTERN Rr_GLTFContext *Rr_CreateGLTFContext(
    size_t VertexInputBindingCount,
    const Rr_VertexInputBinding *VertexInputBindings,
    const Rr_GLTFVertexInputBinding *GLTFVertexInputBindings,
    size_t GLTFTextureMappingCount,
    const Rr_GLTFTextureMapping *GLTFTextureMappings);

RR_EXTERN void Rr_ReleaseGLTFContext(Rr_GLTFContext *GLTFContext);

RR_EXTERN Rr_GLTFAsset *Rr_CreateGLTFAsset(
    Rr_GLTFContext *GLTFContext,
    struct Rr_Graph *Graph,
    size_t GLBDataSize,
    const void *GLBData);

#endif