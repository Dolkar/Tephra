#pragma once

#include <tephra/pipeline.hpp>
#include <tephra/common.hpp>
#include <deque>
#include <array>

namespace tp {

// Helper class for storing and creating shader stage infos
class ShaderStageInfoBuilder {
public:
    // Clear and preallocate buffers for the number of stage setups and specialization constants needed
    void preallocate(std::size_t setupCount, std::size_t specConstantCount, std::size_t specConstantBytes);

    VkPipelineShaderStageCreateInfo& makeInfo(const ShaderStageSetup& stageSetup, ShaderStage stageType);

private:
    ScratchVector<VkPipelineShaderStageCreateInfo> stageCreateInfos;
    ScratchVector<VkSpecializationInfo> specializationInfos;
    ScratchVector<VkSpecializationMapEntry> specializationEntries;
    ScratchVector<std::byte> specializationData;
};

class ComputePipelineInfoBuilder {
public:
    ArrayView<VkComputePipelineCreateInfo> makeInfos(ArrayParameter<const ComputePipelineSetup* const> pipelineSetups);

    static const char* getDebugName(const ComputePipelineSetup* pipelineSetup) {
        return pipelineSetup->debugName.c_str();
    }

private:
    ShaderStageInfoBuilder shaderStageInfoBuilder;
    ScratchVector<VkComputePipelineCreateInfo> pipelineCreateInfos;
};

class GraphicsPipelineInfoBuilder {
public:
    ArrayView<VkGraphicsPipelineCreateInfo> makeInfos(ArrayParameter<const GraphicsPipelineSetup* const> pipelineSetups);

    static const char* getDebugName(const GraphicsPipelineSetup* pipelineSetup) {
        return pipelineSetup->debugName.c_str();
    }

private:
    // Structures that need to be kept in a contiguous array
    ShaderStageInfoBuilder shaderStageInfoBuilder;
    ScratchVector<VkVertexInputBindingDescription> vertexBindingDescriptions;
    ScratchVector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
    ScratchVector<VkDynamicState> dynamicStates;
    ScratchVector<VkPipelineDynamicStateCreateInfo> dynamicStateCreateInfos;
    ScratchVector<VkPipelineColorBlendAttachmentState> blendAttachmentStates;
    ScratchVector<VkGraphicsPipelineCreateInfo> pipelineCreateInfos;

    ScratchDeque<VkPipelineVertexInputStateCreateInfo> vertexInputCreateInfos;
    ScratchDeque<VkPipelineInputAssemblyStateCreateInfo> inputAssemblyCreateInfos;
    ScratchDeque<VkPipelineTessellationStateCreateInfo> tessellationCreateInfos;
    ScratchDeque<VkPipelineViewportStateCreateInfo> viewportCreateInfos;
    ScratchDeque<VkPipelineRasterizationStateCreateInfo> rasterizationCreateInfos;
    ScratchDeque<std::array<VkSampleMask, 2>> sampleMasks;
    ScratchDeque<VkPipelineMultisampleStateCreateInfo> multisampleCreateInfos;
    ScratchDeque<VkPipelineDepthStencilStateCreateInfo> depthStencilCreateInfos;
    ScratchDeque<VkPipelineColorBlendStateCreateInfo> colorBlendCreateInfos;
    ScratchDeque<VkPipelineRenderingCreateInfo> renderingCreateInfos;

    void preallocatePipelineSetups(ArrayParameter<const GraphicsPipelineSetup* const> pipelineSetups);
    void makePipelineSetup(const GraphicsPipelineSetup* pipelineSetup);

    void preallocateShaderStages(ArrayParameter<const GraphicsPipelineSetup* const> pipelineSetups);
    ArrayView<VkPipelineShaderStageCreateInfo> makeShaderStages(const GraphicsPipelineSetup* pipelineSetup);
    void preallocateVertexInputs(ArrayParameter<const GraphicsPipelineSetup* const> pipelineSetups);
    VkPipelineVertexInputStateCreateInfo* makeVertexInputs(const GraphicsPipelineSetup* pipelineSetup);
    void preallocateDynamicStates(ArrayParameter<const GraphicsPipelineSetup* const> pipelineSetups);
    VkPipelineDynamicStateCreateInfo* makeDynamicState(const GraphicsPipelineSetup* pipelineSetup);
    void preallocateBlendStates(ArrayParameter<const GraphicsPipelineSetup* const> pipelineSetups);
    VkPipelineColorBlendStateCreateInfo* makeColorBlendState(const GraphicsPipelineSetup* pipelineSetup);

    VkPipelineInputAssemblyStateCreateInfo* makeInputAssemblyState(const GraphicsPipelineSetup* pipelineSetup);
    VkPipelineTessellationStateCreateInfo* makeTessellationState(const GraphicsPipelineSetup* pipelineSetup);
    VkPipelineViewportStateCreateInfo* makeViewportState(const GraphicsPipelineSetup* pipelineSetup);
    VkPipelineRasterizationStateCreateInfo* makeRasterizationState(const GraphicsPipelineSetup* pipelineSetup);
    VkPipelineMultisampleStateCreateInfo* makeMultisampleState(const GraphicsPipelineSetup* pipelineSetup);
    VkPipelineDepthStencilStateCreateInfo* makeDepthStencilState(const GraphicsPipelineSetup* pipelineSetup);
    VkPipelineRenderingCreateInfo* makeRenderingState(const GraphicsPipelineSetup* pipelineSetup, const void* pNext);
};

}
