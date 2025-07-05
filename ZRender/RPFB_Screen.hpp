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

        // 颜色附件直接使用交换链图像格式。
        VkAttachmentDescription attachmentDescription = {
            .format = graphicsBase::Base().SwapchainCreateInfo().imageFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};

        // 这个子通道只有一个颜色附件，附件索引就是 0。
        VkAttachmentReference attachmentReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

        // 渲染通道当前只包含一个图形子通道。
        VkSubpassDescription subpassDescription = {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &attachmentReference};

        // 子通道依赖负责把外部状态过渡到颜色附件输出阶段。
        VkSubpassDependency subpassDependency = {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT};

        // 把附件、子通道和依赖关系打包成渲染通道创建信息。
        VkRenderPassCreateInfo renderPassCreateInfo = {
            .attachmentCount = 1,
            .pAttachments = &attachmentDescription,
            .subpassCount = 1,
            .pSubpasses = &subpassDescription,
            .dependencyCount = 1,
            .pDependencies = &subpassDependency};

        // 真正创建渲染通道对象。
        rpwf.renderPass.Create(renderPassCreateInfo);

        auto CreateFramebuffers = [] {
            // 每张交换链图像都需要一个对应的帧缓冲。
            rpwf.framebuffers.resize(graphicsBase::Base().SwapchainImageCount());

            // 帧缓冲尺寸与交换链图像尺寸保持一致。
            VkFramebufferCreateInfo framebufferCreateInfo = {
                .renderPass = rpwf.renderPass,
                .attachmentCount = 1,
                .width = windowSize.width,
                .height = windowSize.height,
                .layers = 1};

            // 为每一张交换链 image view 创建一个 framebuffer。
            for (size_t i = 0; i < graphicsBase::Base().SwapchainImageCount(); ++i)
            {
                VkImageView attachment = graphicsBase::Base().SwapchainImageView(static_cast<uint32_t>(i));
                framebufferCreateInfo.pAttachments = &attachment;
                rpwf.framebuffers[i].Create(framebufferCreateInfo);
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
