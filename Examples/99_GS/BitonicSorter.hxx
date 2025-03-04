#pragma once

#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include <array>
#include <cassert>
#include <cstring>

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
    uint32_t ThreadsPerWorkgroup;
    Rr_PipelineLayout *Layout;
    Rr_ComputePipeline *Pipeline;
    Rr_Buffer *InfoBuffer;

    explicit SBitonicSorter(Rr_Renderer *Renderer)
        : Renderer(Renderer)
        , ThreadsPerWorkgroup(Rr_GetMaxComputeWorkgroupInvocations(Renderer))
    {
        std::array Bindings = {
            Rr_PipelineBinding{ 0, 1, RR_PIPELINE_BINDING_TYPE_STORAGE_BUFFER },
            Rr_PipelineBinding{ 1, 1, RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER },
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
            Rr_LoadAsset(EXAMPLE_ASSET_BITONICSORT_COMP_SPV);
        PipelineCreateInfo.SpecializationCount = Specializations.size();
        PipelineCreateInfo.Specializations = Specializations.data();

        Pipeline = Rr_CreateComputePipeline(Renderer, &PipelineCreateInfo);

        InfoBuffer = Rr_CreateBuffer(
            Renderer,
            sizeof(uint32_t) * 1024,
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_STAGING_BIT |
                RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT);
    }

    ~SBitonicSorter()
    {
        Rr_DestroyComputePipeline(Renderer, Pipeline);
        Rr_DestroyPipelineLayout(Renderer, Layout);
        Rr_DestroyBuffer(Renderer, InfoBuffer);
    }

    void Sort(uint32_t Count, Rr_Buffer *Buffer)
    {
        assert(RR_IS_POW2(Count));

        uint32_t DispatchSize = Count / 2 / ThreadsPerWorkgroup;

        size_t InfoBufferOffset = 0;
        auto Dispatch = [&](uint32_t Height, uint32_t Algorithm) {
            SGPUSortInfo SortInfo;
            SortInfo.Count = Count;
            SortInfo.Height = Height;
            SortInfo.Algorithm = Algorithm;

            char *Dst = (char *)Rr_GetMappedBufferData(Renderer, InfoBuffer) +
                        InfoBufferOffset;
            std::memcpy(Dst, &SortInfo, sizeof(SGPUSortInfo));

            Rr_GraphNode *ComputeNode = Rr_AddComputeNode(Renderer, "compute");
            Rr_BindComputePipeline(ComputeNode, Pipeline);
            Rr_BindStorageBuffer(
                ComputeNode,
                Buffer,
                0,
                0,
                0,
                sizeof(uint32_t) * Count);
            Rr_BindUniformBuffer(
                ComputeNode,
                InfoBuffer,
                0,
                1,
                InfoBufferOffset,
                sizeof(SGPUSortInfo));
            Rr_Dispatch(ComputeNode, DispatchSize, 1, 1);

            InfoBufferOffset += RR_ALIGN_POW2(
                sizeof(SGPUSortInfo),
                Rr_GetUniformAlignment(Renderer));
        };

        uint32_t Height = ThreadsPerWorkgroup * 2;

        Dispatch(Height, LOCAL_SORT);

        Height *= 2;

        for(; Height <= Count; Height *= 2)
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
