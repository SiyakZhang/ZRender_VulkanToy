#pragma once
#include "VKBase.h"

namespace vulkan
{
    struct graphicsPipelineCreateInfoPack
    {
        // 整个打包器最终要产出的核心结构体就是它。
        VkGraphicsPipelineCreateInfo createInfo =
            {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        // 着色器阶段数组，通常至少包含顶点着色器和片段着色器。
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
        // 顶点输入阶段：描述顶点缓冲区如何解释成顶点属性。
        VkPipelineVertexInputStateCreateInfo vertexInputStateCi =
            {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        // 顶点绑定描述：说明每条顶点流的步长、输入频率等。
        std::vector<VkVertexInputBindingDescription> vertexInputBindings;
        // 顶点属性描述：说明位置、法线、颜色等字段从哪里取。
        std::vector<VkVertexInputAttributeDescription> vertexInputAttributes;
        // 图元装配阶段：决定点、线、三角形这些图元如何拼出来。
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCi =
            {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        // 曲面细分阶段：本示例暂时不用，但结构体位置先留好。
        VkPipelineTessellationStateCreateInfo tessellationStateCi =
            {VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO};
        // 视口阶段：决定裁剪空间坐标如何映射到屏幕区域。
        VkPipelineViewportStateCreateInfo viewportStateCi =
            {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        // 静态视口数组；如果使用动态视口，这里可以为空。
        std::vector<VkViewport> viewports;
        // 静态裁剪矩形数组；如果使用动态裁剪，这里也可以为空。
        std::vector<VkRect2D> scissors;
        // 当视口由命令缓冲区动态指定时，仍然要告诉 Vulkan 需要几个视口。
        uint32_t dynamicViewportCount = 1;
        // 当裁剪矩形由命令缓冲区动态指定时，也要告知数量。
        uint32_t dynamicScissorCount = 1;
        // 光栅化阶段：控制面剔除、线宽、填充模式等光栅化行为。
        VkPipelineRasterizationStateCreateInfo rasterizationStateCi =
            {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        // 多重采样阶段：控制 MSAA 相关参数。
        VkPipelineMultisampleStateCreateInfo multisampleStateCi =
            {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        // 深度模板阶段：控制深度测试、模板测试以及写入行为。
        VkPipelineDepthStencilStateCreateInfo depthStencilStateCi =
            {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        // 颜色混合阶段：每个颜色附件都对应一个混合描述。
        VkPipelineColorBlendStateCreateInfo colorBlendStateCi =
            {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        // 颜色附件状态数组：决定每个附件是否混合、写哪些通道。
        std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates;
        // 动态状态阶段：把某些固定功能状态延后到命令录制时再指定。
        VkPipelineDynamicStateCreateInfo dynamicStateCi =
            {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        // 动态状态枚举数组，例如视口、裁剪矩形、线宽等。
        std::vector<VkDynamicState> dynamicStates;
        // --------------------
        graphicsPipelineCreateInfoPack()
        {
            // 先把各子结构体挂到 VkGraphicsPipelineCreateInfo 上。
            SetCreateInfos();
            // 非派生管线时必须显式设为 -1，不能默认留在 0。
            createInfo.basePipelineIndex = -1;
        }

        graphicsPipelineCreateInfoPack(const graphicsPipelineCreateInfoPack& other) noexcept
        {
            // 先拷贝顶层结构体的标量字段。
            createInfo = other.createInfo;
            // 再把 createInfo 里那些“指向本对象成员”的指针重新修正回来。
            SetCreateInfos();

            // 这些都是值语义成员，直接复制即可。
            vertexInputStateCi = other.vertexInputStateCi;
            inputAssemblyStateCi = other.inputAssemblyStateCi;
            tessellationStateCi = other.tessellationStateCi;
            viewportStateCi = other.viewportStateCi;
            rasterizationStateCi = other.rasterizationStateCi;
            multisampleStateCi = other.multisampleStateCi;
            depthStencilStateCi = other.depthStencilStateCi;
            colorBlendStateCi = other.colorBlendStateCi;
            dynamicStateCi = other.dynamicStateCi;

            shaderStages = other.shaderStages;
            vertexInputBindings = other.vertexInputBindings;
            vertexInputAttributes = other.vertexInputAttributes;
            viewports = other.viewports;
            scissors = other.scissors;
            colorBlendAttachmentStates = other.colorBlendAttachmentStates;
            dynamicStates = other.dynamicStates;
            // vector 复制后内部地址变化，因此还要再修正一次所有数组指针。
            UpdateAllArrayAddresses();
        }

        //Getter
        operator VkGraphicsPipelineCreateInfo&()
        {
            return createInfo;
        }

        //Non-const Function
        void SetPipelineLayout(VkPipelineLayout layout)
        {
            // 管线布局描述了这条管线将如何访问描述符和 push constant。
            createInfo.layout = layout;
        }

        void SetRenderPass(VkRenderPass renderPass, uint32_t subpass = 0)
        {
            // 传统渲染路径下，图形管线必须绑定到某个渲染通道和子通道。
            createInfo.renderPass = renderPass;
            createInfo.subpass = subpass;
        }

        void SetShaderStages(arrayRef<const VkPipelineShaderStageCreateInfo> stages)
        {
            // 先复制进内部 vector，这样后续再次 UpdateAllArrays 也不会把阶段数组冲掉。
            shaderStages.assign(stages.begin(), stages.end());
            createInfo.stageCount = uint32_t(shaderStages.size());
            createInfo.pStages = shaderStages.data();
        }

        void UpdateAllArrays()
        {
            // 先把各 vector 当前的元素数量同步到 Vulkan 结构体的 count 字段。
            createInfo.stageCount = uint32_t(shaderStages.size());
            vertexInputStateCi.vertexBindingDescriptionCount = uint32_t(vertexInputBindings.size());
            vertexInputStateCi.vertexAttributeDescriptionCount = uint32_t(vertexInputAttributes.size());
            viewportStateCi.viewportCount = viewports.size()
                                                ? uint32_t(viewports.size())
                                                : dynamicViewportCount;
            viewportStateCi.scissorCount = scissors.size()
                                               ? uint32_t(scissors.size())
                                               : dynamicScissorCount;
            colorBlendStateCi.attachmentCount = uint32_t(colorBlendAttachmentStates.size());
            dynamicStateCi.dynamicStateCount = uint32_t(dynamicStates.size());
            // 再同步所有数组首地址，保证 createInfo 内部指针始终有效。
            UpdateAllArrayAddresses();
        }

    private:
        void SetCreateInfos()
        {
            // 这几个指针字段本质上是把“大结构体”和“子结构体”串起来。
            createInfo.pVertexInputState = &vertexInputStateCi;
            createInfo.pInputAssemblyState = &inputAssemblyStateCi;
            createInfo.pTessellationState = &tessellationStateCi;
            createInfo.pViewportState = &viewportStateCi;
            createInfo.pRasterizationState = &rasterizationStateCi;
            createInfo.pMultisampleState = &multisampleStateCi;
            createInfo.pDepthStencilState = &depthStencilStateCi;
            createInfo.pColorBlendState = &colorBlendStateCi;
            createInfo.pDynamicState = &dynamicStateCi;
        }

        void UpdateAllArrayAddresses()
        {
            // 这里专门负责把各 vector 的 data() 回填给 Vulkan 结构体内部指针。
            createInfo.pStages = shaderStages.data();
            vertexInputStateCi.pVertexBindingDescriptions = vertexInputBindings.data();
            vertexInputStateCi.pVertexAttributeDescriptions = vertexInputAttributes.data();
            viewportStateCi.pViewports = viewports.data();
            viewportStateCi.pScissors = scissors.data();
            colorBlendStateCi.pAttachments = colorBlendAttachmentStates.data();
            dynamicStateCi.pDynamicStates = dynamicStates.data();
        }
    };
}
