#define STB_IMAGE_IMPLEMENTATION
#include "GlfwGeneral.hpp"
#include "VKBase+.h"

using namespace vulkan;

// 动态渲染这一章不再依赖 render pass 和 framebuffer，
// 但窗口尺寸仍然要反复拿来配置视口、裁剪区域和渲染区域。
const VkExtent2D& windowSize = graphicsBase::Base().SwapchainCreateInfo().imageExtent;

// 继续沿用前面几章已经搭好的三角形示例。
// 这里只把“如何开始一段渲染”切换到 dynamic rendering。
pipelineLayout pipelineLayout_triangle;

// 图形管线对象继续复用既有封装。
pipeline pipeline_triangle;

void CreateLayout()
{
    // 本节依然没有描述符集和 push constant，
    // 所以继续创建一个空的管线布局即可。
    pipelineLayout_triangle.Create();
}

void CreatePipeline()
{
    // 这里仍然保留上一章的几何着色器示例内容，
    // 因此先检查 GPU 是否支持 geometry shader。
    VkPhysicalDeviceFeatures physicalDeviceFeatures{};
    vkGetPhysicalDeviceFeatures(graphicsBase::Base().PhysicalDevice(), &physicalDeviceFeatures);

    // 若不支持几何着色器，这个示例就无法继续运行。
    if (!physicalDeviceFeatures.geometryShader)
    {
        outStream << "[ main ] ERROR\nCurrent GPU does not support geometry shader feature.\n";
        abort();
    }

    // 顶点着色器模块。
    static shaderModule vert("shader/FirstTriangle.vert.spv");

    // 几何着色器模块。
    static shaderModule geom("shader/FirstTriangle.geom.spv");

    // 片段着色器模块。
    static shaderModule frag("shader/FirstTriangle.frag.spv");

    // 当前图形管线仍然由顶点、几何、片段三个阶段组成。
    static VkPipelineShaderStageCreateInfo shaderStageCreateInfos_triangle[3] = {
        vert.StageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT),
        geom.StageCreateInfo(VK_SHADER_STAGE_GEOMETRY_BIT),
        frag.StageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT)};

    auto Create = [] {
        // 动态渲染路径下，图形管线不再绑定 render pass，
        // 改为通过 VkPipelineRenderingCreateInfo 描述未来会输出到哪些附件格式。
        const VkFormat colorAttachmentFormat = graphicsBase::Base().SwapchainCreateInfo().imageFormat;

        VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &colorAttachmentFormat};

        // 继续使用现有的图形管线创建信息打包器。
        graphicsPipelineCreateInfoPack pipelineCiPack;

        // 把动态渲染专用的创建信息挂到图形管线创建结构的 pNext 链上。
        pipelineCiPack.createInfo.pNext = &pipelineRenderingCreateInfo;

        // 指定管线布局。
        pipelineCiPack.SetPipelineLayout(pipelineLayout_triangle);

        // 输入图元仍然解释为三角形列表。
        pipelineCiPack.inputAssemblyStateCi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // 视口覆盖整个交换链图像。
        pipelineCiPack.viewports.emplace_back(
            0.0f,
            0.0f,
            static_cast<float>(windowSize.width),
            static_cast<float>(windowSize.height),
            0.0f,
            1.0f);

        // 裁剪矩形同样覆盖整个窗口。
        pipelineCiPack.scissors.emplace_back(VkOffset2D{}, windowSize);

        // 本节依旧不开启多重采样。
        pipelineCiPack.multisampleStateCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // 颜色附件继续写出 RGBA 四个分量。
        pipelineCiPack.colorBlendAttachmentStates.push_back({.colorWriteMask = 0b1111});

        // 同步内部数组指针与数量。
        pipelineCiPack.UpdateAllArrays();

        // 把三段着色器阶段信息挂进去。
        pipelineCiPack.SetShaderStages(shaderStageCreateInfos_triangle);

        // 创建真正的 Vulkan 图形管线。
        pipeline_triangle.Create(pipelineCiPack);
    };

    auto Destroy = [] {
        // 交换链重建前先销毁旧管线，避免持有旧尺寸和旧格式对应的状态。
        pipeline_triangle.~pipeline();
    };

    // 交换链重建后重新创建图形管线。
    graphicsBase::Base().AddCallback_CreateSwapchain(Create);

    // 交换链销毁前销毁旧图形管线。
    graphicsBase::Base().AddCallback_DestroySwapchain(Destroy);

    // 首次启动时先创建一次。
    Create();
}

