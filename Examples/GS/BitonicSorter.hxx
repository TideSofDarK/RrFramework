#pragma once

#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include <array>
#include <cassert>
#include <cmath>
#include <cstring>

struct SSortList
{
    struct SUniformData
    {
        Rr_Mat4 ViewProjection;
        uint32_t AliveCount;
    };

    uint32_t ThreadsPerWorkgroup{};

    Rr_ComputePipeline *Pipeline;
    Rr_Buffer *UniformBuffer;
    Rr_Buffer *IndirectBuffer;

    SSortList()
    {
        ThreadsPerWorkgroup = Rr_GetMaxComputeWorkgroupInvocations();
        if (RR_IS_POW2(ThreadsPerWorkgroup) != true)
        {
            ThreadsPerWorkgroup = Rr_NextPowerOfTwo(ThreadsPerWorkgroup) / 2;
        }

        std::array Specializations = {
            Rr_PipelineSpecialization{
                .ConstantID = 0,
                .Size = sizeof(ThreadsPerWorkgroup),
                .Data = &ThreadsPerWorkgroup,
            },
        };

        Rr_Asset ComputeShader = Rr_LoadAsset(EXAMPLE_ASSET_SORTLIST_COMP_SPV);
        Rr_ShaderInfo ShaderInfo = {
            .SPVSize = ComputeShader.Size,
            .SPVData = ComputeShader.Data,
            .SpecializationCount = Specializations.size(),
            .Specializations = Specializations.data(),
        };

        Pipeline = Rr_CreateComputePipeline(&ShaderInfo);

        UniformBuffer = Rr_CreateBuffer(
            sizeof(SUniformData),
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT |
                RR_BUFFER_FLAGS_PER_FRAME_BIT);

        IndirectBuffer = Rr_CreateBuffer(
            sizeof(Rr_DrawIndirectCommand),
            RR_BUFFER_FLAGS_INDIRECT_BIT | RR_BUFFER_FLAGS_STORAGE_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
                RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_STAGING_BIT);
    }

    ~SSortList()
    {
        Rr_ReleaseComputePipeline(Pipeline);
        Rr_ReleaseBuffer(UniformBuffer);
        Rr_ReleaseBuffer(IndirectBuffer);
    }

    void Generate(
        size_t AliveCount,
        size_t AlignedCount,
        const Rr_Mat4 &ViewProjection,
        size_t SplatsSize,
        Rr_Buffer *SplatsBuffer,
        size_t EntriesSize,
        Rr_Buffer *EntriesBuffer)
    {
        uint32_t DispatchSize = AlignedCount / ThreadsPerWorkgroup;
        if (DispatchSize == 0)
        {
            return;
        }

        Rr_DrawIndirectCommand Command = { 0 };
        Command.VertexCount = 6;

        std::memcpy(Rr_GetMappedBufferData(IndirectBuffer), &Command, sizeof(Rr_DrawIndirectCommand));

        SUniformData UniformData;
        UniformData.ViewProjection = ViewProjection;
        UniformData.AliveCount = AliveCount;

        std::memcpy(Rr_GetMappedBufferData(UniformBuffer), &UniformData, sizeof(SUniformData));

        Rr_GraphNode *ComputeNode = Rr_AddComputeNode(Rr_GetGraph());
        Rr_BindComputePipeline(ComputeNode, Pipeline);
        Rr_BindUniformBuffer(ComputeNode, UniformBuffer, 0, 0, 0, sizeof(SUniformData));
        Rr_BindStorageBuffer(ComputeNode, SplatsBuffer, 0, 1, 0, SplatsSize);
        Rr_BindStorageBuffer(ComputeNode, EntriesBuffer, 0, 2, 0, EntriesSize);
        Rr_BindStorageBuffer(ComputeNode, IndirectBuffer, 0, 3, 0, sizeof(Rr_DrawIndirectCommand));
        Rr_Dispatch(ComputeNode, DispatchSize, 1, 1);
    }
};

struct SBitonicSorter
{
    const uint32_t LOCAL_SORT = 0;
    const uint32_t LOCAL_DISPERSE = 1;
    const uint32_t BIG_FLIP = 2;
    const uint32_t BIG_DISPERSE = 3;

    struct SGPUSortInfo
    {
        uint32_t Count;
        uint32_t Height;
        uint32_t Algorithm;
    };

    SSortList SortList;
    uint32_t ThreadsPerWorkgroup{};
    Rr_ComputePipeline *Pipeline;
    Rr_Buffer *UniformBuffer;
    uint32_t AliveCount;
    uint32_t AlignedCount;

