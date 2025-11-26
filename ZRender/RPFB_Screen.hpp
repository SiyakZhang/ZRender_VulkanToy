#pragma once

#include "VKBase.h"

using namespace vulkan;

// 继续直接引用交换链图像的尺寸。
// 后面创建每一张交换链图像对应的 framebuffer 时都会用到它。
const VkExtent2D& windowSize = graphicsBase::Base().SwapchainCreateInfo().imageExtent;

namespace easyVulkan
{
    struct renderPassWithFramebuffers
    {
        // 这是屏幕渲染使用的 render pass。
        renderPass renderPass;

        // 传统路径下，每一张交换链图像都对应一个 framebuffer。
        std::vector<framebuffer> framebuffers;
    };

    const renderPassWithFramebuffers& CreateRpwf_Screen()
    {
        // 用静态对象缓存结果，避免重复创建同一套屏幕渲染资源。
        static renderPassWithFramebuffers rpwf;

        // 当前示例只有一个颜色附件，它最终会写入交换链图像。
        VkAttachmentDescription attachmentDescriptions[] = {{
            .format = graphicsBase::Base().SwapchainCreateInfo().imageFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR}};

        // 子通道里唯一的颜色附件索引就是 0。
        VkAttachmentReference attachmentReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

        // 当前 render pass 仍然只包含一个图形子通道。
        VkSubpassDescription subpassDescriptions[] = {{
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &attachmentReference}};

        // 这条依赖负责把外部状态过渡到颜色附件输出阶段。
        VkSubpassDependency subpassDependencies[] = {{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT}};

        // 创建 render pass。
        rpwf.renderPass.Create(attachmentDescriptions, subpassDescriptions, subpassDependencies);

        auto CreateFramebuffers = [] {
            // 交换链里有几张图像，这里就准备几个 framebuffer。
            rpwf.framebuffers.resize(graphicsBase::Base().SwapchainImageCount());

            // 为每一张交换链 image view 创建对应的 framebuffer。
            for (size_t i = 0; i < graphicsBase::Base().SwapchainImageCount(); ++i)
            {
                // 当前 framebuffer 只挂一个颜色附件，也就是这张交换链 image view。
                VkImageView attachment = graphicsBase::Base().SwapchainImageView(static_cast<uint32_t>(i));

                // 利用封装好的辅助接口创建 framebuffer。
                rpwf.framebuffers[i].Create(rpwf.renderPass, attachment, windowSize);
            }
        };

        auto DestroyFramebuffers = [] {
            // 清空容器时会顺带析构内部的 framebuffer 包装对象。
            rpwf.framebuffers.clear();
        };

        // 首次进入时先创建当前交换链对应的 framebuffer 集合。
        CreateFramebuffers();

        // 后续重复调用时直接返回缓存对象，不再重复注册回调。
        ExecuteOnce(rpwf);

        // 交换链重建后需要重新创建 framebuffer。
        graphicsBase::Base().AddCallback_CreateSwapchain(CreateFramebuffers);

        // 交换链销毁前先销毁旧 framebuffer。
        graphicsBase::Base().AddCallback_DestroySwapchain(DestroyFramebuffers);

        return rpwf;
    }
}
