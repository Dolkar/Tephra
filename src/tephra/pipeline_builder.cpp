#include "common_impl.hpp"
#include "pipeline_builder.hpp"
#include "job/render_pass.hpp"
#include <tephra/pipeline.hpp>

namespace tp {

template <typename T>
bool isVectorPointerValid(const ScratchVector<T>& vector, const T* pointer) {
    return vector.data() <= pointer && vector.data() + vector.size() >= pointer;
}

void countShaderSetup(
    const ShaderStageSetup& stageSetup,
    std::size_t* setupCount,
    std::size_t* specConstantCount,
    std::size_t* specConstantBytes) {
    *setupCount += 1;
    *specConstantCount += stageSetup.specializationConstants.size();
    for (const SpecializationConstant& constant : stageSetup.specializationConstants) {
        *specConstantBytes += constant.constantSizeBytes;
    }
}

void ShaderStageInfoBuilder::preallocate(
    std::size_t setupCount,
    std::size_t specConstantCount,
    std::size_t specConstantBytes) {
    stageCreateInfos.clear();
    specializationInfos.clear();
    specializationEntries.clear();
    specializationData.clear();

    stageCreateInfos.reserve(setupCount);
    specializationInfos.reserve(setupCount);
    specializationData.reserve(specConstantBytes);
    specializationEntries.reserve(specConstantCount);
}

VkPipelineShaderStageCreateInfo& ShaderStageInfoBuilder::makeInfo(
    const ShaderStageSetup& stageSetup,
    ShaderStage stageType) {
    TEPHRA_ASSERT(stageCreateInfos.size() != stageCreateInfos.capacity());

    VkSpecializationInfo& specializationInfo = specializationInfos.emplace_back();
    specializationInfo.mapEntryCount = 0;
    specializationInfo.pMapEntries = specializationEntries.data() + specializationEntries.size();
    specializationInfo.dataSize = 0;
    specializationInfo.pData = specializationData.data() + specializationData.size();

    for (const SpecializationConstant& constant : stageSetup.specializationConstants) {
        VkSpecializationMapEntry& specEntry = specializationEntries.emplace_back();
        specEntry.constantID = constant.constantID;
        specEntry.offset = static_cast<uint32_t>(specializationInfo.dataSize);
        specEntry.size = constant.constantSizeBytes;
        specializationData.insert(specializationData.end(), constant.data, constant.data + constant.constantSizeBytes);

        specializationInfo.mapEntryCount++;
        specializationInfo.dataSize += constant.constantSizeBytes;
    }

    VkPipelineShaderStageCreateInfo& stageCreateInfo = stageCreateInfos.emplace_back();
    stageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageCreateInfo.pNext = nullptr;
    stageCreateInfo.flags = 0;
    stageCreateInfo.stage = vkCastConvertibleEnum(stageType);
    stageCreateInfo.module = stageSetup.stageModule->vkGetShaderModuleHandle();
    stageCreateInfo.pName = stageSetup.stageEntryPoint;
    stageCreateInfo.pSpecializationInfo = &specializationInfo;

    TEPHRA_ASSERT(isVectorPointerValid(specializationInfos, stageCreateInfo.pSpecializationInfo));
    TEPHRA_ASSERT(isVectorPointerValid(specializationEntries, specializationInfo.pMapEntries));
    TEPHRA_ASSERT(isVectorPointerValid(specializationData, static_cast<const std::byte*>(specializationInfo.pData)));
    return stageCreateInfo;
}

ArrayView<VkComputePipelineCreateInfo> ComputePipelineInfoBuilder::makeInfos(
    ArrayParameter<const ComputePipelineSetup* const> pipelineSetups) {
    pipelineCreateInfos.clear();
    pipelineCreateInfos.reserve(pipelineSetups.size());

    std::size_t shaderSetupCount = 0;
    std::size_t specConstantCount = 0;
    std::size_t specConstantBytes = 0;
    for (const ComputePipelineSetup* pipelineSetup : pipelineSetups) {
        countShaderSetup(pipelineSetup->computeStageSetup, &shaderSetupCount, &specConstantCount, &specConstantBytes);
    }
    shaderStageInfoBuilder.preallocate(pipelineSetups.size(), specConstantCount, specConstantBytes);

    for (const ComputePipelineSetup* pipelineSetup : pipelineSetups) {
        VkComputePipelineCreateInfo& pipelineInfo = pipelineCreateInfos.emplace_back();
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.pNext = pipelineSetup->pNext;
        pipelineInfo.flags = vkCastConvertibleEnumMask(pipelineSetup->flags);
        pipelineInfo.stage = shaderStageInfoBuilder.makeInfo(pipelineSetup->computeStageSetup, ShaderStage::Compute);
        pipelineInfo.layout = pipelineSetup->pipelineLayout->vkGetPipelineLayoutHandle();
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = 0;
    }

    return view(pipelineCreateInfos);
}

ArrayView<VkGraphicsPipelineCreateInfo> GraphicsPipelineInfoBuilder::makeInfos(
    ArrayParameter<const GraphicsPipelineSetup* const> pipelineSetups) {
    vertexInputCreateInfos.clear();
    inputAssemblyCreateInfos.clear();
    tessellationCreateInfos.clear();
    viewportCreateInfos.clear();
    rasterizationCreateInfos.clear();
    sampleMasks.clear();
    multisampleCreateInfos.clear();
    depthStencilCreateInfos.clear();
    colorBlendCreateInfos.clear();

    preallocatePipelineSetups(pipelineSetups);
    for (const GraphicsPipelineSetup* pipelineSetup : pipelineSetups) {
        makePipelineSetup(pipelineSetup);
    }
    return view(pipelineCreateInfos);
}

void GraphicsPipelineInfoBuilder::preallocatePipelineSetups(
    ArrayParameter<const GraphicsPipelineSetup* const> pipelineSetups) {
    // Preallocate all the contiguous vectors, so that we can assume that adding elements won't invalidate pointers
    preallocateShaderStages(pipelineSetups);
    preallocateVertexInputs(pipelineSetups);
    preallocateDynamicStates(pipelineSetups);
    preallocateBlendStates(pipelineSetups);

    pipelineCreateInfos.clear();
    pipelineCreateInfos.reserve(pipelineSetups.size());
}

void GraphicsPipelineInfoBuilder::makePipelineSetup(const GraphicsPipelineSetup* pipelineSetup) {
    VkGraphicsPipelineCreateInfo& pipelineInfo = pipelineCreateInfos.emplace_back();
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = pipelineSetup->pNext;
    pipelineInfo.flags = vkCastConvertibleEnumMask(pipelineSetup->flags);

    ArrayView<VkPipelineShaderStageCreateInfo> shaderStages = makeShaderStages(pipelineSetup);
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pVertexInputState = makeVertexInputs(pipelineSetup);
    pipelineInfo.pInputAssemblyState = makeInputAssemblyState(pipelineSetup);
    pipelineInfo.pTessellationState = makeTessellationState(pipelineSetup);
    pipelineInfo.pViewportState = makeViewportState(pipelineSetup);
    pipelineInfo.pRasterizationState = makeRasterizationState(pipelineSetup);
    pipelineInfo.pMultisampleState = makeMultisampleState(pipelineSetup);
    pipelineInfo.pDepthStencilState = makeDepthStencilState(pipelineSetup);
    pipelineInfo.pColorBlendState = makeColorBlendState(pipelineSetup);
    pipelineInfo.pDynamicState = makeDynamicState(pipelineSetup);

    // setup dynamic rendering state
    pipelineInfo.pNext = makeRenderingState(pipelineSetup, pipelineInfo.pNext);

    pipelineInfo.layout = pipelineSetup->pipelineLayout->vkGetPipelineLayoutHandle();
    pipelineInfo.renderPass = VK_NULL_HANDLE;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = 0;
}

void GraphicsPipelineInfoBuilder::preallocateShaderStages(
    ArrayParameter<const GraphicsPipelineSetup* const> pipelineSetups) {
    std::size_t shaderSetupCount = 0;
    std::size_t specConstantCount = 0;
    std::size_t specConstantBytes = 0;
    for (const GraphicsPipelineSetup* pipelineSetup : pipelineSetups) {
        countShaderSetup(pipelineSetup->vertexStageSetup, &shaderSetupCount, &specConstantCount, &specConstantBytes);
        if (pipelineSetup->fragmentStageSetup.stageModule != nullptr) {
            countShaderSetup(
                pipelineSetup->fragmentStageSetup, &shaderSetupCount, &specConstantCount, &specConstantBytes);
        }
        if (pipelineSetup->geometryStageSetup.stageModule != nullptr) {
            countShaderSetup(
                pipelineSetup->geometryStageSetup, &shaderSetupCount, &specConstantCount, &specConstantBytes);
        }
        if (pipelineSetup->tessellationControlStageSetup.stageModule != nullptr) {
            TEPHRA_ASSERT(pipelineSetup->tessellationEvaluationStageSetup.stageModule != nullptr);
            countShaderSetup(
                pipelineSetup->tessellationControlStageSetup, &shaderSetupCount, &specConstantCount, &specConstantBytes);
            countShaderSetup(
                pipelineSetup->tessellationEvaluationStageSetup,
                &shaderSetupCount,
                &specConstantCount,
                &specConstantBytes);
        }
    }
    shaderStageInfoBuilder.preallocate(shaderSetupCount, specConstantCount, specConstantBytes);
}

ArrayView<VkPipelineShaderStageCreateInfo> GraphicsPipelineInfoBuilder::makeShaderStages(
    const GraphicsPipelineSetup* pipelineSetup) {
    VkPipelineShaderStageCreateInfo* stagePtr = &shaderStageInfoBuilder.makeInfo(
        pipelineSetup->vertexStageSetup, ShaderStage::Vertex);
    std::size_t stageCount = 1;

    if (pipelineSetup->fragmentStageSetup.stageModule != nullptr) {
        shaderStageInfoBuilder.makeInfo(pipelineSetup->fragmentStageSetup, ShaderStage::Fragment);
        stageCount++;
    }
    if (pipelineSetup->geometryStageSetup.stageModule != nullptr) {
        shaderStageInfoBuilder.makeInfo(pipelineSetup->geometryStageSetup, ShaderStage::Geometry);
        stageCount++;
    }
    if (pipelineSetup->tessellationControlStageSetup.stageModule != nullptr) {
        shaderStageInfoBuilder.makeInfo(pipelineSetup->tessellationControlStageSetup, ShaderStage::TessellationControl);
        shaderStageInfoBuilder.makeInfo(
            pipelineSetup->tessellationEvaluationStageSetup, ShaderStage::TessellationEvaluation);
        stageCount += 2;
    }

    return ArrayView<VkPipelineShaderStageCreateInfo>(stagePtr, stageCount);
}

void GraphicsPipelineInfoBuilder::preallocateVertexInputs(
    ArrayParameter<const GraphicsPipelineSetup* const> pipelineSetups) {
    vertexBindingDescriptions.clear();
    vertexAttributeDescriptions.clear();

    std::size_t bindingCount = 0;
    std::size_t attributeCount = 0;
    for (const GraphicsPipelineSetup* pipelineSetup : pipelineSetups) {
        for (const VertexInputBinding& binding : pipelineSetup->vertexInputBindings) {
            bindingCount++;
            attributeCount += binding.attributes.size();
        }
    }
    vertexBindingDescriptions.reserve(bindingCount);
    vertexAttributeDescriptions.reserve(attributeCount);
}

VkPipelineVertexInputStateCreateInfo* GraphicsPipelineInfoBuilder::makeVertexInputs(
    const GraphicsPipelineSetup* pipelineSetup) {
    VkPipelineVertexInputStateCreateInfo& createInfo = vertexInputCreateInfos.emplace_back();
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.vertexBindingDescriptionCount = 0;
    createInfo.pVertexBindingDescriptions = vertexBindingDescriptions.data() + vertexBindingDescriptions.size();
    createInfo.vertexAttributeDescriptionCount = 0;
    createInfo.pVertexAttributeDescriptions = vertexAttributeDescriptions.data() + vertexAttributeDescriptions.size();

    for (uint32_t i = 0; i < pipelineSetup->vertexInputBindings.size(); i++) {
        const VertexInputBinding& binding = pipelineSetup->vertexInputBindings[i];
        VkVertexInputBindingDescription& vkBindingDescription = vertexBindingDescriptions.emplace_back();
        vkBindingDescription.binding = i;
        vkBindingDescription.inputRate = vkCastConvertibleEnum(binding.inputRate);
        vkBindingDescription.stride = binding.stride;
        createInfo.vertexBindingDescriptionCount++;

        for (const VertexInputAttribute& attribute : binding.attributes) {
            VkVertexInputAttributeDescription& vkAttributeDescription = vertexAttributeDescriptions.emplace_back();
            vkAttributeDescription.binding = i;
            vkAttributeDescription.format = vkCastConvertibleEnum(attribute.format);
            vkAttributeDescription.location = attribute.location;
            vkAttributeDescription.offset = attribute.offset;
            createInfo.vertexAttributeDescriptionCount++;
        }
    }

    TEPHRA_ASSERT(isVectorPointerValid(vertexBindingDescriptions, createInfo.pVertexBindingDescriptions));
    TEPHRA_ASSERT(isVectorPointerValid(vertexAttributeDescriptions, createInfo.pVertexAttributeDescriptions));
    return &createInfo;
}

constexpr uint32_t ImplicitDynamicStateCount = 2; // VIEWPORT and SCISSOR
void GraphicsPipelineInfoBuilder::preallocateDynamicStates(
    ArrayParameter<const GraphicsPipelineSetup* const> pipelineSetups) {
    dynamicStates.clear();
    dynamicStateCreateInfos.clear();

    std::size_t stateCount = 0;
    for (const GraphicsPipelineSetup* pipelineSetup : pipelineSetups) {
        stateCount += pipelineSetup->dynamicStates.size() + ImplicitDynamicStateCount;
    }
    dynamicStateCreateInfos.reserve(pipelineSetups.size());
    dynamicStates.reserve(stateCount);
}

VkPipelineDynamicStateCreateInfo* GraphicsPipelineInfoBuilder::makeDynamicState(
    const GraphicsPipelineSetup* pipelineSetup) {
    VkPipelineDynamicStateCreateInfo& createInfo = dynamicStateCreateInfos.emplace_back();
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.pDynamicStates = dynamicStates.data() + dynamicStates.size();

    dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);