    uint32_t DispatchCount()
    {
        uint32_t Result = 1;

        uint32_t Height = ThreadsPerWorkgroup * 2;

        Result += std::log2(AlignedCount / Height);

        Height *= 2;

        for (; Height <= AlignedCount; Height *= 2)
        {
            for (uint32_t DisperseHeight = Height / 2; DisperseHeight > 1; DisperseHeight /= 2)
            {
                if (DisperseHeight <= ThreadsPerWorkgroup * 2)
                {
                    Result++;
                    break;
                }
                else
                {
                    Result++;
                }
            }
        }

        return Result;
    }

    explicit SBitonicSorter(uint32_t AliveCount, uint32_t AlignedCount)
        : AliveCount(AliveCount)
        , AlignedCount(AlignedCount)
    {
        ThreadsPerWorkgroup = Rr_GetMaxComputeWorkgroupInvocations();
        if (RR_IS_POW2(ThreadsPerWorkgroup) != true)
        {
            ThreadsPerWorkgroup = Rr_NextPowerOfTwo(ThreadsPerWorkgroup) / 2;
        }

        /* Create compute pipeline. */

        std::array Specializations = {
            Rr_PipelineSpecialization{
                .ConstantID = 0,
                .Size = sizeof(ThreadsPerWorkgroup),
                .Data = &ThreadsPerWorkgroup,
            },
        };

        Rr_Asset ComputeShader = Rr_LoadAsset(EXAMPLE_ASSET_BITONICSORT_COMP_SPV);
        Rr_ShaderInfo ShaderInfo = {
            .SPVSize = ComputeShader.Size,
            .SPVData = ComputeShader.Data,
            .SpecializationCount = Specializations.size(),
            .Specializations = Specializations.data(),
        };

        Pipeline = Rr_CreateComputePipeline(&ShaderInfo);

        UniformBuffer = Rr_CreateBuffer(
            RR_ALIGN_POW2(sizeof(SGPUSortInfo), Rr_GetUniformAlignment()) * DispatchCount(),
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT |
                RR_BUFFER_FLAGS_PER_FRAME_BIT);
    }

    ~SBitonicSorter()
    {
        Rr_ReleaseComputePipeline(Pipeline);
        Rr_ReleaseBuffer(UniformBuffer);
    }

    void Sort(
        const Rr_Mat4 &ViewProjection,
        size_t SplatsSize,
        Rr_Buffer *SplatsBuffer,
        size_t EntriesSize,
        Rr_Buffer *EntriesBuffer)
    {
        SortList
            .Generate(AliveCount, AlignedCount, ViewProjection, SplatsSize, SplatsBuffer, EntriesSize, EntriesBuffer);

        Rr_GraphNode *ComputeNode = Rr_AddComputeNode(Rr_GetGraph());
        Rr_BindComputePipeline(ComputeNode, Pipeline);
        Rr_BindStorageBuffer(ComputeNode, SplatsBuffer, 0, 0, 0, SplatsSize);
        Rr_BindStorageBuffer(ComputeNode, EntriesBuffer, 0, 1, 0, EntriesSize);

        uint32_t DispatchSize = AlignedCount / 2 / ThreadsPerWorkgroup;

        size_t UniformBufferOffset = 0;
        auto Dispatch = [&](uint32_t Height, uint32_t Algorithm) {
            SGPUSortInfo SortInfo;
            SortInfo.Count = AlignedCount;
            SortInfo.Height = Height;
            SortInfo.Algorithm = Algorithm;

            char *Dst = (char *)Rr_GetMappedBufferData(UniformBuffer) + UniformBufferOffset;
            std::memcpy(Dst, &SortInfo, sizeof(SGPUSortInfo));

            Rr_BindUniformBuffer(ComputeNode, UniformBuffer, 1, 0, UniformBufferOffset, sizeof(SGPUSortInfo));
            Rr_Dispatch(ComputeNode, DispatchSize, 1, 1);
            Rr_ComputeBarrier(ComputeNode);

            UniformBufferOffset += RR_ALIGN_POW2(sizeof(SGPUSortInfo), Rr_GetUniformAlignment());
        };

        uint32_t Height = ThreadsPerWorkgroup * 2;

        Dispatch(Height, LOCAL_SORT);

        Height *= 2;

        for (; Height <= AlignedCount; Height *= 2)
        {
            Dispatch(Height, BIG_FLIP);

            for (uint32_t DisperseHeight = Height / 2; DisperseHeight > 1; DisperseHeight /= 2)
            {
                if (DisperseHeight <= ThreadsPerWorkgroup * 2)
                {
                    Dispatch(DisperseHeight, LOCAL_DISPERSE);
                    break;
                }
                else
                {

                    Dispatch(DisperseHeight, BIG_DISPERSE);
                }
            }
        }
    }
};
