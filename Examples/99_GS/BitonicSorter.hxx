#pragma once

#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include <array>
#include <cassert>
#include <cstring>

struct SSortList
{
    struct SUniformData
    {
        Rr_Mat4 View;
        uint32_t AliveCount;
    };

    Rr_Renderer *Renderer;
    uint32_t ThreadsPerWorkgroup;

    Rr_PipelineLayout *Layout;
    Rr_ComputePipeline *Pipeline;
    Rr_Buffer *UniformBuffer;

    SSortList(Rr_Renderer *Renderer)
        : Renderer(Renderer)
        , ThreadsPerWorkgroup(Rr_GetMaxComputeWorkgroupInvocations(Renderer))
    {
        std::array Bindings = {
            Rr_PipelineBinding{ 0, 1, RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER },
            Rr_PipelineBinding{ 1, 1, RR_PIPELINE_BINDING_TYPE_STORAGE_BUFFER },
            Rr_PipelineBinding{ 2, 1, RR_PIPELINE_BINDING_TYPE_STORAGE_BUFFER },
        };
        std::array BindingSets = {
            Rr_PipelineBindingSet{
                Bindings.size(),
                Bindings.data(),
                RR_SHADER_STAGE_COMPUTE_BIT,
            },
        };
        Layout = Rr_CreatePipelineLayout(
            Renderer,
            BindingSets.size(),
            BindingSets.data());

        std::array Specializations = {
            Rr_PipelineSpecialization{
                0,
                RR_MAKE_DATA_STRUCT(ThreadsPerWorkgroup) },
        };

        Rr_ComputePipelineCreateInfo PipelineCreateInfo = {};
        PipelineCreateInfo.Layout = Layout;
        PipelineCreateInfo.ShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_SORTLIST_COMP_SPV);
        PipelineCreateInfo.SpecializationCount = Specializations.size();
        PipelineCreateInfo.Specializations = Specializations.data();

        Pipeline = Rr_CreateComputePipeline(Renderer, &PipelineCreateInfo);

