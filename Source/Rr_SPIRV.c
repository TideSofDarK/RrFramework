#include "Rr_SPIRV.h"

#define RR_LOG_MACRO_CATEGORY RR_LOG_CATEGORY_SPIRV
#include "Rr_LogMacro.h"

#include "Rr_Hash.h"

#include <Rr/Rr_Thread.h>

typedef struct Rr_SPIRVHeader Rr_SPIRVHeader;
struct Rr_SPIRVHeader
{
    uint32_t Magic;
    uint32_t Version;
    uint32_t Generator;
    uint32_t Bound;
    uint32_t Schema;
};

typedef struct Rr_SPIRVConstant Rr_SPIRVConstant;
struct Rr_SPIRVConstant
{
    uint32_t TypeID;
    uint32_t Value;
};

typedef struct Rr_SPIRVVariable Rr_SPIRVVariable;
struct Rr_SPIRVVariable
{
    uint32_t PointerID;
    uint8_t Binding;         /* Comes from OpDecorate! */
    uint8_t DescriptorSet;   /* Comes from OpDecorate! */
    uint8_t NonWritable : 1; /* Comes from OpDecorate! */
    uint8_t NonReadable : 1; /* Comes from OpDecorate! */
    uint8_t StorageClassStorageBuffer : 1;
};

typedef struct Rr_SPIRVPointer Rr_SPIRVPointer;
struct Rr_SPIRVPointer
{
    uint32_t TypeID;
    uint16_t StorageClass;
};

typedef struct Rr_SPIRVArray Rr_SPIRVArray;
struct Rr_SPIRVArray
{
    uint32_t ElementTypeID;
    uint32_t LengthID;
};

typedef struct Rr_SPIRVImage Rr_SPIRVImage;
struct Rr_SPIRVImage
{
    uint8_t Dimension;
    uint8_t Arrayed;
    uint8_t Sampled; /* 0 means known at run time.
                      * 1 means sampled.
                      * 2 means storage or subpass data image. */
    uint8_t Format;  /* Use Rr_GetSPIRVImageFormat() */
};

typedef struct Rr_SPIRVStruct Rr_SPIRVStruct;
struct Rr_SPIRVStruct
{
    uint16_t Block;
    uint16_t BufferBlock;
};

typedef struct Rr_SPIRVOp Rr_SPIRVOp;
struct Rr_SPIRVOp
{
    union
    {
        Rr_SPIRVConstant Constant;
        Rr_SPIRVVariable Variable;
        Rr_SPIRVPointer Pointer;
        Rr_SPIRVArray Array;
        Rr_SPIRVImage Image;
        Rr_SPIRVStruct Struct;
    } Union;
    uint16_t OpCode;
};

typedef struct Rr_SPIRVOpMap Rr_SPIRVOpMap;
struct Rr_SPIRVOpMap
{
    uint32_t Key;
    Rr_SPIRVOpMap *Children[4];

    Rr_SPIRVOp SPIRVOp;
};

enum
{
    RR_SPIRV_STORAGE_CLASS_UNIFORM_CONSTANT = 0U,
    RR_SPIRV_STORAGE_CLASS_INPUT = 1U,
    RR_SPIRV_STORAGE_CLASS_UNIFORM = 2U,
    RR_SPIRV_STORAGE_CLASS_STORAGE_BUFFER = 12U,
};

enum
{
    RR_SPIRV_DECORATION_BLOCK = 2U,
    RR_SPIRV_DECORATION_BUFFER_BLOCK = 3U,
    RR_SPIRV_DECORATION_NON_WRITABLE = 24U,
    RR_SPIRV_DECORATION_NON_READABLE = 25U,
    RR_SPIRV_DECORATION_BINDING = 33U,
    RR_SPIRV_DECORATION_DESCRIPTOR_SET = 34U,
};

