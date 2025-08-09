#pragma once

#include "VKBase.h"

using namespace vulkan;

// 直接引用交换链当前的图像尺寸，后面创建帧缓冲时会反复用到。
const VkExtent2D& windowSize = graphicsBase::Base().SwapchainCreateInfo().imageExtent;

namespace easyVulkan
{
    struct renderPassWithFramebuffers
    {
        // 一个屏幕渲染通道。
        renderPass renderPass;

        // 与每张交换链图像一一对应的帧缓冲。
        std::vector<framebuffer> framebuffers;
    };

    const renderPassWithFramebuffers& CreateRpwf_Screen()
    {
        // 用静态对象缓存结果，避免重复创建同一组对象。
        static renderPassWithFramebuffers rpwf;

        // 当前示例只有一个颜色附件，它直接写入交换链图像。
        VkAttachmentDescription attachmentDescriptions[] = {{
            .format = graphicsBase::Base().SwapchainCreateInfo().imageFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR}};

        // 这个子通道只有一个颜色附件，附件索引就是 0。
        VkAttachmentReference attachmentReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

        // 当前渲染通道也只有一个图形子通道。
        VkSubpassDescription subpassDescriptions[] = {{
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &attachmentReference}};

        // 子通道依赖负责把外部状态过渡到颜色附件输出阶段。
        VkSubpassDependency subpassDependencies[] = {{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT}};

        // 直接把“附件数组 + 子通道数组 + 依赖数组”交给这一章补好的 renderPass 封装。
        rpwf.renderPass.Create(attachmentDescriptions, subpassDescriptions, subpassDependencies);

        auto CreateFramebuffers = [] {
            // 每张交换链图像都需要一个对应的帧缓冲。
            rpwf.framebuffers.resize(graphicsBase::Base().SwapchainImageCount());

            // 为每一张交换链 image view 创建一个 framebuffer，它们共享同一个 render pass 模板。
            for (size_t i = 0; i < graphicsBase::Base().SwapchainImageCount(); ++i)
            {
                // 这一帧缓冲只挂一个颜色附件，也就是当前这张交换链 image view。
                VkImageView attachment = graphicsBase::Base().SwapchainImageView(static_cast<uint32_t>(i));

                // 直接调用 framebuffer 的章节级辅助重载，让参数语义更直观。
                rpwf.framebuffers[i].Create(rpwf.renderPass, attachment, windowSize);
            }
        };

        auto DestroyFramebuffers = [] {
            // 清空 vector 时会顺带析构内部 framebuffer 包装对象。
            rpwf.framebuffers.clear();
        };

        // 第一次进入时先立即创建当前交换链对应的帧缓冲。
        CreateFramebuffers();

        // 保证后续重复调用时直接返回现成对象。
        ExecuteOnce(rpwf);

        // 交换链重建后需要重新创建帧缓冲。
        graphicsBase::Base().AddCallback_CreateSwapchain(CreateFramebuffers);

        // 交换链销毁前要先销毁旧帧缓冲。
        graphicsBase::Base().AddCallback_DestroySwapchain(DestroyFramebuffers);

        return rpwf;
    }
}
