#pragma once

#include "VKBase.h"
#include "VKFormat.h"
#include "VulkanGraphicsPipelineBuilder.h"
#include "stb_image.h"

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

    // 图像传输辅助函数。
    struct imageOperation
    {
        struct imageMemoryBarrierParameterPack
        {
            // 这个小结构专门把“阶段 + 访问掩码 + 目标布局”打包，便于下面几个图像辅助函数复用。
            const bool isNeeded = false;
            const VkPipelineStageFlags stage = 0;
            const VkAccessFlags access = 0;
            const VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

            constexpr imageMemoryBarrierParameterPack() = default;

            constexpr imageMemoryBarrierParameterPack(
                VkPipelineStageFlags stage,
                VkAccessFlags access,
                VkImageLayout layout)
                : isNeeded(true), stage(stage), access(access), layout(layout)
            {
            }
        };

        static void CmdCopyBufferToImage(
            VkCommandBuffer commandBuffer,
            VkBuffer buffer,
            VkImage image,
            const VkBufferImageCopy& region,
            imageMemoryBarrierParameterPack imb_from,
            imageMemoryBarrierParameterPack imb_to)
        {
            // 先把目标子资源切到 transfer-dst 布局，准备接收 buffer 数据。
            VkImageMemoryBarrier imageMemoryBarrier = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                nullptr,
                imb_from.access,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                imb_from.layout,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                image,
                {
                    region.imageSubresource.aspectMask,
                    region.imageSubresource.mipLevel,
                    1,
                    region.imageSubresource.baseArrayLayer,
                    region.imageSubresource.layerCount}};

            // 只有调用方真的要求做前置转换时，才插入这道 barrier。
            if (imb_from.isNeeded)
            {
                vkCmdPipelineBarrier(
                    commandBuffer,
                    imb_from.stage,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    0,
                    0,
                    nullptr,
                    0,
                    nullptr,
                    1,
                    &imageMemoryBarrier);
            }

            // 把 staging buffer 里的像素数据拷贝进图像。
            vkCmdCopyBufferToImage(
                commandBuffer,
                buffer,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &region);

            // 如果后续还要继续被读或被采样，就在这里顺手切到目标布局。
            if (imb_to.isNeeded)
            {
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                imageMemoryBarrier.dstAccessMask = imb_to.access;
                imageMemoryBarrier.newLayout = imb_to.layout;

                vkCmdPipelineBarrier(
                    commandBuffer,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    imb_to.stage,
                    0,
                    0,
                    nullptr,
                    0,
                    nullptr,
                    1,
                    &imageMemoryBarrier);
            }
        }

        static void CmdBlitImage(
            VkCommandBuffer commandBuffer,
            VkImage image_src,
            VkImage image_dst,
            const VkImageBlit& region,
            imageMemoryBarrierParameterPack imb_dst_from,
            imageMemoryBarrierParameterPack imb_dst_to,
            VkFilter filter = VK_FILTER_LINEAR)
        {
            // blit 前只需要关心目标图像，因为源图像应当已经被调用方准备成 transfer-src 布局。
            VkImageMemoryBarrier imageMemoryBarrier = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                nullptr,
                imb_dst_from.access,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                imb_dst_from.layout,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                image_dst,
                {
                    region.dstSubresource.aspectMask,
                    region.dstSubresource.mipLevel,
                    1,
                    region.dstSubresource.baseArrayLayer,
                    region.dstSubresource.layerCount}};

            // 如有需要，先把目标子资源转成可以写入的布局。
            if (imb_dst_from.isNeeded)
            {
                vkCmdPipelineBarrier(
                    commandBuffer,
                    imb_dst_from.stage,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    0,
                    0,
                    nullptr,
                    0,
                    nullptr,
                    1,
                    &imageMemoryBarrier);
            }

            // 由 Vulkan 完成图像缩放 / 格式转换 / 各层拷贝。
            vkCmdBlitImage(
                commandBuffer,
                image_src,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image_dst,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &region,
                filter);

            // 如果目标接下来要继续被读，就立刻切过去，方便串联后续操作。
            if (imb_dst_to.isNeeded)
            {
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                imageMemoryBarrier.dstAccessMask = imb_dst_to.access;
                imageMemoryBarrier.newLayout = imb_dst_to.layout;

                vkCmdPipelineBarrier(
                    commandBuffer,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    imb_dst_to.stage,
                    0,
                    0,
                    nullptr,
                    0,
                    nullptr,
                    1,
                    &imageMemoryBarrier);
            }
        }

        static void CmdGenerateMipmap2d(
            VkCommandBuffer commandBuffer,
            VkImage image,
            VkExtent2D imageExtent,
            uint32_t mipLevelCount,
            uint32_t layerCount,
            imageMemoryBarrierParameterPack imb_to,
            VkFilter minFilter = VK_FILTER_LINEAR)
        {
            // 按教程要求，mip 尺寸每级右移一位，但最小不能小于 1x1。
            auto MipmapExtent = [](VkExtent2D imageExtent, uint32_t mipLevel) {
                VkOffset3D extent = {
                    int32_t(imageExtent.width >> mipLevel),
                    int32_t(imageExtent.height >> mipLevel),
                    1};
                extent.x += !extent.x;
                extent.y += !extent.y;
                return extent;
            };

            // 贴图数组在部分驱动上直接逐层循环更稳妥，这里沿用教程/解答里的兼容写法。
            if (layerCount > 1)
            {
                std::unique_ptr<VkImageBlit[]> regions = std::make_unique<VkImageBlit[]>(layerCount);

                for (uint32_t i = 1; i < mipLevelCount; ++i)
                {
                    VkOffset3D mipmapExtent_src = MipmapExtent(imageExtent, i - 1);
                    VkOffset3D mipmapExtent_dst = MipmapExtent(imageExtent, i);

                    // 同一级 mip 的所有数组层一起准备好，交给一次 vkCmdBlitImage。
                    for (uint32_t j = 0; j < layerCount; ++j)
                    {
                        regions[j] = {
                            {VK_IMAGE_ASPECT_COLOR_BIT, i - 1, j, 1},
                            {{}, mipmapExtent_src},
                            {VK_IMAGE_ASPECT_COLOR_BIT, i, j, 1},
                            {{}, mipmapExtent_dst}};
                    }

                    // 新的目标 mip 级别还没有数据，先把它们统一切成 transfer-dst。
                    VkImageMemoryBarrier imageMemoryBarrier = {
                        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                        nullptr,
                        0,
                        VK_ACCESS_TRANSFER_WRITE_BIT,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_QUEUE_FAMILY_IGNORED,
                        VK_QUEUE_FAMILY_IGNORED,
                        image,
                        {VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, layerCount}};

                    vkCmdPipelineBarrier(
                        commandBuffer,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        0,
                        0,
                        nullptr,
                        0,
                        nullptr,
                        1,
                        &imageMemoryBarrier);

                    // 由上一级 mip 同时缩小生成本级所有 layer。
                    vkCmdBlitImage(
                        commandBuffer,
                        image,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        image,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        layerCount,
                        regions.get(),
                        minFilter);

                    // 生成完的 mip 级要立刻转成 transfer-src，供下一轮继续向下生成。
                    imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                    imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

                    vkCmdPipelineBarrier(
                        commandBuffer,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        0,
                        0,
                        nullptr,
                        0,
                        nullptr,
                        1,
                        &imageMemoryBarrier);
                }
            }
            else
            {
                for (uint32_t i = 1; i < mipLevelCount; ++i)
                {
                    // 非数组 2D 贴图直接逐级从 i-1 blit 到 i。
                    VkImageBlit region = {
                        {VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 0, layerCount},
                        {{}, MipmapExtent(imageExtent, i - 1)},
                        {VK_IMAGE_ASPECT_COLOR_BIT, i, 0, layerCount},
                        {{}, MipmapExtent(imageExtent, i)}};

                    CmdBlitImage(
                        commandBuffer,
                        image,
                        image,
                        region,
                        {VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED},
                        {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL},
                        minFilter);
                }
            }

            // 最后把整张贴图统一切到调用方指定的布局，通常就是 shader-read-only。
            if (imb_to.isNeeded)
            {
                VkImageMemoryBarrier imageMemoryBarrier = {
                    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    nullptr,
                    0,
                    imb_to.access,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    imb_to.layout,
                    VK_QUEUE_FAMILY_IGNORED,
                    VK_QUEUE_FAMILY_IGNORED,
                    image,
                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevelCount, 0, layerCount}};

                vkCmdPipelineBarrier(
                    commandBuffer,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    imb_to.stage,
                    0,
                    0,
                    nullptr,
                    0,
                    nullptr,
                    1,
                    &imageMemoryBarrier);
            }
        }
    };

    class texture
    {
    protected:
        // 贴图对象最终都由“图像 + 图像视图”两部分对外服务。
        imageView imageView;
        imageMemory imageMemory;

        // stb_image 返回的内存必须用 stbi_image_free 释放，不能直接 delete[]。
        struct stbImageDeleter
        {
            void operator()(uint8_t* pImageData) const
            {
                stbi_image_free(pImageData);
            }
        };

        using imageData_t = std::unique_ptr<uint8_t, stbImageDeleter>;

        texture() = default;

        void CreateImageMemory(
            VkImageType imageType,
            VkFormat format,
            VkExtent3D extent,
            uint32_t mipLevelCount,
            uint32_t arrayLayerCount,
            VkImageCreateFlags flags = 0)
        {
            // 贴图需要被采样，也需要在上传阶段参与 copy/blit，因此三个 usage 都要带上。
            VkImageCreateInfo imageCreateInfo = {
                .flags = flags,
                .imageType = imageType,
                .format = format,
                .extent = extent,
                .mipLevels = mipLevelCount,
                .arrayLayers = arrayLayerCount,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT};

            // 贴图最终常驻设备本地内存，利于采样性能。
            imageMemory.Create(imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        }

        void CreateImageView(
            VkImageViewType viewType,
            VkFormat format,
            uint32_t mipLevelCount,
            uint32_t arrayLayerCount,
            VkImageViewCreateFlags flags = 0)
        {
            // 视图负责把整张图像的颜色子资源暴露给着色器使用。
            imageView.Create(
                imageMemory.Image(),
                viewType,
                format,
                {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevelCount, 0, arrayLayerCount},
                flags);
        }

        template<typename TAddress>
        [[nodiscard]]
        static imageData_t LoadFile_Internal(
            TAddress address,
            size_t fileSize,
            VkExtent2D& extent,
            formatInfo requiredFormatInfo)
        {
#ifndef NDEBUG
            // 当前教程阶段只处理常见的 8-bit / 16-bit 整数图像与 32-bit float HDR 图像。
            if (!(requiredFormatInfo.rawDataType == formatInfo::floatingPoint && requiredFormatInfo.sizePerComponent == 4) &&
                !(requiredFormatInfo.rawDataType == formatInfo::integer && requiredFormatInfo.sizePerComponent >= 1 && requiredFormatInfo.sizePerComponent <= 2))
            {
                outStream << "[ texture ] ERROR\nRequired format is not available for source image data!\n";
                abort();
            }
#endif

            // stb_image 的宽高输出类型是 int，这里先用局部变量接收，再写回 Vulkan 的 extent。
            int width = 0;
            int height = 0;
            int channelCount = 0;
            void* pImageData = nullptr;

            if constexpr (std::same_as<TAddress, const char*>)
            {
                // 从磁盘读取文件时，按“目标格式每分量字节数”选择 stb 对应的加载函数。
                if (requiredFormatInfo.rawDataType == formatInfo::integer)
                {
                    pImageData = requiredFormatInfo.sizePerComponent == 1
                        ? static_cast<void*>(stbi_load(address, &width, &height, &channelCount, requiredFormatInfo.componentCount))
                        : static_cast<void*>(stbi_load_16(address, &width, &height, &channelCount, requiredFormatInfo.componentCount));
                }
                else
                {
                    pImageData = static_cast<void*>(stbi_loadf(address, &width, &height, &channelCount, requiredFormatInfo.componentCount));
                }

                if (!pImageData)
                    outStream << std::format("[ texture ] ERROR\nFailed to load the file: {}\n", address);
            }
            else
            {
                // 从内存读取时，stb 的长度参数是 int，因此这里先做一次范围保护。
                if (fileSize > INT32_MAX)
                {
                    outStream << "[ texture ] ERROR\nFailed to load image data from the given address! Data size must be less than 2G!\n";
                    return {};
                }

                if (requiredFormatInfo.rawDataType == formatInfo::integer)
                {
                    pImageData = requiredFormatInfo.sizePerComponent == 1
                        ? static_cast<void*>(stbi_load_from_memory(address, int(fileSize), &width, &height, &channelCount, requiredFormatInfo.componentCount))
                        : static_cast<void*>(stbi_load_16_from_memory(address, int(fileSize), &width, &height, &channelCount, requiredFormatInfo.componentCount));
                }
                else
                {
                    pImageData = static_cast<void*>(stbi_loadf_from_memory(address, int(fileSize), &width, &height, &channelCount, requiredFormatInfo.componentCount));
                }

                if (!pImageData)
                    outStream << "[ texture ] ERROR\nFailed to load image data from the given address!\n";
            }

            // 成功读取后把尺寸写回给调用方，供后续创建 VkImage 使用。
            if (pImageData)
            {
                extent.width = uint32_t(width);
                extent.height = uint32_t(height);
            }

            // 这里统一转成 uint8_t* 保存，释放时仍然交回 stb_image_free。
            return imageData_t(static_cast<uint8_t*>(pImageData));
        }

    public:
        VkImageView ImageView() const
        {
            return imageView;
        }

        VkImage Image() const
        {
            return imageMemory.Image();
        }

        const VkImageView* AddressOfImageView() const
        {
            return imageView.Address();
        }

        const VkImage* AddressOfImage() const
        {
            return imageMemory.AddressOfImage();
        }

        VkDescriptorImageInfo DescriptorImageInfo(VkSampler sampler) const
        {
            // 采样器、图像视图、最终布局正是写 descriptor image info 所需的三元组。
            return {sampler, imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        }

        [[nodiscard]]
        static imageData_t LoadFile(const char* filepath, VkExtent2D& extent, formatInfo requiredFormatInfo)
        {
            return LoadFile_Internal(filepath, 0, extent, requiredFormatInfo);
        }

        [[nodiscard]]
        static imageData_t LoadFile(
            const uint8_t* fileBinaries,
            size_t fileSize,
            VkExtent2D& extent,
            formatInfo requiredFormatInfo)
        {
            return LoadFile_Internal(fileBinaries, fileSize, extent, requiredFormatInfo);
        }

        static uint32_t CalculateMipLevelCount(VkExtent2D extent)
        {
            // 完整 mip 链的总级数 = floor(log2(max(width, height))) + 1。
            return uint32_t(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1;
        }

        static void CopyBlitAndGenerateMipmap2d(
            VkBuffer buffer_copyFrom,
            VkImage image_copyTo,
            VkImage image_blitTo,
            VkExtent2D imageExtent,
            uint32_t mipLevelCount = 1,
            uint32_t layerCount = 1,
            VkFilter minFilter = VK_FILTER_LINEAR)
        {
            // 两种目标状态：直接进入 shader-read-only，或继续保持 transfer-src 供后续生成 mip。
            static constexpr imageOperation::imageMemoryBarrierParameterPack imbs[2] = {
                {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL}};

            const bool generateMipmap = mipLevelCount > 1;
            const bool blitMipLevel0 = image_copyTo != image_blitTo;

            // 上传贴图总要用到一次即时命令缓冲，所以这里统一走扩展层里的传输命令缓冲。
            auto& commandBuffer = graphicsBase::Plus().CommandBuffer_Transfer();
            commandBuffer.BeginOneTime();

            // buffer -> image 的拷贝区域只覆盖第 0 级 mip。
            VkBufferImageCopy region = {
                .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, layerCount},
                .imageExtent = {imageExtent.width, imageExtent.height, 1}};

            imageOperation::CmdCopyBufferToImage(
                commandBuffer,
                buffer_copyFrom,
                image_copyTo,
                region,
                {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED},
                imbs[generateMipmap || blitMipLevel0]);

            // 如果格式转换或中转图像存在，就把第 0 级 mip 再 blit 到最终图像。
            if (blitMipLevel0)
            {
                VkImageBlit blitRegion = {
                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, layerCount},
                    {{}, {int32_t(imageExtent.width), int32_t(imageExtent.height), 1}},
                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, layerCount},
                    {{}, {int32_t(imageExtent.width), int32_t(imageExtent.height), 1}}};

                imageOperation::CmdBlitImage(
                    commandBuffer,
                    image_copyTo,
                    image_blitTo,
                    blitRegion,
                    {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED},
                    imbs[generateMipmap],
                    minFilter);
            }

            // 需要 mip 的话，就从 image_blitTo 的第 0 级继续往下生成整条 mip 链。
            if (generateMipmap)
            {
                imageOperation::CmdGenerateMipmap2d(
                    commandBuffer,
                    image_blitTo,
                    imageExtent,
                    mipLevelCount,
                    layerCount,
                    {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                    minFilter);
            }

            commandBuffer.End();

            // 立即提交并等待完成，这样贴图创建函数返回时资源就已经可用了。
            graphicsBase::Plus().ExecuteCommandBuffer_Graphics(commandBuffer);
        }

        static void BlitAndGenerateMipmap2d(
            VkImage image_preinitialized,
            VkImage image_final,
            VkExtent2D imageExtent,
            uint32_t mipLevelCount = 1,
            uint32_t layerCount = 1,
            VkFilter minFilter = VK_FILTER_LINEAR)
        {
            // 与上面同理，这里也准备“最终采样”与“继续传输”两种目标状态。
            static constexpr imageOperation::imageMemoryBarrierParameterPack imbs[2] = {
                {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL}};

            const bool generateMipmap = mipLevelCount > 1;
            const bool blitMipLevel0 = image_preinitialized != image_final;

            // 只有真的要 blit 或生成 mip 时，才有必要录制命令。
            if (generateMipmap || blitMipLevel0)
            {
                auto& commandBuffer = graphicsBase::Plus().CommandBuffer_Transfer();
                commandBuffer.BeginOneTime();

                if (blitMipLevel0)
                {
                    // 线性 tiling 的别名图像初始是 PREINITIALIZED，先转成 transfer-src。
                    VkImageMemoryBarrier imageMemoryBarrier = {
                        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                        nullptr,
                        0,
                        VK_ACCESS_TRANSFER_READ_BIT,
                        VK_IMAGE_LAYOUT_PREINITIALIZED,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        VK_QUEUE_FAMILY_IGNORED,
                        VK_QUEUE_FAMILY_IGNORED,
                        image_preinitialized,
                        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, layerCount}};

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

                    // 第 0 级 mip 做一次等尺寸 blit，既可完成格式转换，也能把数据转到 optimal tiling 图像。
                    VkImageBlit region = {
                        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, layerCount},
                        {{}, {int32_t(imageExtent.width), int32_t(imageExtent.height), 1}},
                        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, layerCount},
                        {{}, {int32_t(imageExtent.width), int32_t(imageExtent.height), 1}}};

                    imageOperation::CmdBlitImage(
                        commandBuffer,
                        image_preinitialized,
                        image_final,
                        region,
                        {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED},
                        imbs[generateMipmap],
                        minFilter);
                }

                // 如果要求生成 mip，就在最终图像上继续往下 blit。
                if (generateMipmap)
                {
                    imageOperation::CmdGenerateMipmap2d(
                        commandBuffer,
                        image_final,
                        imageExtent,
                        mipLevelCount,
                        layerCount,
                        {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                        minFilter);
                }

                commandBuffer.End();
                graphicsBase::Plus().ExecuteCommandBuffer_Graphics(commandBuffer);
            }
        }

        static VkSamplerCreateInfo SamplerCreateInfo()
        {
            // 默认采样器按教程给成“线性过滤 + 线性 mip + 开启各向异性”。
            return {
                .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .magFilter = VK_FILTER_LINEAR,
                .minFilter = VK_FILTER_LINEAR,
                .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .mipLodBias = 0.0f,
                .anisotropyEnable = VK_TRUE,
                .maxAnisotropy = graphicsBase::Base().PhysicalDeviceProperties().limits.maxSamplerAnisotropy,
                .compareEnable = VK_FALSE,
                .compareOp = VK_COMPARE_OP_ALWAYS,
                .minLod = 0.0f,
                .maxLod = VK_LOD_CLAMP_NONE,
                .borderColor = {},
                .unnormalizedCoordinates = VK_FALSE};
        }
    };

    class texture2d : public texture
    {
    protected:
        // 2D 贴图只需要额外记住宽高即可。
        VkExtent2D extent = {};

        void Create_Internal(VkFormat format_initial, VkFormat format_final, bool generateMipmap)
        {
            // 若启用 mipmap，就先计算完整 mip 链的层数。
            const uint32_t mipLevelCount = generateMipmap ? CalculateMipLevelCount(extent) : 1;

            // 创建最终的 optimal tiling 贴图。
            CreateImageMemory(
                VK_IMAGE_TYPE_2D,
                format_final,
                {extent.width, extent.height, 1},
                mipLevelCount,
                1);

            // 再为整张图像创建一个普通 2D 视图。
            CreateImageView(VK_IMAGE_VIEW_TYPE_2D, format_final, mipLevelCount, 1);

            // 若原始像素格式已经等于目标格式，就直接从 staging buffer 上传。
            if (format_initial == format_final)
            {
                CopyBlitAndGenerateMipmap2d(
                    stagingBuffer::Buffer_MainThread(),
                    imageMemory.Image(),
                    imageMemory.Image(),
                    extent,
                    mipLevelCount,
                    1);
            }
            else if (VkImage image_conversion = stagingBuffer::AliasedImage2d_MainThread(format_initial, extent))
            {
                // 若 staging buffer 能别名成线性图像，就先别名，再从别名图像 blit 到最终贴图。
                BlitAndGenerateMipmap2d(
                    image_conversion,
                    imageMemory.Image(),
                    extent,
                    mipLevelCount,
                    1);
            }
            else
            {
                // 若无法直接别名，就额外创建一张中转图像，先 copy，再 blit 到最终贴图。
                VkImageCreateInfo imageCreateInfo = {
                    .imageType = VK_IMAGE_TYPE_2D,
                    .format = format_initial,
                    .extent = {extent.width, extent.height, 1},
                    .mipLevels = 1,
                    .arrayLayers = 1,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT};

                vulkan::imageMemory imageMemory_conversion(
                    imageCreateInfo,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

                CopyBlitAndGenerateMipmap2d(
                    stagingBuffer::Buffer_MainThread(),
                    imageMemory_conversion.Image(),
                    imageMemory.Image(),
                    extent,
                    mipLevelCount,
                    1);
            }
        }

    public:
        texture2d() = default;

        texture2d(
            const char* filepath,
            VkFormat format_initial,
            VkFormat format_final,
            bool generateMipmap = true)
        {
            Create(filepath, format_initial, format_final, generateMipmap);
        }

        texture2d(
            const uint8_t* pImageData,
            VkExtent2D extent,
            VkFormat format_initial,
            VkFormat format_final,
            bool generateMipmap = true)
        {
            Create(pImageData, extent, format_initial, format_final, generateMipmap);
        }

        VkExtent2D Extent() const
        {
            return extent;
        }

        uint32_t Width() const
        {
            return extent.width;
        }

        uint32_t Height() const
        {
            return extent.height;
        }

        void Create(
            const char* filepath,
            VkFormat format_initial,
            VkFormat format_final,
            bool generateMipmap = true)
        {
            // 从文件读取时，先根据源格式推导 stb 应该输出的通道布局与位宽。
            VkExtent2D extent = {};
            const formatInfo formatDetails = FormatInfo(format_initial);
            imageData_t pImageData = LoadFile(filepath, extent, formatDetails);

            // 成功读取后，再复用内存版 Create 继续后面的上传逻辑。
            if (pImageData)
                Create(pImageData.get(), extent, format_initial, format_final, generateMipmap);
        }

        void Create(
            const uint8_t* pImageData,
            VkExtent2D extent,
            VkFormat format_initial,
            VkFormat format_final,
            bool generateMipmap = true)
        {
            this->extent = extent;

            // staging buffer 里先放原始像素数据，后面的 copy / blit 都从它出发。
            const size_t imageDataSize = size_t(FormatInfo(format_initial).sizePerPixel) * extent.width * extent.height;
            stagingBuffer::BufferData_MainThread(pImageData, imageDataSize);

            // 然后创建最终图像、视图，并完成上传 / 转换 / 生成 mip。
            Create_Internal(format_initial, format_final, generateMipmap);
        }
    };

    // 便捷的全局格式特性查询入口。
    inline const VkFormatProperties& FormatProperties(VkFormat format)
    {
        return graphicsBase::Plus().FormatProperties(format);
    }
}
