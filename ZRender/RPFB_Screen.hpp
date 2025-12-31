#pragma once

#include "VKBase+.h"

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

    void BootScreen(const char* imagePath, VkFormat imageFormat)
    {
        // 先把启动图从磁盘读进内存。
        VkExtent2D imageExtent{};
        auto pImageData = texture2d::LoadFile(imagePath, imageExtent, FormatInfo(imageFormat));
        if (!pImageData)
            return;

        // 把像素数据先写进主线程共用的 staging buffer。
        const VkDeviceSize imageDataSize = VkDeviceSize(FormatInfo(imageFormat).sizePerPixel) * imageExtent.width * imageExtent.height;
        stagingBuffer::BufferData_MainThread(pImageData.get(), imageDataSize);

        // 启动画面只做一次提交，所以这里临时准备同步对象和命令缓冲区即可。
        semaphore semaphore_imageIsAvailable;
        fence fence;
        commandBuffer commandBuffer;
        graphicsBase::Plus().CommandPool_Graphics().AllocateBuffers(commandBuffer);

        // 先取一张交换链图像出来，准备把启动图拷进去。
        graphicsBase::Base().SwapImage(semaphore_imageIsAvailable);

        // 开始录制一次性命令缓冲区。
        commandBuffer.BeginOneTime();

        // 拿到交换链图像的尺寸，用来判断是否需要 blit。
        const VkExtent2D swapchainImageSize = graphicsBase::Base().SwapchainCreateInfo().imageExtent;

        // 只要尺寸不同，或者像素格式不同，就不能直接 copy，必须先走 blit。
        const bool blit =
            imageExtent.width != swapchainImageSize.width ||
            imageExtent.height != swapchainImageSize.height ||
            imageFormat != graphicsBase::Base().SwapchainCreateInfo().imageFormat;

        // 若需要自己创建中转图像，就让这个对象托管其生命周期。
        imageMemory imageMemory_boot;

        if (blit)
        {
            // 优先尝试把 staging buffer 直接别名成一张线性 tiling 的源图像。
            VkImage image = stagingBuffer::AliasedImage2d_MainThread(imageFormat, imageExtent);

            if (image)
            {
                // 别名图像初始布局是 PREINITIALIZED，先转成 transfer-src。
                VkImageMemoryBarrier imageMemoryBarrier = {
                    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    nullptr,
                    0,
                    VK_ACCESS_TRANSFER_READ_BIT,
                    VK_IMAGE_LAYOUT_PREINITIALIZED,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_QUEUE_FAMILY_IGNORED,
                    VK_QUEUE_FAMILY_IGNORED,
                    image,
                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

                vkCmdPipelineBarrier(
                    commandBuffer,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    0,
                    0,
                    nullptr,
                    0,
                    nullptr,
                    1,
                    &imageMemoryBarrier);
            }
            else
            {
                // 若不能别名，就创建一张 device-local 中转图像。
                VkImageCreateInfo imageCreateInfo = {
                    .imageType = VK_IMAGE_TYPE_2D,
                    .format = imageFormat,
                    .extent = {imageExtent.width, imageExtent.height, 1},
                    .mipLevels = 1,
                    .arrayLayers = 1,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT};

                imageMemory_boot.Create(imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

                // 先把 staging buffer 里的像素数据 copy 到这张中转图像里。
                VkBufferImageCopy region_copy = {
                    .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                    .imageExtent = imageCreateInfo.extent};

                imageOperation::CmdCopyBufferToImage(
                    commandBuffer,
                    stagingBuffer::Buffer_MainThread(),
                    imageMemory_boot.Image(),
                    region_copy,
                    {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED},
                    {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL});

                // 后面统一从 image 这个句柄继续 blit 到交换链图像。
                image = imageMemory_boot.Image();
            }

            // 用 blit 把启动图缩放/格式转换到当前交换链图像里。
            VkImageBlit region_blit = {
                {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                {{}, {int32_t(imageExtent.width), int32_t(imageExtent.height), 1}},
                {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                {{}, {int32_t(swapchainImageSize.width), int32_t(swapchainImageSize.height), 1}}};

            imageOperation::CmdBlitImage(
                commandBuffer,
                image,
                graphicsBase::Base().SwapchainImage(graphicsBase::Base().CurrentImageIndex()),
                region_blit,
                {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED},
                {VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR},
                VK_FILTER_LINEAR);
        }
        else
        {
            // 若尺寸和格式都完全一致，直接从 staging buffer copy 到交换链图像即可。
            VkBufferImageCopy region_copy = {
                .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                .imageExtent = {imageExtent.width, imageExtent.height, 1}};

            imageOperation::CmdCopyBufferToImage(
                commandBuffer,
                stagingBuffer::Buffer_MainThread(),
                graphicsBase::Base().SwapchainImage(graphicsBase::Base().CurrentImageIndex()),
                region_copy,
                {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED},
                {VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR});
        }

        // 结束命令录制。
        commandBuffer.End();

        // 启动画面提交时，需要等待交换链图像先变为可用。
        VkPipelineStageFlags waitDstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo submitInfo = {
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = semaphore_imageIsAvailable.Address(),
            .pWaitDstStageMask = &waitDstStage,
            .commandBufferCount = 1,
            .pCommandBuffers = commandBuffer.Address()};

        // 提交并等待执行完成。
        graphicsBase::Base().SubmitCommandBuffer_Graphics(submitInfo, fence);
        fence.WaitAndReset();

        // 直接把这张已经写好的交换链图像呈现出去。
        graphicsBase::Base().PresentImage();

        // 释放临时命令缓冲区。
        graphicsBase::Plus().CommandPool_Graphics().FreeBuffers(commandBuffer);
    }
}