enum
{
    RR_SPIRV_OP_TYPE_INT = 21U,
    RR_SPIRV_OP_TYPE_FLOAT = 22U,
    RR_SPIRV_OP_TYPE_VECTOR = 23U,
    RR_SPIRV_OP_TYPE_MATRIX = 24U,
    RR_SPIRV_OP_TYPE_IMAGE = 25U,
    RR_SPIRV_OP_TYPE_SAMPLER = 26U,
    RR_SPIRV_OP_TYPE_SAMPLED_IMAGE = 27U,
    RR_SPIRV_OP_TYPE_ARRAY = 28U,
    RR_SPIRV_OP_TYPE_RUNTIME_ARRAY = 29U,
    RR_SPIRV_OP_TYPE_STRUCT = 30U,
    RR_SPIRV_OP_TYPE_OPAQUE = 31U,
    RR_SPIRV_OP_TYPE_POINTER = 32U,
    RR_SPIRV_OP_CONSTANT = 43U,
    RR_SPIRV_OP_FUNCTION = 54U,
    RR_SPIRV_OP_VARIABLE = 59U,
    RR_SPIRV_OP_DECORATE = 71U,
};

static inline Rr_ImageFormat Rr_GetSPIRVImageFormat(uint8_t SPIRVFormat)
{
    switch (SPIRVFormat)
    {
        case 1:
            return RR_IMAGE_FORMAT_R32G32B32A32_SFLOAT;
        // case 2:
        //     return RR_IMAGE_FORMAT_R16G16B16A16_SFLOAT;
        case 3:
            return RR_IMAGE_FORMAT_R32_SFLOAT;
        case 4:
            return RR_IMAGE_FORMAT_R8G8B8A8_UNORM;
        // case 5:
        //     return RR_IMAGE_FORMAT_R8G8B8A8_SNORM;
        case 6:
            return RR_IMAGE_FORMAT_R32G32_SFLOAT;
        case 7:
            return RR_IMAGE_FORMAT_R16G16_SFLOAT;
        // case 8:
        //     return RR_IMAGE_FORMAT_R11G11B10_SFLOAT;
        // case 9:
        //     return RR_IMAGE_FORMAT_R16_SFLOAT;
        // case 10:
        //     return RR_IMAGE_FORMAT_R16G16B16A16_UNORM;
        // case 11:
        //     return RR_IMAGE_FORMAT_R10G10B10A2_UNORM;
        // case 12:
        //     return RR_IMAGE_FORMAT_R16G16_UNORM;
        case 13:
            return RR_IMAGE_FORMAT_R8G8_UNORM;
        // case 14:
        //     return RR_IMAGE_FORMAT_R16_UNORM;
        // case 15:
        //     return RR_IMAGE_FORMAT_R8_UNORM;
        // case 16:
        //     return RR_IMAGE_FORMAT_R16G16B16A16_SNORM;
        // case 17:
        //     return RR_IMAGE_FORMAT_R16G16_SNORM;
        // case 18:
        //     return RR_IMAGE_FORMAT_R8G8_SNORM;
        // case 19:
        //     return RR_IMAGE_FORMAT_R16_SNORM;
        // case 20:
        //     return RR_IMAGE_FORMAT_R8_SNORM;
        // case 21:
        //     return RR_IMAGE_FORMAT_R32G32B32A32_SINT;
        // case 22:
        //     return RR_IMAGE_FORMAT_R16G16B16A16_SINT;
        case 23:
            return RR_IMAGE_FORMAT_R8G8B8A8_SINT;
        case 24:
            return RR_IMAGE_FORMAT_R32_SINT;
        // case 25:
        //     return RR_IMAGE_FORMAT_R32G32_SINT;
        // case 26:
        //     return RR_IMAGE_FORMAT_R16G16_SINT;
        case 27:
            return RR_IMAGE_FORMAT_R8G8_SINT;
        // case 28:
        //     return RR_IMAGE_FORMAT_R16_SINT;
        // case 29:
        //     return RR_IMAGE_FORMAT_R8_SINT;
        // case 30:
        //     return RR_IMAGE_FORMAT_R32G32B32A32_UINT;
        // case 31:
        //     return RR_IMAGE_FORMAT_R16G16B16A16_UINT;
        case 32:
            return RR_IMAGE_FORMAT_R8G8B8A8_UINT;
        case 33:
            return RR_IMAGE_FORMAT_R32_UINT;
        // case 34:
        //     return RR_IMAGE_FORMAT_R10G10B10A2_UINT;
        // case 35:
        //     return RR_IMAGE_FORMAT_R32G32_UINT;
        // case 36:
        //     return RR_IMAGE_FORMAT_R16G16_UINT;
        case 37:
            return RR_IMAGE_FORMAT_R8G8_UINT;
        // case 38:
        //     return RR_IMAGE_FORMAT_R16_UINT;
        // case 39:
        //     return RR_IMAGE_FORMAT_R8_UINT;
        default:
            return RR_IMAGE_FORMAT_UNDEFINED;
    }
}

