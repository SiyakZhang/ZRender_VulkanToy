#pragma once

#include "VKBase.h"
#include "VKFormat.h"
#include "VulkanGraphicsPipelineBuilder.h"

namespace vulkan
{
    // 当前工程在更早的提交里已经把 graphicsPipelineCreateInfoPack 拆到了
    // VulkanGraphicsPipelineBuilder.h 中，这里通过包含它来继续沿用教程“VKBase+.h
    // 统一提供增强封装”的使用方式。

    class graphicsBasePlus
    {
        // 缓存 Vulkan 1.0 范围内各格式的特性，后续查询时就不用反复打驱动。
        VkFormatProperties formatProperties[formatInfoCount_v1_0] = {};

        // 图形队列专用命令池。
        commandPool commandPool_graphics;

        // 呈现队列专用命令池；只有图形队列与呈现队列分离时才真的需要。
        commandPool commandPool_presentation;

        // 计算队列专用命令池。
        commandPool commandPool_compute;

        // 一个长期复用的“传输用”命令缓冲区，从图形命令池分配。
        commandBuffer commandBuffer_transfer;

        // 当图像所有权需要转给呈现队列时，使用这个命令缓冲区。
        commandBuffer commandBuffer_presentation;

        // 扩展层本身也做成单例，和 graphicsBase 的生命周期保持一致。
        static graphicsBasePlus singleton;

        graphicsBasePlus()
        {
            auto Initialize = [] {
                auto& base = graphicsBase::Base();

                // 图形队列存在时，创建图形命令池并分配一个常驻传输命令缓冲区。
                if (base.QueueFamilyIndex_Graphics() != VK_QUEUE_FAMILY_IGNORED)
                {
                    singleton.commandPool_graphics.Create(
                        base.QueueFamilyIndex_Graphics(),
                        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
                    singleton.commandPool_graphics.AllocateBuffers(singleton.commandBuffer_transfer);
                }

                // 计算队列若存在，就给它单独准备命令池。
                if (base.QueueFamilyIndex_Compute() != VK_QUEUE_FAMILY_IGNORED)
                {
                    singleton.commandPool_compute.Create(
                        base.QueueFamilyIndex_Compute(),
                        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
                }

                // 若呈现队列与图形队列分离，且交换链使用独占模式，就需要显式做所有权转移。
                if (base.QueueFamilyIndex_Presentation() != VK_QUEUE_FAMILY_IGNORED &&
                    base.QueueFamilyIndex_Presentation() != base.QueueFamilyIndex_Graphics() &&
                    base.SwapchainCreateInfo().imageSharingMode == VK_SHARING_MODE_EXCLUSIVE)
                {
                    singleton.commandPool_presentation.Create(
                        base.QueueFamilyIndex_Presentation(),
                        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
                    singleton.commandPool_presentation.AllocateBuffers(singleton.commandBuffer_presentation);
                }

                // 预先把常见格式特性全部查出来，后面封装里直接走缓存。
                for (size_t i = 0; i < formatInfoCount_v1_0; ++i)
                {
                    vkGetPhysicalDeviceFormatProperties(
                        base.PhysicalDevice(),
                        static_cast<VkFormat>(i),
                        &singleton.formatProperties[i]);
                }
            };

            auto CleanUp = [] {
                // 命令池析构时会连带释放它分配出的命令缓冲区。
                singleton.commandPool_graphics.~commandPool();
                singleton.commandPool_presentation.~commandPool();
                singleton.commandPool_compute.~commandPool();
            };

            // 把扩展层挂到 graphicsBase 上，后续即可通过 graphicsBase::Plus() 访问。
            graphicsBase::Plus(singleton);

            // 让扩展层跟随逻辑设备的创建 / 销毁自动初始化与清理。
            graphicsBase::Base().AddCallback_CreateDevice(Initialize);
            graphicsBase::Base().AddCallback_DestroyDevice(CleanUp);
        }

        graphicsBasePlus(graphicsBasePlus&&) = delete;
        ~graphicsBasePlus() = default;

    public:
        // 查询某个格式的 Vulkan 特性位。
        const VkFormatProperties& FormatProperties(VkFormat format) const
        {
#ifndef NDEBUG
            if (uint32_t(format) >= formatInfoCount_v1_0)
            {
                outStream << "[ graphicsBasePlus ] ERROR\nThis function only supports definite formats provided by VK_VERSION_1_0.\n";
                abort();
            }
#endif
            return formatProperties[uint32_t(format)];
        }

        // 取图形命令池，后续很多资源上传都会用到它。
        const commandPool& CommandPool_Graphics() const
        {
            return commandPool_graphics;
        }

        // 取计算命令池。
        const commandPool& CommandPool_Compute() const
        {
            return commandPool_compute;
        }

        // 取常驻传输命令缓冲区。
        const commandBuffer& CommandBuffer_Transfer() const
        {
            return commandBuffer_transfer;
        }

        // 提交一个图形命令缓冲区并同步等待完成，适合后续的“立即执行”型辅助封装。
        result_t ExecuteCommandBuffer_Graphics(VkCommandBuffer commandBuffer) const
        {
            fence executionFence;
            VkSubmitInfo submitInfo = {
                .commandBufferCount = 1,
                .pCommandBuffers = &commandBuffer};
            VkResult result = graphicsBase::Base().SubmitCommandBuffer_Graphics(submitInfo, executionFence);
            if (!result)
                executionFence.Wait();
            return result;
        }

        // 计算队列版本的立即执行封装。
        result_t ExecuteCommandBuffer_Compute(VkCommandBuffer commandBuffer) const
        {
            fence executionFence;
            VkSubmitInfo submitInfo = {
                .commandBufferCount = 1,
                .pCommandBuffers = &commandBuffer};
            VkResult result = graphicsBase::Base().SubmitCommandBuffer_Compute(submitInfo, executionFence);
            if (!result)
                executionFence.Wait();
            return result;
        }

        // 若交换链图像需要从图形队列转移到呈现队列，就在这里录制并提交所有权转移命令。
        result_t AcquireImageOwnership_Presentation(
            VkSemaphore semaphore_renderingIsOver,
            VkSemaphore semaphore_ownershipIsTransfered,
            VkFence fence = VK_NULL_HANDLE) const
        {
            if (VkResult result = commandBuffer_presentation.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT))
                return result;

            graphicsBase::Base().CmdTransferImageOwnership(commandBuffer_presentation);

            if (VkResult result = commandBuffer_presentation.End())
                return result;

            return graphicsBase::Base().SubmitCommandBuffer_Presentation(
                commandBuffer_presentation,
                semaphore_renderingIsOver,
                semaphore_ownershipIsTransfered,
                fence);
        }
    };

    inline graphicsBasePlus graphicsBasePlus::singleton;

    // 便捷的全局格式特性查询入口。
    inline const VkFormatProperties& FormatProperties(VkFormat format)
    {
        return graphicsBase::Plus().FormatProperties(format);
    }
}