    for (const DynamicState& dynamicState : pipelineSetup->dynamicStates) {
        dynamicStates.push_back(vkCastConvertibleEnum(dynamicState));
    }

    createInfo.dynamicStateCount = static_cast<uint32_t>(
        pipelineSetup->dynamicStates.size() + ImplicitDynamicStateCount);

    TEPHRA_ASSERT(isVectorPointerValid(dynamicStates, createInfo.pDynamicStates));
    return &createInfo;
}

void GraphicsPipelineInfoBuilder::preallocateBlendStates(
    ArrayParameter<const GraphicsPipelineSetup* const> pipelineSetups) {
    blendAttachmentStates.clear();

    std::size_t attachmentStateCount = 0;
    for (const GraphicsPipelineSetup* pipelineSetup : pipelineSetups) {
        attachmentStateCount += pipelineSetup->colorAttachmentFormats.size();
    }
    blendAttachmentStates.reserve(attachmentStateCount);
}

VkPipelineColorBlendStateCreateInfo* GraphicsPipelineInfoBuilder::makeColorBlendState(
    const GraphicsPipelineSetup* pipelineSetup) {
    auto makeBlendAttachmentState = [](const AttachmentBlendState& blendState,
                                       VkPipelineColorBlendAttachmentState* vkBlendState) {
        vkBlendState->blendEnable = blendState != AttachmentBlendState::NoBlend();
        vkBlendState->srcColorBlendFactor = vkCastConvertibleEnum(blendState.colorBlend.srcBlendFactor);
        vkBlendState->dstColorBlendFactor = vkCastConvertibleEnum(blendState.colorBlend.dstBlendFactor);
        vkBlendState->colorBlendOp = vkCastConvertibleEnum(blendState.colorBlend.blendOp);
        vkBlendState->srcAlphaBlendFactor = vkCastConvertibleEnum(blendState.alphaBlend.srcBlendFactor);
        vkBlendState->dstAlphaBlendFactor = vkCastConvertibleEnum(blendState.alphaBlend.dstBlendFactor);
        vkBlendState->alphaBlendOp = vkCastConvertibleEnum(blendState.alphaBlend.blendOp);
        vkBlendState->colorWriteMask = vkCastConvertibleEnumMask(blendState.writeMask);
    };

    uint32_t colorAttachmentCount = static_cast<uint32_t>(pipelineSetup->colorAttachmentFormats.size());

    VkPipelineColorBlendStateCreateInfo& createInfo = colorBlendCreateInfos.emplace_back();
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.logicOpEnable = pipelineSetup->logicBlendEnable;
    createInfo.logicOp = vkCastConvertibleEnum(pipelineSetup->logicBlendOp);
    createInfo.attachmentCount = colorAttachmentCount;
    createInfo.pAttachments = blendAttachmentStates.data() + blendAttachmentStates.size();
    for (int i = 0; i < 4; i++) {
        createInfo.blendConstants[i] = pipelineSetup->blendConstants[i];
    }

    if (pipelineSetup->blendEnable && pipelineSetup->independentBlendEnable) {
        TEPHRA_ASSERT(pipelineSetup->blendStates.size() == colorAttachmentCount);
        for (const AttachmentBlendState& blendState : pipelineSetup->blendStates) {
            VkPipelineColorBlendAttachmentState& vkBlendState = blendAttachmentStates.emplace_back();
            makeBlendAttachmentState(blendState, &vkBlendState);
        }
    } else if (pipelineSetup->blendEnable) {
        // Blending, but not independent blending
        TEPHRA_ASSERT(pipelineSetup->blendStates.size() == 1);
        for (uint32_t i = 0; i < colorAttachmentCount; i++) {
            VkPipelineColorBlendAttachmentState& vkBlendState = blendAttachmentStates.emplace_back();
            makeBlendAttachmentState(pipelineSetup->blendStates[0], &vkBlendState);
        }
    } else {
        // No blending
        TEPHRA_ASSERT(pipelineSetup->blendStates.size() == 0);
        for (uint32_t i = 0; i < colorAttachmentCount; i++) {
            VkPipelineColorBlendAttachmentState& vkBlendState = blendAttachmentStates.emplace_back();
            makeBlendAttachmentState(AttachmentBlendState::NoBlend(), &vkBlendState);
        }
    }

    TEPHRA_ASSERT(isVectorPointerValid(blendAttachmentStates, createInfo.pAttachments));
    return &createInfo;
}