static inline bool Rr_FindOrAddBinding(
    Rr_BindingArray *BindingArray,
    uint32_t BindingIndex,
    Rr_Binding **OutBinding)
{
    for (uint32_t Index = 0; Index < BindingArray->Count; ++Index)
    {
        Rr_Binding *Binding = &BindingArray->Data[Index];
        if (Binding->Index == BindingIndex)
        {
            *OutBinding = Binding;

            return true;
        }
    }

    *OutBinding = RR_PUSH_INTO_ARRAY(BindingArray, NULL);

    return false;
}

static inline bool Rr_AddSPIRVBinding(
    uint32_t BindingIndex,
    uint32_t Count,
    Rr_SPIRVOp const *TypeOp,
    Rr_SPIRVVariable const *Variable,
    Rr_ShaderStage ShaderStage,
    Rr_BindingArray *BindingArray,
    Rr_Arena *Arena)
{
    RR_RESERVE_ARRAY(BindingArray, RR_MAX_BINDINGS, Arena);
    Rr_Binding *Binding = NULL;
    if (Rr_FindOrAddBinding(BindingArray, BindingIndex, &Binding))
    {
        /* Binding already exists, just add current stage to it. */
        Binding->Stages |= ShaderStage;

        return true;
    }
    if (Variable->NonWritable)
    {
        Binding->Flags |= RR_BINDING_FLAGS_NON_WRITABLE_BIT;
    }
    if (Variable->NonReadable)
    {
        Binding->Flags |= RR_BINDING_FLAGS_NON_READABLE_BIT;
    }
    Binding->Count = Count;
    Binding->Stages = ShaderStage;
    Binding->Index = BindingIndex;
    Binding->ImageFormat = RR_IMAGE_FORMAT_UNDEFINED;
    switch (TypeOp->OpCode)
    {
        case RR_SPIRV_OP_TYPE_STRUCT:
        {
            /* Whether it's storage or uniform comes from decoration of its
             * OpTypeStruct or, more recently, from storage class. */
            if (TypeOp->Union.Struct.Block)
            {
                Binding->Type = RR_BINDING_TYPE_UNIFORM_BUFFER;
            }
            else if (
                TypeOp->Union.Struct.BufferBlock ||
                Variable->StorageClassStorageBuffer)
            {
                Binding->Type = RR_BINDING_TYPE_STORAGE_BUFFER;
            }
            else
            {
                RR_LOG_ERROR(
                    "Couldn't determine buffer type for binding %d!",
                    BindingIndex);

                return false;
            }
        }
        break;
        case RR_SPIRV_OP_TYPE_IMAGE:
        {
            switch (TypeOp->Union.Image.Sampled)
            {
                case 1:
                {
                    Binding->Type = RR_BINDING_TYPE_SAMPLED_IMAGE;
                }
                break;
                case 2:
                {
                    Binding->Type = RR_BINDING_TYPE_STORAGE_IMAGE;
                    Binding->ImageFormat =
                        Rr_GetSPIRVImageFormat(TypeOp->Union.Image.Format);
                }
                break;
                default:
                {
                    RR_LOG_ERROR(
                        "Incorrect SPIRV decoration for binding %d!",
                        BindingIndex);
                }
                break;
            }
        }
        break;
        case RR_SPIRV_OP_TYPE_SAMPLER:
        {
            Binding->Type = RR_BINDING_TYPE_SAMPLER;
        }
        break;
        case RR_SPIRV_OP_TYPE_SAMPLED_IMAGE:
        {
            /* This is apparently a combined image sampler... */
            Binding->Type = RR_BINDING_TYPE_COMBINED_IMAGE_SAMPLER;
        }
        break;
        default:
        {
            RR_LOG_ERROR(
                "Incorrect type OpCode %d for binding %d!",
                TypeOp->OpCode,
                BindingIndex);

            return false;
        }
        break;
    }

    return true;
}

static inline Rr_SPIRVOp *Rr_UpsertSPIRVOp(
    Rr_SPIRVOpMap **Map,
    uint32_t Key,
    Rr_Arena *Arena)
{
    for (uint64_t Hash = Rr_Hash64(sizeof(Key), &Key); *Map; Hash <<= 2)
    {
        if (Key == (*Map)->Key)
        {
            return &(*Map)->SPIRVOp;
        }
        Map = &(*Map)->Children[Hash >> 62];
    }

    *Map = Rr_Alloc(sizeof(Rr_SPIRVOpMap), Arena);
    (*Map)->Key = Key;

    return &(*Map)->SPIRVOp;
}

