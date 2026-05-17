#include "Rr_SPIRV.h"

#define RR_LOG_MACRO_CATEGORY RR_LOG_CATEGORY_SPIRV
#include "Rr_App.h"
#include "Rr_LogMacro.h"

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
    uint16_t StorageClass;
    uint8_t Binding;       /* Comes from OpDecorate! */
    uint8_t DescriptorSet; /* Comes from OpDecorate! */
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
};

typedef struct Rr_SPIRVStruct Rr_SPIRVStruct;
struct Rr_SPIRVStruct
{
    uint32_t Decoration;
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

void Rr_CreatePipelineLayoutInfoFromSPIRV(
    size_t Size,
    uint32_t const *Data)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_SPIRVOp *SPIRVOps = RR_ALLOC(sizeof(Rr_SPIRVOp) * 1024, Scratch.Arena);
    RR_ARRAY(uint32_t) BindingIndices = { 0 };
    RR_RESERVE_ARRAY(&BindingIndices, RR_MAX_BINDINGS, Scratch.Arena);

    Rr_SPIRVHeader const *Header = (Rr_SPIRVHeader const *)Data;
    RR_LOG_INFO("Parsing SPIRV (size: %d)", Size);
    RR_LOG_INFO("Magic: %d", Header->Magic);
    RR_LOG_INFO("Version: %d", Header->Version);

#define STORAGE_CLASS_UNIFORM_CONSTANT 0U
#define STORAGE_CLASS_INPUT            1U
#define STORAGE_CLASS_UNIFORM          2U
#define STORAGE_CLASS_STORAGE_BUFFER   12U

#define DECORATION_BLOCK          2U
#define DECORATION_BUFFER_BLOCK   3U
#define DECORATION_BINDING        33U
#define DECORATION_DESCRIPTOR_SET 34U

#define OP_DECORATE           71U
#define OP_VARIABLE           59U
#define OP_TYPE_INT           21U
#define OP_TYPE_FLOAT         22U
#define OP_TYPE_VECTOR        23U
#define OP_TYPE_MATRIX        24U
#define OP_TYPE_IMAGE         25U
#define OP_TYPE_SAMPLER       26U
#define OP_TYPE_SAMPLED_IMAGE 27U
#define OP_TYPE_ARRAY         28U
#define OP_TYPE_RUNTIME_ARRAY 29U
#define OP_TYPE_STRUCT        30U
#define OP_TYPE_OPAQUE        31U
#define OP_TYPE_POINTER       32U
#define OP_CONSTANT           43U