VkPipelineInputAssemblyStateCreateInfo* GraphicsPipelineInfoBuilder::makeInputAssemblyState(
    const GraphicsPipelineSetup* pipelineSetup) {
    VkPipelineInputAssemblyStateCreateInfo& createInfo = inputAssemblyCreateInfos.emplace_back();
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.topology = vkCastConvertibleEnum(pipelineSetup->topology);
    createInfo.primitiveRestartEnable = pipelineSetup->primitiveRestartEnable;
    return &createInfo;
}

VkPipelineTessellationStateCreateInfo* GraphicsPipelineInfoBuilder::makeTessellationState(
    const GraphicsPipelineSetup* pipelineSetup) {
    if (pipelineSetup->tessellationControlStageSetup.stageModule == nullptr) {
        return nullptr;
    }

    VkPipelineTessellationStateCreateInfo& createInfo = tessellationCreateInfos.emplace_back();
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.patchControlPoints = pipelineSetup->patchControlPoints;
    return &createInfo;
}

VkPipelineViewportStateCreateInfo* GraphicsPipelineInfoBuilder::makeViewportState(
    const GraphicsPipelineSetup* pipelineSetup) {
    VkPipelineViewportStateCreateInfo& createInfo = viewportCreateInfos.emplace_back();
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.viewportCount = pipelineSetup->viewportCount;
    createInfo.pViewports = nullptr; // Always a dynamic state
    createInfo.scissorCount = pipelineSetup->viewportCount;
    createInfo.pScissors = nullptr;
    return &createInfo;
}