bool Rr_GetBindingsFromSPIRV(
    Rr_ShaderInfo const *ShaderInfo,
    Rr_ShaderStage ShaderStage,
    Rr_BindingArray BindingArrays[RR_MAX_SETS],
    Rr_Arena *Arena)
{
    Rr_Scratch Scratch = Rr_GetScratch(Arena);

    uint32_t const *Data = ShaderInfo->SPVData;

    Rr_SPIRVOpMap *SPIRVOpMap = NULL;

    RR_ARRAY(uint32_t) BindingIndices = { 0 };
    RR_RESERVE_ARRAY(&BindingIndices, RR_MAX_BINDINGS, Scratch.Arena);

    Rr_SPIRVHeader const *Header = (Rr_SPIRVHeader const *)Data;
    if (Header->Magic != 0x07230203)
    {
        RR_LOG_ERROR("Magic mismatch!");

        return false;
    }

    size_t Offset = sizeof(Rr_SPIRVHeader) / sizeof(uint32_t);
    size_t End = ShaderInfo->SPVSize / sizeof(uint32_t);
    while (Offset < End)
    {
        uint32_t Instruction = Data[Offset];
        uint16_t Length = (uint16_t)(Instruction >> 16);
        uint16_t OpCode = Instruction & 0xFFFFU;

        if (OpCode == RR_SPIRV_OP_DECORATE)
        {
            uint32_t ID = Data[Offset + 1];
            uint32_t Decoration = Data[Offset + 2];

            Rr_SPIRVOp *SPIRVOp =
                Rr_UpsertSPIRVOp(&SPIRVOpMap, ID, Scratch.Arena);

            if (Decoration == RR_SPIRV_DECORATION_BLOCK)
            {
                SPIRVOp->Union.Struct.Block = true;
            }

            if (Decoration == RR_SPIRV_DECORATION_BUFFER_BLOCK)
            {
                SPIRVOp->Union.Struct.BufferBlock = true;
            }

            if (Decoration == RR_SPIRV_DECORATION_BINDING)
            {
                uint32_t Binding = Data[Offset + 3];
                SPIRVOp->Union.Variable.Binding = (uint8_t)Binding;

                *RR_PUSH_INTO_ARRAY(&BindingIndices, Scratch.Arena) = ID;
            }

            if (Decoration == RR_SPIRV_DECORATION_DESCRIPTOR_SET)
            {
                uint32_t Set = Data[Offset + 3];
                SPIRVOp->Union.Variable.DescriptorSet = (uint8_t)Set;
            }

            if (Decoration == RR_SPIRV_DECORATION_NON_WRITABLE)
            {
                SPIRVOp->Union.Variable.NonWritable = true;
            }

            if (Decoration == RR_SPIRV_DECORATION_NON_READABLE)
            {
                SPIRVOp->Union.Variable.NonReadable = true;
            }

            SPIRVOp->OpCode = OpCode;
        }

        if (OpCode == RR_SPIRV_OP_VARIABLE)
        {
            uint32_t ResultTypeID = Data[Offset + 1];
            uint32_t ResultID = Data[Offset + 2];
            uint32_t StorageClass = Data[Offset + 3];

            Rr_SPIRVOp *SPIRVOp =
                Rr_UpsertSPIRVOp(&SPIRVOpMap, ResultID, Scratch.Arena);

            SPIRVOp->Union.Variable.StorageClassStorageBuffer =
                StorageClass == RR_SPIRV_STORAGE_CLASS_STORAGE_BUFFER;
            SPIRVOp->Union.Variable.PointerID = ResultTypeID;

            SPIRVOp->OpCode = OpCode;
        }

        if (OpCode == RR_SPIRV_OP_TYPE_POINTER)
        {
            uint32_t ResultID = Data[Offset + 1];
            uint32_t StorageClass = Data[Offset + 2];
            uint32_t TypeID = Data[Offset + 3];

            Rr_SPIRVOp *SPIRVOp =
                Rr_UpsertSPIRVOp(&SPIRVOpMap, ResultID, Scratch.Arena);

            SPIRVOp->Union.Pointer.StorageClass = (uint16_t)StorageClass;
            SPIRVOp->Union.Pointer.TypeID = TypeID;

            SPIRVOp->OpCode = OpCode;
        }

        if (OpCode >= RR_SPIRV_OP_TYPE_INT && OpCode <= RR_SPIRV_OP_TYPE_OPAQUE)
        {
            uint32_t ResultID = Data[Offset + 1];

            Rr_SPIRVOp *SPIRVOp =
                Rr_UpsertSPIRVOp(&SPIRVOpMap, ResultID, Scratch.Arena);

            if (OpCode == RR_SPIRV_OP_TYPE_IMAGE)
            {
                SPIRVOp->Union.Image.Dimension = (uint8_t)Data[Offset + 3];
                SPIRVOp->Union.Image.Arrayed = (uint8_t)Data[Offset + 5];
                SPIRVOp->Union.Image.Sampled = (uint8_t)Data[Offset + 7];
                SPIRVOp->Union.Image.Format = (uint8_t)Data[Offset + 8];
            }

            if (OpCode == RR_SPIRV_OP_TYPE_ARRAY)
            {
                SPIRVOp->Union.Array.ElementTypeID = Data[Offset + 2];
                SPIRVOp->Union.Array.LengthID = Data[Offset + 3];
            }

            SPIRVOp->OpCode = OpCode;
        }

        if (OpCode == RR_SPIRV_OP_CONSTANT)
        {
            uint32_t ResultID = Data[Offset + 2];

            Rr_SPIRVOp *SPIRVOp =
                Rr_UpsertSPIRVOp(&SPIRVOpMap, ResultID, Scratch.Arena);

            SPIRVOp->Union.Constant.TypeID = Data[Offset + 1];
            SPIRVOp->Union.Constant.Value = Data[Offset + 3];

            SPIRVOp->OpCode = OpCode;
        }

        Offset += Length;
    }

    for (uint32_t Index = 0; Index < BindingIndices.Count; ++Index)
    {
        uint32_t VariableID = BindingIndices.Data[Index];
        Rr_SPIRVVariable *Variable =
            &Rr_UpsertSPIRVOp(&SPIRVOpMap, VariableID, NULL)->Union.Variable;
        uint8_t BindingIndex = Variable->Binding;
        uint8_t SetIndex = Variable->DescriptorSet;
        if (SetIndex >= RR_MAX_SETS)
        {
            RR_LOG_ERROR(
                "Binding wants set index %d but maximum set index is %d!",
                SetIndex,
                RR_MAX_SETS - 1);

            Rr_DestroyScratch(Scratch);

            return false;
        }
        Rr_SPIRVPointer *Pointer =
            &Rr_UpsertSPIRVOp(&SPIRVOpMap, Variable->PointerID, NULL)
                 ->Union.Pointer;
        /* uint32_t PointerStorageClass = Pointer->StorageClass; */
        Rr_SPIRVOp *PointerTypeOp =
            Rr_UpsertSPIRVOp(&SPIRVOpMap, Pointer->TypeID, NULL);

        if (PointerTypeOp->OpCode == RR_SPIRV_OP_TYPE_ARRAY)
        {
            Rr_SPIRVArray *Array = &PointerTypeOp->Union.Array;
            Rr_SPIRVOp *ArrayElementTypeOp =
                Rr_UpsertSPIRVOp(&SPIRVOpMap, Array->ElementTypeID, NULL);
            Rr_SPIRVConstant *ArrayLengthConstant =
                &Rr_UpsertSPIRVOp(&SPIRVOpMap, Array->LengthID, NULL)
                     ->Union.Constant;

            if (!Rr_AddSPIRVBinding(
                    BindingIndex,
                    ArrayLengthConstant->Value,
                    ArrayElementTypeOp,
                    Variable,
                    ShaderStage,
                    &BindingArrays[SetIndex],
                    Arena))
            {
                Rr_DestroyScratch(Scratch);

                return false;
            }
        }
        else
        {
            if (!Rr_AddSPIRVBinding(
                    BindingIndex,
                    1,
                    PointerTypeOp,
                    Variable,
                    ShaderStage,
                    &BindingArrays[SetIndex],
                    Arena))
            {
                Rr_DestroyScratch(Scratch);

                return false;
            }
        }
    }

    Rr_DestroyScratch(Scratch);

    return true;
}