    uint32_t Offset = sizeof(Rr_SPIRVHeader) / sizeof(uint32_t);
    while (Offset < Size)
    {
        uint32_t Instruction = Data[Offset];

        uint16_t Length = (uint16_t)(Instruction >> 16);
        if (Length == 0)
        {
            break;
        }
        uint16_t OpCode = Instruction & 0x0FFFFU;

        if (OpCode == OP_DECORATE)
        {
            uint32_t ID = Data[Offset + 1];
            uint32_t Decoration = Data[Offset + 2];

            if (Decoration == DECORATION_BLOCK)
            {
                SPIRVOps[ID].Union.Struct.Decoration = DECORATION_BLOCK;
            }

            if (Decoration == DECORATION_BUFFER_BLOCK)
            {
                SPIRVOps[ID].Union.Struct.Decoration = DECORATION_BUFFER_BLOCK;
            }

            if (Decoration == DECORATION_BINDING)
            {
                uint32_t Binding = Data[Offset + 3];
                SPIRVOps[ID].Union.Variable.Binding = (uint8_t)Binding;

                *RR_PUSH_INTO_ARRAY(&BindingIndices, Scratch.Arena) = ID;
            }

            if (Decoration == DECORATION_DESCRIPTOR_SET)
            {
                uint32_t Set = Data[Offset + 3];
                SPIRVOps[ID].Union.Variable.DescriptorSet = (uint8_t)Set;
            }

            SPIRVOps[ID].OpCode = OpCode;
        }

        if (OpCode == OP_VARIABLE)
        {
            uint32_t ResultTypeID = Data[Offset + 1];
            uint32_t ResultID = Data[Offset + 2];
            uint32_t StorageClass = Data[Offset + 3];

            SPIRVOps[ResultID].Union.Variable.StorageClass =
                (uint16_t)StorageClass;
            SPIRVOps[ResultID].Union.Variable.PointerID = ResultTypeID;

            SPIRVOps[ResultID].OpCode = OpCode;
        }

        if (OpCode == OP_TYPE_POINTER)
        {
            uint32_t ResultID = Data[Offset + 1];
            uint32_t StorageClass = Data[Offset + 2];
            uint32_t TypeID = Data[Offset + 3];

            SPIRVOps[ResultID].Union.Pointer.StorageClass =
                (uint16_t)StorageClass;
            SPIRVOps[ResultID].Union.Pointer.TypeID = TypeID;

            SPIRVOps[ResultID].OpCode = OpCode;
        }

        if (OpCode >= OP_TYPE_INT && OpCode <= OP_TYPE_OPAQUE)
        {
            uint32_t ResultID = Data[Offset + 1];

            if (OpCode == OP_TYPE_IMAGE)
            {
                SPIRVOps[ResultID].Union.Image.Dimension =
                    (uint8_t)Data[Offset + 3];
                SPIRVOps[ResultID].Union.Image.Arrayed =
                    (uint8_t)Data[Offset + 5];
                SPIRVOps[ResultID].Union.Image.Sampled =
                    (uint8_t)Data[Offset + 7];
            }

            if (OpCode == OP_TYPE_ARRAY)
            {
                SPIRVOps[ResultID].Union.Array.ElementTypeID = Data[Offset + 2];
                SPIRVOps[ResultID].Union.Array.LengthID = Data[Offset + 3];
            }

            SPIRVOps[ResultID].OpCode = OpCode;
        }

        if (OpCode == OP_CONSTANT)
        {
            uint32_t ResultID = Data[Offset + 2];

            SPIRVOps[ResultID].Union.Constant.TypeID = Data[Offset + 1];
            SPIRVOps[ResultID].Union.Constant.Value = Data[Offset + 3];

            SPIRVOps[ResultID].OpCode = OpCode;
        }

        Offset += Length;
    }

    RR_LOG_INFO("Found %d bindings...", BindingIndices.Count);
    for (uint32_t Index = 0; Index < BindingIndices.Count; ++Index)
    {
        uint32_t VariableID = BindingIndices.Data[Index];
        Rr_SPIRVVariable *Variable = &SPIRVOps[VariableID].Union.Variable;
        uint32_t VariableStorageClass = Variable->StorageClass;
        uint8_t Binding = Variable->Binding;
        uint8_t DescriptorSet = Variable->DescriptorSet;
        Rr_SPIRVPointer *Pointer = &SPIRVOps[Variable->PointerID].Union.Pointer;
        uint32_t PointerStorageClass = Pointer->StorageClass;
        Rr_SPIRVOp *PointerTypeOp = &SPIRVOps[Pointer->TypeID];

        if (PointerTypeOp->OpCode == OP_TYPE_ARRAY)
        {
            Rr_SPIRVArray *Array = &PointerTypeOp->Union.Array;
            Rr_SPIRVOp *ArrayElementTypeOp = &SPIRVOps[Array->ElementTypeID];
            Rr_SPIRVConstant *ArrayLengthConstant =
                &SPIRVOps[Array->LengthID].Union.Constant;
            RR_LOG_INFO(
                "%d) Binding %d, Set %d, Type %d, Count %d, VSC %d, PSC %d",
                Index,
                Binding,
                DescriptorSet,
                ArrayElementTypeOp->OpCode,
                ArrayLengthConstant->Value,
                VariableStorageClass,
                PointerStorageClass);
        }
        else
        {
            RR_LOG_INFO(
                "%d) Binding %d, Set %d, Type %d, VSC %d, PSC %d",
                Index,
                Binding,
                DescriptorSet,
                PointerTypeOp->OpCode,
                VariableStorageClass,
                PointerStorageClass);
        }
    }

    RR_LOG_INFO("Ending SPIRV...");

    Rr_DestroyScratch(Scratch);
}
