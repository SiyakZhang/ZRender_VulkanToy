#define STB_IMAGE_IMPLEMENTATION
#include "GlfwGeneral.hpp"
#include "RPFB_Screen.hpp"
#include "VKBase+.h"

using namespace vulkan;

// 这一章继续沿用前面几章已经搭好的三角形示例。
// 这里先保留原来的“几何着色器版本”内容，只把帧缓冲相关流程切换到 imageless framebuffer。
pipelineLayout pipelineLayout_triangle;

// 图形管线同样沿用前面的封装对象。
pipeline pipeline_triangle;

const easyVulkan::renderPassWithFramebuffer& RenderPassAndFramebuffers()
{
    // 改为返回“单个渲染通道 + 单个无图像帧缓冲”的组合。
    static const auto& rpwf = easyVulkan::CreateRpwf_Screen_ImagelessFramebuffer();
    return rpwf;
}

void CreateLayout()
{
    // 本节依然没有描述符集和 push constant，
    // 所以继续创建一个空的管线布局即可。
    pipelineLayout_triangle.Create();
}

void CreatePipeline()
{
    // 几何着色器仍然属于可选特性，所以先检查当前 GPU 是否支持。
    VkPhysicalDeviceFeatures physicalDeviceFeatures{};
    vkGetPhysicalDeviceFeatures(graphicsBase::Base().PhysicalDevice(), &physicalDeviceFeatures);

    // 若设备不支持几何着色器，这个示例就无法继续运行。
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

    // 当前图形管线由顶点、几何、片段三个阶段组成。
    static VkPipelineShaderStageCreateInfo shaderStageCreateInfos_triangle[3] = {
        vert.StageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT),
        geom.StageCreateInfo(VK_SHADER_STAGE_GEOMETRY_BIT),
        frag.StageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT)};

    auto Create = [] {
        // 继续使用教程前面封装好的图形管线创建信息打包器。
        graphicsPipelineCreateInfoPack pipelineCiPack;

        // 指定这条管线使用的管线布局。
        pipelineCiPack.SetPipelineLayout(pipelineLayout_triangle);

        // 指定这条管线对应的 render pass。
        // 即使 framebuffer 变成 imageless，管线依然需要绑定具体的 render pass 兼容信息。
        pipelineCiPack.SetRenderPass(RenderPassAndFramebuffers().renderPass);

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

        // 颜色附件照常写出 RGBA 四个分量。
        pipelineCiPack.colorBlendAttachmentStates.push_back({.colorWriteMask = 0b1111});

        // 同步内部数组指针与数量。
        pipelineCiPack.UpdateAllArrays();

        // 把三段着色器阶段信息挂进去。
        pipelineCiPack.SetShaderStages(shaderStageCreateInfos_triangle);

        // 创建真正的 Vulkan 图形管线。
        pipeline_triangle.Create(pipelineCiPack);
    };

    auto Destroy = [] {
        // 交换链重建前先销毁旧管线，避免持有旧尺寸相关状态。
        pipeline_triangle.~pipeline();
    };

    // 交换链重建后重新创建图形管线。
    graphicsBase::Base().AddCallback_CreateSwapchain(Create);

    // 交换链销毁前销毁旧管线。
    graphicsBase::Base().AddCallback_DestroySwapchain(Destroy);

    // 首次启动时先创建一次。
    Create();
}

int main()
{
    // 这一章需要 imageless framebuffer。
    // Vulkan 1.2 起它已经进入核心；在 Vulkan 1.1 上则需要额外启用扩展和特性结构体。
    graphicsBase::Base().UseLatestApiVersion();

    // Vulkan 1.0 没有这一套能力，直接结束即可。
    if (graphicsBase::Base().ApiVersion() < VK_API_VERSION_1_1)
        return -1;

    if (graphicsBase::Base().ApiVersion() < VK_API_VERSION_1_2)
    {
        // Vulkan 1.1 路径下需要手动启用两个相关扩展。
        graphicsBase::Base().AddDeviceExtension(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME);
        graphicsBase::Base().AddDeviceExtension(VK_KHR_IMAGELESS_FRAMEBUFFER_EXTENSION_NAME);

        // 这个特性结构体会通过 Ch6-0 新加好的 pNext 接口挂到 features 链上。
        VkPhysicalDeviceImagelessFramebufferFeatures physicalDeviceImagelessFramebufferFeatures = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES,
        };
        graphicsBase::Base().AddNextStructure_PhysicalDeviceFeatures(physicalDeviceImagelessFramebufferFeatures);

        // 初始化窗口、实例、设备和交换链。
        // 初始化完成后，再检查驱动是否真的把 imagelessFramebuffer 特性打开了。
        if (!InitializeWindow(defaultWindowSize) ||
            !physicalDeviceImagelessFramebufferFeatures.imagelessFramebuffer)
            return -1;
    }
    else
    {
        // Vulkan 1.2 及以上直接从核心特性结构里读取 imagelessFramebuffer 即可。
        if (!InitializeWindow(defaultWindowSize) ||
            !graphicsBase::Base().PhysicalDeviceVulkan12Features().imagelessFramebuffer)
            return -1;
    }

    // 拿到屏幕渲染通道，以及唯一那一个 imageless framebuffer。
    const auto& [renderPass, framebuffer] = RenderPassAndFramebuffers();

    // 创建管线布局。
    CreateLayout();

    // 创建图形管线。
    CreatePipeline();

    // 继续使用“初始为已完成”的栅栏，保证第一帧不会在等待上卡住。
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

        // 等待上一帧的 GPU 工作完成，并把栅栏复位。
        fence.WaitAndReset();

        // 从交换链里取出当前这一帧要渲染的图像。
        graphicsBase::Base().SwapImage(semaphore_imageIsAvailable);

        // 记录当前交换链图像的索引。
        const uint32_t i = graphicsBase::Base().CurrentImageIndex();

        // 开始录制这一帧的主命令缓冲区。
        commandBuffer.BeginOneTime();

        // imageless framebuffer 不在创建时绑定附件，
        // 所以这里要把“当前这张 swapchain image view”临时挂进 VkRenderPassBeginInfo 的 pNext 链上。
        VkImageView attachment = graphicsBase::Base().SwapchainImageView(i);

        // 这个结构专门用来在开始 render pass 时补充真正的附件视图。
        VkRenderPassAttachmentBeginInfo renderPassAttachmentBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO,
            .attachmentCount = 1,
            .pAttachments = &attachment};

        // 这里的 framebuffer 不再区分第几张交换链图像，
        // 因为所有图像共用同一个 imageless framebuffer。
        VkRenderPassBeginInfo renderPassBeginInfo = {
            .pNext = &renderPassAttachmentBeginInfo,
            .framebuffer = framebuffer,
            .renderArea = {{}, windowSize},
            .clearValueCount = 1,
            .pClearValues = &clearColor};

        // 开始渲染通道。
        renderPass.CmdBegin(commandBuffer, renderPassBeginInfo);

        // 绑定这一章使用的图形管线。
        pipeline_triangle.CmdBind(commandBuffer);

        // 继续提交 3 个顶点，交给顶点着色器和几何着色器去生成图元。
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);

        // 结束渲染通道。
        renderPass.CmdEnd(commandBuffer);

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
