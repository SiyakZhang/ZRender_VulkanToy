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

    // Buffer
    class stagingBuffer
    {
        // 主线程专用的暂存缓冲区单例。
        static inline class
        {
            // 这里保存真正的暂存缓冲区对象指针。
            stagingBuffer* pointer = Create();

            stagingBuffer* Create()
            {
                // 用静态对象承载主线程公用的 staging buffer。
                static stagingBuffer stagingBuffer;

                // 逻辑设备销毁时，主动释放其中持有的 Vulkan 资源。
                graphicsBase::Base().AddCallback_DestroyDevice([] { stagingBuffer.~stagingBuffer(); });
                return &stagingBuffer;
            }

        public:
            // 对外只暴露引用访问。
            stagingBuffer& Get() const
            {
                return *pointer;
            }
        } stagingBuffer_mainThread;

    protected:
        // 实际承载数据的“缓冲区 + 设备内存”组合。
        bufferMemory bufferMemory;

        // 记录当前一次映射正在使用多少字节，取消映射时会用到。
        VkDeviceSize memoryUsage = 0;

        // 某些后续操作会把 staging buffer 临时别名成线性图像。
        image aliasedImage;

    public:
        stagingBuffer() = default;

        stagingBuffer(VkDeviceSize size)
        {
            Expand(size);
        }

        // Getter
        operator VkBuffer() const
        {
            return bufferMemory.Buffer();
        }

        const VkBuffer* Address() const
        {
            return bufferMemory.AddressOfBuffer();
        }

        VkDeviceSize AllocationSize() const
        {
            return bufferMemory.AllocationSize();
        }

        VkImage AliasedImage() const
        {
            return aliasedImage;
        }

        // Const Function
        void RetrieveData(void* pData_dst, VkDeviceSize size) const
        {
            // staging buffer 本来就是 CPU 可见的，所以可以直接拷回主存。
            bufferMemory.RetrieveData(pData_dst, size);
        }

        // Non-const Function
        void Expand(VkDeviceSize size)
        {
            // 若现有容量已经够用，就不重复分配。
            if (size <= AllocationSize())
                return;

            // 不够用时直接整块重建，逻辑最简单，也符合教程当前阶段需求。
            Release();

            VkBufferCreateInfo bufferCreateInfo = {
                .size = size,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT};

            // staging buffer 需要 CPU 直接访问，因此至少要求 HOST_VISIBLE。
            bufferMemory.Create(bufferCreateInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        }

        void Release()
        {
            // 先销毁可能复用了这块内存的别名图像。
            aliasedImage.~image();

            // 再释放底层 buffer + memory。
            bufferMemory.~bufferMemory();

            // 当前没有处于映射中的数据范围了。
            memoryUsage = 0;
        }

        void* MapMemory(VkDeviceSize size)
        {
            // 映射前先确保 staging buffer 够大。
            Expand(size);

            void* pData_dst = nullptr;
            bufferMemory.MapMemory(pData_dst, size);

            // 记住这次映射的有效长度，方便后续成对取消映射。
            memoryUsage = size;
            return pData_dst;
        }

        void UnmapMemory()
        {
            // 按最近一次映射的大小取消映射。
            bufferMemory.UnmapMemory(memoryUsage);
            memoryUsage = 0;
        }

        void BufferData(const void* pData_src, VkDeviceSize size)
        {
            // 直接把 CPU 数据写到 staging buffer 中。
            Expand(size);
            bufferMemory.BufferData(pData_src, size);
        }

        [[nodiscard]]
        VkImage AliasedImage2d(VkFormat format, VkExtent2D extent)
        {
            // 若这个格式不支持作为线性图像的 blit 源，就没法走这条路径。
            if (!(graphicsBase::Plus().FormatProperties(format).linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT))
                return VK_NULL_HANDLE;

            // 若格式信息里拿不到稳定的像素字节数，也没法继续做别名。
            const formatInfo formatDetails = FormatInfo(format);
            if (!formatDetails.sizePerPixel)
                return VK_NULL_HANDLE;

            // 计算一整张 2D 图像需要占用多少字节。
            const VkDeviceSize imageDataSize = VkDeviceSize(formatDetails.sizePerPixel) * extent.width * extent.height;

            // staging buffer 自己的容量不够，就不能别名成图像。
            if (imageDataSize > AllocationSize())
                return VK_NULL_HANDLE;

            // 进一步询问驱动：这种格式 / tiling / usage 的 2D 图像到底允不允许创建。
            VkImageFormatProperties imageFormatProperties{};
            if (vkGetPhysicalDeviceImageFormatProperties(
                    graphicsBase::Base().PhysicalDevice(),
                    format,
                    VK_IMAGE_TYPE_2D,
                    VK_IMAGE_TILING_LINEAR,
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                    0,
                    &imageFormatProperties))
                return VK_NULL_HANDLE;

            // 尺寸或资源总量超限时同样直接返回失败。
            if (extent.width > imageFormatProperties.maxExtent.width ||
                extent.height > imageFormatProperties.maxExtent.height ||
                imageDataSize > imageFormatProperties.maxResourceSize)
                return VK_NULL_HANDLE;

            VkImageCreateInfo imageCreateInfo = {
                .imageType = VK_IMAGE_TYPE_2D,
                .format = format,
                .extent = {extent.width, extent.height, 1},
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_LINEAR,
                .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED};

            // 每次重新请求别名图像前，都先把旧图像句柄释放掉。
            aliasedImage.~image();
            if (aliasedImage.Create(imageCreateInfo))
                return VK_NULL_HANDLE;

            // 检查图像的子资源布局是否与我们期望的“紧凑线性排布”一致。
            VkImageSubresource subResource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0};
            VkSubresourceLayout subresourceLayout{};
            vkGetImageSubresourceLayout(graphicsBase::Base().Device(), aliasedImage, &subResource, &subresourceLayout);

            // 教程这一版只接受没有额外 padding 的最简单布局。
            if (subresourceLayout.size != imageDataSize)
            {
                aliasedImage.~image();
                return VK_NULL_HANDLE;
            }

            // 让这个线性图像和 staging buffer 共享同一块设备内存。
            if (aliasedImage.BindMemory(bufferMemory.Memory()))
            {
                aliasedImage.~image();
                return VK_NULL_HANDLE;
            }

            return aliasedImage;
        }

        // Static Function
        static VkBuffer Buffer_MainThread()
        {
            return stagingBuffer_mainThread.Get();
        }

        static void Expand_MainThread(VkDeviceSize size)
        {
            stagingBuffer_mainThread.Get().Expand(size);
        }

        static void Release_MainThread()
        {
            stagingBuffer_mainThread.Get().Release();
        }

        static void* MapMemory_MainThread(VkDeviceSize size)
        {
            return stagingBuffer_mainThread.Get().MapMemory(size);
        }

        static void UnmapMemory_MainThread()
        {
            stagingBuffer_mainThread.Get().UnmapMemory();
        }

        static void BufferData_MainThread(const void* pData_src, VkDeviceSize size)
        {
            stagingBuffer_mainThread.Get().BufferData(pData_src, size);
        }

        static void RetrieveData_MainThread(void* pData_dst, VkDeviceSize size)
        {
            stagingBuffer_mainThread.Get().RetrieveData(pData_dst, size);
        }

        [[nodiscard]]
        static VkImage AliasedImage2d_MainThread(VkFormat format, VkExtent2D extent)
        {
            return stagingBuffer_mainThread.Get().AliasedImage2d(format, extent);
        }
    };

    class deviceLocalBuffer
    {
    protected:
        // 实际资源仍然交给 bufferMemory 承载。
        bufferMemory bufferMemory;

    public:
        deviceLocalBuffer() = default;

        deviceLocalBuffer(VkDeviceSize size, VkBufferUsageFlags desiredUsages_Without_transfer_dst)
        {
            Create(size, desiredUsages_Without_transfer_dst);
        }

        // Getter
        operator VkBuffer() const
        {
            return bufferMemory.Buffer();
        }

        const VkBuffer* Address() const
        {
            return bufferMemory.AddressOfBuffer();
        }

        VkDeviceSize AllocationSize() const
        {
            return bufferMemory.AllocationSize();
        }

        // Const Function
        void TransferData(const void* pData_src, VkDeviceSize size, VkDeviceSize offset = 0) const
        {
            // 若这块 buffer 的内存本身对 CPU 可见，就直接写，避免额外 copy。
            if (bufferMemory.MemoryProperties() & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
            {
                bufferMemory.BufferData(pData_src, size, offset);
                return;
            }

            // 否则先把数据写到 staging buffer，再走一次 buffer copy。
            stagingBuffer::BufferData_MainThread(pData_src, size);

            auto& commandBuffer = graphicsBase::Plus().CommandBuffer_Transfer();
            commandBuffer.BeginOneTime();

            VkBufferCopy region = {
                .srcOffset = 0,
                .dstOffset = offset,
                .size = size};
            vkCmdCopyBuffer(commandBuffer, stagingBuffer::Buffer_MainThread(), bufferMemory.Buffer(), 1, &region);

            commandBuffer.End();
            graphicsBase::Plus().ExecuteCommandBuffer_Graphics(commandBuffer);
        }

        void TransferData(
            const void* pData_src,
            uint32_t elementCount,
            VkDeviceSize elementSize,
            VkDeviceSize stride_src,
            VkDeviceSize stride_dst,
            VkDeviceSize offset = 0) const
        {
            // 这种重载专门处理“源 / 目标步长不一致”的情形。
            if (bufferMemory.MemoryProperties() & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
            {
                void* pData_dst = nullptr;
                bufferMemory.MapMemory(pData_dst, stride_dst * elementCount, offset);

                for (size_t i = 0; i < elementCount; ++i)
                {
                    memcpy(
                        static_cast<uint8_t*>(pData_dst) + stride_dst * i,
                        static_cast<const uint8_t*>(pData_src) + stride_src * i,
                        static_cast<size_t>(elementSize));
                }

                bufferMemory.UnmapMemory(stride_dst * elementCount, offset);
                return;
            }

            // 纯设备本地内存时，先把源数据整体写到 staging buffer。
            stagingBuffer::BufferData_MainThread(pData_src, stride_src * elementCount);

            auto& commandBuffer = graphicsBase::Plus().CommandBuffer_Transfer();
            commandBuffer.BeginOneTime();

            // 为每个元素都准备一段独立的 copy region。
            std::unique_ptr<VkBufferCopy[]> regions = std::make_unique<VkBufferCopy[]>(elementCount);
            for (size_t i = 0; i < elementCount; ++i)
            {
                regions[i] = {
                    .srcOffset = stride_src * i,
                    .dstOffset = stride_dst * i + offset,
                    .size = elementSize};
            }

            vkCmdCopyBuffer(
                commandBuffer,
                stagingBuffer::Buffer_MainThread(),
                bufferMemory.Buffer(),
                elementCount,
                regions.get());

            commandBuffer.End();
            graphicsBase::Plus().ExecuteCommandBuffer_Graphics(commandBuffer);
        }

        void TransferData(const auto& data_src) const
        {
            TransferData(&data_src, sizeof data_src);
        }

        void CmdUpdateBuffer(
            VkCommandBuffer commandBuffer,
            const void* pData_src,
            VkDeviceSize size_Limited_to_65536,
            VkDeviceSize offset = 0) const
        {
            // vkCmdUpdateBuffer 适合少量常量数据直接塞进命令缓冲区。
            vkCmdUpdateBuffer(commandBuffer, bufferMemory.Buffer(), offset, size_Limited_to_65536, pData_src);
        }

        void CmdUpdateBuffer(VkCommandBuffer commandBuffer, const auto& data_src) const
        {
            vkCmdUpdateBuffer(commandBuffer, bufferMemory.Buffer(), 0, sizeof data_src, &data_src);
        }

        // Non-const Function
        void Create(VkDeviceSize size, VkBufferUsageFlags desiredUsages_Without_transfer_dst)
        {
            VkBufferCreateInfo bufferCreateInfo = {
                .size = size,
                .usage = desiredUsages_Without_transfer_dst | VK_BUFFER_USAGE_TRANSFER_DST_BIT};

            // 先创建 VkBuffer 本体。
            if (bufferMemory.CreateBuffer(bufferCreateInfo))
                return;

            // 优先尝试“设备本地 + CPU 可见”的内存类型，方便直接更新。
            if (VkResult result = bufferMemory.AllocateMemory(
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
            {
                // 若不存在这种更理想的内存类型，就退回纯 device-local。
                if (bufferMemory.AllocateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
                    return;
            }

            // 把分配到的内存绑定给 buffer。
            bufferMemory.BindMemory();
        }

        void Recreate(VkDeviceSize size, VkBufferUsageFlags desiredUsages_Without_transfer_dst)
        {
            // 重建前先等设备空闲，避免 GPU 还在使用旧 buffer。
            graphicsBase::Base().WaitIdle();
            bufferMemory.~bufferMemory();
            Create(size, desiredUsages_Without_transfer_dst);
        }
    };

    class vertexBuffer : public deviceLocalBuffer
    {
    public:
        vertexBuffer() = default;

        vertexBuffer(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0)
            : deviceLocalBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | otherUsages)
        {
        }

        // Non-const Function
        void Create(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0)
        {
            deviceLocalBuffer::Create(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | otherUsages);
        }

        void Recreate(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0)
        {
            deviceLocalBuffer::Recreate(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | otherUsages);
        }
    };

    class indexBuffer : public deviceLocalBuffer
    {
    public:
        indexBuffer() = default;

        indexBuffer(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0)
            : deviceLocalBuffer(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | otherUsages)
        {
        }

        // Non-const Function
        void Create(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0)
        {
            deviceLocalBuffer::Create(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | otherUsages);
        }

        void Recreate(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0)
        {
            deviceLocalBuffer::Recreate(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | otherUsages);
        }
    };

    class uniformBuffer : public deviceLocalBuffer
    {
    public:
        uniformBuffer() = default;

        uniformBuffer(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0)
            : deviceLocalBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | otherUsages)
        {
        }

        // Non-const Function
        void Create(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0)
        {
            deviceLocalBuffer::Create(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | otherUsages);
        }

        void Recreate(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0)
        {
            deviceLocalBuffer::Recreate(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | otherUsages);
        }

        // Static Function
        static VkDeviceSize CalculateAlignedSize(VkDeviceSize dataSize)
        {
            // 动态 uniform buffer 的每个元素都必须按设备要求的对齐粒度排布。
            const VkDeviceSize alignment = graphicsBase::Base().PhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment;
            if (!alignment)
                return dataSize;
            return (dataSize + alignment - 1) & ~(alignment - 1);
        }
    };

    class storageBuffer : public deviceLocalBuffer
    {
    public:
        storageBuffer() = default;

        storageBuffer(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0)
            : deviceLocalBuffer(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | otherUsages)
        {
        }

        // Non-const Function
        void Create(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0)
        {
            deviceLocalBuffer::Create(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | otherUsages);
        }

        void Recreate(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0)
        {
            deviceLocalBuffer::Recreate(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | otherUsages);
        }

        // Static Function
        static VkDeviceSize CalculateAlignedSize(VkDeviceSize dataSize)
        {
            // 动态 storage buffer 也有自己的最小对齐要求。
            const VkDeviceSize alignment = graphicsBase::Base().PhysicalDeviceProperties().limits.minStorageBufferOffsetAlignment;
            if (!alignment)
                return dataSize;
            return (dataSize + alignment - 1) & ~(alignment - 1);
        }
    };

    // 便捷的全局格式特性查询入口。
    inline const VkFormatProperties& FormatProperties(VkFormat format)
    {
        return graphicsBase::Plus().FormatProperties(format);
    }
}