VkPipelineRasterizationStateCreateInfo* GraphicsPipelineInfoBuilder::makeRasterizationState(
    const GraphicsPipelineSetup* pipelineSetup) {
    VkPipelineRasterizationStateCreateInfo& createInfo = rasterizationCreateInfos.emplace_back();
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.depthClampEnable = pipelineSetup->depthClampEnable;
    switch (pipelineSetup->rasterizationMode) {
    case RasterizationMode::Discard:
        createInfo.rasterizerDiscardEnable = true;
        break;
    case RasterizationMode::Fill:
        createInfo.rasterizerDiscardEnable = false;
        createInfo.polygonMode = VK_POLYGON_MODE_FILL;
        break;
    case RasterizationMode::Line:
        createInfo.rasterizerDiscardEnable = false;
        createInfo.polygonMode = VK_POLYGON_MODE_LINE;
        break;
    case RasterizationMode::Point:
        createInfo.rasterizerDiscardEnable = false;
        createInfo.polygonMode = VK_POLYGON_MODE_POINT;
        break;
    }

    createInfo.cullMode = vkCastConvertibleEnumMask(pipelineSetup->cullMode);
    createInfo.frontFace = pipelineSetup->frontFaceIsClockwise ? VK_FRONT_FACE_CLOCKWISE :
                                                                 VK_FRONT_FACE_COUNTER_CLOCKWISE;
    createInfo.depthBiasEnable = pipelineSetup->depthBiasEnable;
    createInfo.depthBiasConstantFactor = pipelineSetup->depthBiasConstantFactor;
    createInfo.depthBiasClamp = pipelineSetup->depthBiasClamp;
    createInfo.depthBiasSlopeFactor = pipelineSetup->depthBiasSlopeFactor;
    createInfo.lineWidth = pipelineSetup->lineWidth;
    return &createInfo;
}

