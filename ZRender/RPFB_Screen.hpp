#pragma once

#include "VKBase.h"

using namespace vulkan;

// 这里继续直接引用交换链图像的尺寸。
// 后面创建无图像帧缓冲时，宽高要求仍然必须与真正绑定上来的图像视图相匹配。
const VkExtent2D& windowSize = graphicsBase::Base().SwapchainCreateInfo().imageExtent;

namespace easyVulkan
{
    struct renderPassWithFramebuffer
    {
        // 这一章仍然只需要一个屏幕渲染通道。
        renderPass renderPass;

        // 与旧方案不同，这里只保留一个“无图像帧缓冲”对象。
        // 真正的交换链图像不会在创建 framebuffer 时写死，而是在开始渲染时再指定。
        framebuffer framebuffer;
    };

    const renderPassWithFramebuffer& CreateRpwf_Screen_ImagelessFramebuffer()
    {
        // 继续用静态对象缓存结果，避免重复创建同一套屏幕渲染资源。
        static renderPassWithFramebuffer rpwf;

        // 颜色附件仍然描述“最终要写到交换链图像里”这件事。
        // 这里只描述附件格式与布局转换规则，不直接绑定具体图像。
        VkAttachmentDescription attachmentDescriptions[] = {{
            .format = graphicsBase::Base().SwapchainCreateInfo().imageFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR}};

        // 本章仍然只有一个颜色附件，所以索引依旧是 0。
        VkAttachmentReference attachmentReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

        // 子通道的结构没有因为 imageless framebuffer 而变化。
        VkSubpassDescription subpassDescriptions[] = {{
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &attachmentReference}};

        // 子通道依赖也保持之前的屏幕渲染写法即可。
        VkSubpassDependency subpassDependencies[] = {{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT}};

        // render pass 本身仍然照常创建。
        rpwf.renderPass.Create(attachmentDescriptions, subpassDescriptions, subpassDependencies);

        auto CreateFramebuffer = [] {
            // 无图像帧缓冲虽然不直接拿到 VkImageView，
            // 但它依然要提前声明“未来会接什么样的图像附件进来”。
            const VkFormat attachmentFormat = graphicsBase::Base().SwapchainCreateInfo().imageFormat;

            VkFramebufferAttachmentImageInfo framebufferAttachmentImageInfo = {
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,
                .usage = graphicsBase::Base().SwapchainCreateInfo().imageUsage,
                .width = windowSize.width,
                .height = windowSize.height,
                .layerCount = 1,
                .viewFormatCount = 1,
                .pViewFormats = &attachmentFormat};

            // 如果交换链启用了某些影响图像创建方式的标志，
            // 那么这里也要把等价的 image flag 写出来，保证匹配关系成立。
            const VkSwapchainCreateFlagsKHR swapchainFlags = graphicsBase::Base().SwapchainCreateInfo().flags;
            if (swapchainFlags & VK_SWAPCHAIN_CREATE_SPLIT_INSTANCE_BIND_REGIONS_BIT_KHR)
                framebufferAttachmentImageInfo.flags |= VK_IMAGE_CREATE_SPLIT_INSTANCE_BIND_REGIONS_BIT;
            if (swapchainFlags & VK_SWAPCHAIN_CREATE_PROTECTED_BIT_KHR)
                framebufferAttachmentImageInfo.flags |= VK_IMAGE_CREATE_PROTECTED_BIT;
            if (swapchainFlags & VK_SWAPCHAIN_CREATE_MUTABLE_FORMAT_BIT_KHR)
                framebufferAttachmentImageInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;

            // 这个结构把“附件要求数组”挂到 VkFramebufferCreateInfo 的 pNext 链上。
            VkFramebufferAttachmentsCreateInfo framebufferAttachmentsCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO,
                .attachmentImageInfoCount = 1,
                .pAttachmentImageInfos = &framebufferAttachmentImageInfo};

            // 关键点在于：
            // 1. 通过 VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT 声明这是无图像帧缓冲；
            // 2. 不提供 pAttachments；
            // 3. 宽高层数只描述约束，而不是具体资源。
            VkFramebufferCreateInfo framebufferCreateInfo = {
                .pNext = &framebufferAttachmentsCreateInfo,
                .flags = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT,
                .renderPass = rpwf.renderPass,
                .attachmentCount = 1,
                .width = windowSize.width,
                .height = windowSize.height,
                .layers = 1};

            // 真正创建 framebuffer。
            rpwf.framebuffer.Create(framebufferCreateInfo);
        };

        auto DestroyFramebuffer = [] {
            // 交换链销毁前先把旧 framebuffer 释放掉。
            rpwf.framebuffer.~framebuffer();
        };

        // 首次进入时先为当前交换链创建一份匹配的 imageless framebuffer。
        CreateFramebuffer();

        // 后续重复调用时直接返回缓存对象，不再重复注册回调。
        ExecuteOnce(rpwf);

        // 交换链重建后，新的尺寸或标志可能已经变化，因此要重新创建 framebuffer。
        graphicsBase::Base().AddCallback_CreateSwapchain(CreateFramebuffer);

        // 交换链销毁前则销毁旧的 framebuffer。
        graphicsBase::Base().AddCallback_DestroySwapchain(DestroyFramebuffer);

        return rpwf;
    }
}