        UniformBuffer = Rr_CreateBuffer(
            Renderer,
            sizeof(SUniformData),
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_STAGING_BIT |
                RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT);
    }

    ~SSortList()
    {
        Rr_DestroyComputePipeline(Renderer, Pipeline);
        Rr_DestroyPipelineLayout(Renderer, Layout);
        Rr_DestroyBuffer(Renderer, UniformBuffer);
    }

    void Generate(
        size_t AliveCount,
        size_t AlignedCount,
        const Rr_Mat4 &View,
        size_t SplatsSize,
        Rr_Buffer *SplatsBuffer,
        size_t EntriesSize,
        Rr_Buffer *EntriesBuffer)
    {
        uint32_t DispatchSize = AlignedCount / ThreadsPerWorkgroup;
        if(DispatchSize == 0)
        {
            return;
        }

        SUniformData UniformData;
        UniformData.View = View;
        UniformData.AliveCount = AliveCount;

        std::memcpy(
            Rr_GetMappedBufferData(Renderer, UniformBuffer),
            &UniformData,
            sizeof(SUniformData));

        Rr_GraphNode *ComputeNode =
            Rr_AddComputeNode(Renderer, "generate_sort_list");
        Rr_BindComputePipeline(ComputeNode, Pipeline);
        Rr_BindUniformBuffer(
            ComputeNode,
            UniformBuffer,
            0,
            0,
            0,
            sizeof(SUniformData));
        Rr_BindStorageBuffer(ComputeNode, SplatsBuffer, 0, 1, 0, SplatsSize);
        Rr_BindStorageBuffer(ComputeNode, EntriesBuffer, 0, 2, 0, EntriesSize);
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

    Rr_Renderer *Renderer;
    SSortList SortList;
    uint32_t ThreadsPerWorkgroup;
    Rr_PipelineLayout *Layout;
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

        for(; Height <= AlignedCount; Height *= 2)
        {
            for(uint32_t DisperseHeight = Height / 2; DisperseHeight > 1;
                DisperseHeight /= 2)
            {
                if(DisperseHeight <= ThreadsPerWorkgroup * 2)
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

    explicit SBitonicSorter(
        Rr_Renderer *Renderer,
        uint32_t AliveCount,
        uint32_t AlignedCount)
        : Renderer(Renderer)
        , SortList(Renderer)
        , ThreadsPerWorkgroup(Rr_GetMaxComputeWorkgroupInvocations(Renderer))
        , AliveCount(AliveCount)
        , AlignedCount(AlignedCount)
    {
        /* Create compute pipeline. */

        std::array BindingsA = {
            Rr_PipelineBinding{ 0, 1, RR_PIPELINE_BINDING_TYPE_STORAGE_BUFFER },
            Rr_PipelineBinding{ 1, 1, RR_PIPELINE_BINDING_TYPE_STORAGE_BUFFER },
        };
        std::array BindingsB = {
            Rr_PipelineBinding{ 0, 1, RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER },
        };
        std::array BindingSets = {
            Rr_PipelineBindingSet{
                BindingsA.size(),
                BindingsA.data(),
                RR_SHADER_STAGE_COMPUTE_BIT,
            },
            Rr_PipelineBindingSet{
                BindingsB.size(),
                BindingsB.data(),
                RR_SHADER_STAGE_COMPUTE_BIT,
            },
        };
        Layout = Rr_CreatePipelineLayout(
            Renderer,
            BindingSets.size(),
            BindingSets.data());

        std::array Specializations = {
            Rr_PipelineSpecialization{
                0,
                RR_MAKE_DATA_STRUCT(ThreadsPerWorkgroup) },
        };

        Rr_ComputePipelineCreateInfo PipelineCreateInfo = {};
        PipelineCreateInfo.Layout = Layout;
        PipelineCreateInfo.ShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_BITONICSORT_COMP_SPV);
        PipelineCreateInfo.SpecializationCount = Specializations.size();
        PipelineCreateInfo.Specializations = Specializations.data();

        Pipeline = Rr_CreateComputePipeline(Renderer, &PipelineCreateInfo);

        UniformBuffer = Rr_CreateBuffer(
            Renderer,
            RR_ALIGN_POW2(sizeof(Rr_Mat4), Rr_GetUniformAlignment(Renderer)) +
                RR_ALIGN_POW2(
                    sizeof(SGPUSortInfo),
                    Rr_GetUniformAlignment(Renderer)) *
                    DispatchCount(),
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_STAGING_BIT |
                RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT);
    }

    ~SBitonicSorter()
    {
        Rr_DestroyComputePipeline(Renderer, Pipeline);
        Rr_DestroyPipelineLayout(Renderer, Layout);
        Rr_DestroyBuffer(Renderer, UniformBuffer);
    }

    void Sort(
        const Rr_Mat4 &View,
        size_t SplatsSize,
        Rr_Buffer *SplatsBuffer,
        size_t EntriesSize,
        Rr_Buffer *EntriesBuffer)
    {
        SortList.Generate(
            AliveCount,
            AlignedCount,
            View,
            SplatsSize,
            SplatsBuffer,
            EntriesSize,
            EntriesBuffer);

        uint32_t DispatchSize = AlignedCount / 2 / ThreadsPerWorkgroup;

        size_t UniformBufferOffset =
            RR_ALIGN_POW2(sizeof(Rr_Mat4), Rr_GetUniformAlignment(Renderer));
        auto Dispatch = [&](uint32_t Height, uint32_t Algorithm) {
            SGPUSortInfo SortInfo;
            SortInfo.Count = AlignedCount;
            SortInfo.Height = Height;
            SortInfo.Algorithm = Algorithm;

            char *Dst =
                (char *)Rr_GetMappedBufferData(Renderer, UniformBuffer) +
                UniformBufferOffset;
            std::memcpy(Dst, &SortInfo, sizeof(SGPUSortInfo));

            Rr_GraphNode *ComputeNode = Rr_AddComputeNode(Renderer, "compute");
            Rr_BindComputePipeline(ComputeNode, Pipeline);
            Rr_BindStorageBuffer(
                ComputeNode,
                SplatsBuffer,
                0,
                0,
                0,
                SplatsSize);
            Rr_BindStorageBuffer(
                ComputeNode,
                EntriesBuffer,
                0,
                1,
                0,
                EntriesSize);
            Rr_BindUniformBuffer(
                ComputeNode,
                UniformBuffer,
                1,
                0,
                UniformBufferOffset,
                sizeof(SGPUSortInfo));
            Rr_Dispatch(ComputeNode, DispatchSize, 1, 1);

            UniformBufferOffset += RR_ALIGN_POW2(
                sizeof(SGPUSortInfo),
                Rr_GetUniformAlignment(Renderer));
        };

        uint32_t Height = ThreadsPerWorkgroup * 2;

        Dispatch(Height, LOCAL_SORT);

        Height *= 2;

        for(; Height <= AlignedCount; Height *= 2)
        {
            Dispatch(Height, BIG_FLIP);

            for(uint32_t DisperseHeight = Height / 2; DisperseHeight > 1;
                DisperseHeight /= 2)
            {
                if(DisperseHeight <= ThreadsPerWorkgroup * 2)
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