VkPipelineMultisampleStateCreateInfo* GraphicsPipelineInfoBuilder::makeMultisampleState(
    const GraphicsPipelineSetup* pipelineSetup) {
    MultisampleLevel multisampleLevel = pipelineSetup->multisampleLevel;

    VkPipelineMultisampleStateCreateInfo& createInfo = multisampleCreateInfos.emplace_back();
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.rasterizationSamples = vkCastConvertibleEnum(multisampleLevel);
    createInfo.sampleShadingEnable = pipelineSetup->sampleShadingEnable;
    createInfo.minSampleShading = pipelineSetup->minSampleShading;

    std::array<VkSampleMask, 2>& sampleMask = sampleMasks.emplace_back();
    sampleMask[0] = static_cast<VkSampleMask>(pipelineSetup->sampleMask);
    sampleMask[1] = static_cast<VkSampleMask>(pipelineSetup->sampleMask >> 32);
    createInfo.pSampleMask = sampleMask.data();
    createInfo.alphaToCoverageEnable = pipelineSetup->alphaToCoverageEnable;
    createInfo.alphaToOneEnable = pipelineSetup->alphaToOneEnable;
    return &createInfo;
}

VkPipelineDepthStencilStateCreateInfo* GraphicsPipelineInfoBuilder::makeDepthStencilState(
    const GraphicsPipelineSetup* pipelineSetup) {
    VkPipelineDepthStencilStateCreateInfo& createInfo = depthStencilCreateInfos.emplace_back();
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.depthTestEnable = pipelineSetup->depthTestEnable;
    createInfo.depthWriteEnable = pipelineSetup->depthWriteEnable;
    createInfo.depthCompareOp = vkCastConvertibleEnum(pipelineSetup->depthTestCompareOp);
    createInfo.depthBoundsTestEnable = pipelineSetup->depthBoundsTestEnable;
    createInfo.stencilTestEnable = pipelineSetup->stencilTestEnable;
    createInfo.front = vkCastConvertibleStruct(pipelineSetup->frontFaceStencilState);
    createInfo.back = vkCastConvertibleStruct(pipelineSetup->backFaceStencilState);
    createInfo.minDepthBounds = pipelineSetup->minDepthBounds;
    createInfo.maxDepthBounds = pipelineSetup->maxDepthBounds;
    return &createInfo;
}

VkPipelineRenderingCreateInfo* GraphicsPipelineInfoBuilder::makeRenderingState(
    const GraphicsPipelineSetup* pipelineSetup,
    const void* pNext) {
    VkPipelineRenderingCreateInfo& createInfo = renderingCreateInfos.emplace_back();
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    createInfo.pNext = pNext;
    createInfo.viewMask = pipelineSetup->viewMask;
    createInfo.colorAttachmentCount = static_cast<uint32_t>(pipelineSetup->colorAttachmentFormats.size());
    createInfo.pColorAttachmentFormats = vkCastConvertibleEnumPtr(pipelineSetup->colorAttachmentFormats.data());

    if (pipelineSetup->depthStencilAspects.contains(ImageAspect::Depth))
        createInfo.depthAttachmentFormat = vkCastConvertibleEnum(pipelineSetup->depthStencilAttachmentFormat);
    else
        createInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;

    if (pipelineSetup->depthStencilAspects.contains(ImageAspect::Stencil))
        createInfo.stencilAttachmentFormat = vkCastConvertibleEnum(pipelineSetup->depthStencilAttachmentFormat);
    else
        createInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    return &createInfo;
}

}