int main()
{
    // Vulkan 1.3 已把 dynamic rendering 纳入核心。
    // 若只支持 Vulkan 1.2，则仍然可以通过 VK_KHR_dynamic_rendering 扩展来使用。
    PFN_vkCmdBeginRenderingKHR vkCmdBeginRendering = ::vkCmdBeginRendering;
    PFN_vkCmdEndRenderingKHR vkCmdEndRendering = ::vkCmdEndRendering;

    // 先把 API 版本提升到驱动可支持的最高版本。
    graphicsBase::Base().UseLatestApiVersion();

    // Vulkan 1.1 及以下不具备这一章需要的基础能力。
    if (graphicsBase::Base().ApiVersion() < VK_API_VERSION_1_2)
        return -1;

    if (graphicsBase::Base().ApiVersion() < VK_API_VERSION_1_3)
    {
        // Vulkan 1.2 路径下，需要手动启用动态渲染扩展。
        graphicsBase::Base().AddDeviceExtension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);

        // 再通过 pNext 链显式请求 dynamicRendering 特性。
        VkPhysicalDeviceDynamicRenderingFeatures physicalDeviceDynamicRenderingFeatures = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
        };
        graphicsBase::Base().AddNextStructure_PhysicalDeviceFeatures(physicalDeviceDynamicRenderingFeatures);

        // 初始化窗口、实例、设备和交换链。
        // 初始化成功后，还要确认驱动确实开启了 dynamicRendering。
        if (!InitializeWindow(defaultWindowSize) ||
            !physicalDeviceDynamicRenderingFeatures.dynamicRendering)
            return -1;

        // Vulkan 1.2 扩展路径下，命令入口点需要在创建设备后手动查询。
        vkCmdBeginRendering = reinterpret_cast<PFN_vkCmdBeginRenderingKHR>(
            vkGetDeviceProcAddr(graphicsBase::Base().Device(), "vkCmdBeginRenderingKHR"));
        vkCmdEndRendering = reinterpret_cast<PFN_vkCmdEndRenderingKHR>(
            vkGetDeviceProcAddr(graphicsBase::Base().Device(), "vkCmdEndRenderingKHR"));

        // 如果函数指针没取到，就没法继续录制动态渲染命令。
        if (!vkCmdBeginRendering || !vkCmdEndRendering)
            return -1;
    }
    else
    {
        // Vulkan 1.3 及以上可以直接从核心特性结构里检查 dynamicRendering。
        if (!InitializeWindow(defaultWindowSize) ||
            !graphicsBase::Base().PhysicalDeviceVulkan13Features().dynamicRendering)
            return -1;
    }

    // 创建管线布局。
    CreateLayout();

    // 创建图形管线。
    CreatePipeline();

    // 继续使用“初始为已完成”的栅栏，保证第一帧不会卡在等待上。
    fence fence(VK_FENCE_CREATE_SIGNALED_BIT);

    // 获取交换链图像成功后，呈现引擎会置位这个信号量。
    semaphore semaphore_imageIsAvailable;

    // 图形队列执行完命令缓冲区后，会置位这个信号量。
    semaphore semaphore_renderingIsOver;

    // 本节仍然只使用一个主命令缓冲区。
    commandBuffer commandBuffer;

    // 命令池来自图形队列族，并允许逐帧 reset 命令缓冲区。
    commandPool commandPool(
        graphicsBase::Base().QueueFamilyIndex_Graphics(),
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    // 申请一个主命令缓冲区对象。
    commandPool.AllocateBuffers(commandBuffer);

    // 继续把清屏颜色设成红色。
    VkClearValue clearColor = {.color = {1.0f, 0.0f, 0.0f, 1.0f}};

    while (!glfwWindowShouldClose(pWindow))
    {
        // 如果窗口被最小化，就先等待窗口恢复，避免无意义地持续渲染。
        while (glfwGetWindowAttrib(pWindow, GLFW_ICONIFIED))
            glfwWaitEvents();

        // 等待上一帧 GPU 工作结束，并把栅栏复位。
        fence.WaitAndReset();

        // 从交换链里取出当前这一帧要渲染的图像。
        graphicsBase::Base().SwapImage(semaphore_imageIsAvailable);

        // 记住当前交换链图像的索引。
        const uint32_t i = graphicsBase::Base().CurrentImageIndex();

        // 开始录制这一帧的主命令缓冲区。
        commandBuffer.BeginOneTime();

        // 动态渲染不会帮我们隐式完成 render pass 那套布局转换，
        // 所以在真正开始渲染前，要手动把交换链图像切到颜色附件布局。
        VkImageMemoryBarrier imageMemoryBarrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = graphicsBase::Base().SwapchainImage(i),
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

        // 由于本帧会直接清屏重写整张图像，所以 oldLayout 保持默认的 UNDEFINED 也没问题。
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_DEPENDENCY_BY_REGION_BIT,
            0, nullptr,
            0, nullptr,
            1, &imageMemoryBarrier);

        // 动态渲染开始前，需要把颜色附件的信息打包到 VkRenderingAttachmentInfo 里。
        VkRenderingAttachmentInfo colorAttachmentInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = graphicsBase::Base().SwapchainImageView(i),
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = clearColor};

        // 再用 VkRenderingInfo 描述这次渲染区域、层数以及附件数组。
        VkRenderingInfo renderingInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = {{}, windowSize},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentInfo};

        // 开始动态渲染。
        vkCmdBeginRendering(commandBuffer, &renderingInfo);

        // 绑定这一章使用的图形管线。
        pipeline_triangle.CmdBind(commandBuffer);

        // 继续提交 3 个顶点，交给顶点着色器和几何着色器去生成图元。
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);

        // 结束动态渲染。
        vkCmdEndRendering(commandBuffer);

        // 渲染结束后，还要把图像布局切回 PRESENT_SRC_KHR，方便后面的呈现队列使用。
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = 0;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            VK_DEPENDENCY_BY_REGION_BIT,
            0, nullptr,
            0, nullptr,
            1, &imageMemoryBarrier);

        // 结束命令录制。
        commandBuffer.End();

        // 提交命令时等待“图像可用”信号量，完成后发出“渲染结束”信号量，并关联栅栏。
        graphicsBase::Base().SubmitCommandBuffer_Graphics(
            commandBuffer,
            semaphore_imageIsAvailable,
            semaphore_renderingIsOver,
            fence);

        // 呈现阶段等待“渲染结束”信号量，确保展示的是已经写好的图像。
        graphicsBase::Base().PresentImage(semaphore_renderingIsOver);

        // 处理窗口事件。
        glfwPollEvents();

        // 刷新标题栏中的 FPS 显示。
        TitleFps();
    }

    // 退出前统一释放窗口和 Vulkan 资源。
    TerminateWindow();
    return 0;
}
