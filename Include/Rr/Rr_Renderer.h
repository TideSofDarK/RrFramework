#pragma once

#include <Rr/Rr_App.h>
#include <Rr/Rr_Memory.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Rr_Image;
struct Rr_Graph;

typedef enum
{
    RR_FORMAT_INVALID,
    RR_FORMAT_FLOAT,
    RR_FORMAT_UINT,
    RR_FORMAT_VEC2,
    RR_FORMAT_VEC3,
    RR_FORMAT_VEC4,
} Rr_Format;

typedef enum
{
    RR_INDEX_TYPE_UINT8,
    RR_INDEX_TYPE_UINT16,
    RR_INDEX_TYPE_UINT32,
} Rr_IndexType;

typedef enum
{
    RR_LOAD_OP_LOAD,
    RR_LOAD_OP_CLEAR,
    RR_LOAD_OP_DONT_CARE,
} Rr_LoadOp;

typedef enum
{
    RR_STORE_OP_STORE,
    RR_STORE_OP_DONT_CARE,
    RR_STORE_OP_RESOLVE,
    RR_STORE_OP_RESOLVE_AND_STORE,
} Rr_StoreOp;

typedef enum
{
    RR_COMPARE_OP_INVALID,
    RR_COMPARE_OP_NEVER,
    RR_COMPARE_OP_LESS,
    RR_COMPARE_OP_EQUAL,
    RR_COMPARE_OP_LESS_OR_EQUAL,
    RR_COMPARE_OP_GREATER,
    RR_COMPARE_OP_NOT_EQUAL,
    RR_COMPARE_OP_GREATER_OR_EQUAL,
    RR_COMPARE_OP_ALWAYS,
} Rr_CompareOp;

typedef enum
{
    RR_COLOR_COMPONENT_R = (1 << 0),
    RR_COLOR_COMPONENT_G = (1 << 1),
    RR_COLOR_COMPONENT_B = (1 << 2),
    RR_COLOR_COMPONENT_A = (1 << 3),
    RR_COLOR_COMPONENT_ALL = RR_COLOR_COMPONENT_R | RR_COLOR_COMPONENT_G | RR_COLOR_COMPONENT_B | RR_COLOR_COMPONENT_A,
} Rr_ColorComponent;

typedef enum
{
    RR_TEXTURE_FORMAT_UNDEFINED,
    RR_TEXTURE_FORMAT_R8G8B8A8_UNORM,
    RR_TEXTURE_FORMAT_B8G8R8A8_UNORM,
    RR_TEXTURE_FORMAT_D32_SFLOAT,
    RR_TEXTURE_FORMAT_D24_UNORM_S8_UINT,
    RR_TEXTURE_FORMAT_D32_SFLOAT_S8_UINT
} Rr_TextureFormat;

typedef enum
{
    RR_SHADER_STAGE_VERTEX_BIT = (1 << 0),
    RR_SHADER_STAGE_FRAGMENT_BIT = (1 << 1),
} Rr_ShaderStageBits;
typedef uint32_t Rr_ShaderStage;

extern struct Rr_Graph *Rr_GetGraph(Rr_App *App);

extern Rr_Arena *Rr_GetFrameArena(Rr_App *App);

extern Rr_TextureFormat Rr_GetSwapchainFormat(Rr_App *App);

extern struct Rr_Image *Rr_GetSwapchainImage(Rr_App *App);

extern size_t Rr_GetUniformAlignment(Rr_App *App);

extern size_t Rr_GetStorageAlignment(Rr_App *App);

#ifdef __cplusplus
}
#endif
