#include "GlfwGeneral.hpp"
#include "RPFB_Screen.hpp"
#include "VulkanGraphicsPipelineBuilder.h"

using namespace vulkan;

// 三角形示例使用的管线布局。
pipelineLayout pipelineLayout_triangle;

// 三角形示例使用的图形管线。
pipeline pipeline_triangle;

const easyVulkan::renderPassWithFramebuffers& RenderPassAndFramebuffers()
{
    // 用一个静态引用统一拿到屏幕渲染通道与帧缓冲集合。
    static const auto& rpwf = easyVulkan::CreateRpwf_Screen();
    return rpwf;
}

void CreateLayout()
{
    // 本节还没有描述符和 push constant，所以直接创建一个“空管线布局”即可。
    pipelineLayout_triangle.Create();
}

void CreatePipeline()
{
    // 几何着色器属于可选设备特性，先确认当前物理设备是否支持。
    VkPhysicalDeviceFeatures physicalDeviceFeatures{};
    vkGetPhysicalDeviceFeatures(graphicsBase::Base().PhysicalDevice(), &physicalDeviceFeatures);

    // 如果不支持几何着色器，这一课对应的示例就无法继续运行。
    if (!physicalDeviceFeatures.geometryShader)
    {
        outStream << "[ main ] ERROR\nCurrent GPU does not support geometry shader feature.\n";
        abort();
    }

    // 顶点着色器的 SPIR-V 模组会在这里被读入并创建成 VkShaderModule。
    static shaderModule vert("shader/FirstTriangle.vert.spv");

    // 几何着色器位于顶点着色器之后、片段着色器之前，用来基于输入图元继续生成新图元。
    static shaderModule geom("shader/FirstTriangle.geom.spv");

    // 片段着色器的 SPIR-V 模组同理，会在创建管线阶段作为另一个着色器阶段使用。
    static shaderModule frag("shader/FirstTriangle.frag.spv");

    // 这里组装的是“管线着色器阶段创建信息”，它引用前面创建好的 shaderModule。
    static VkPipelineShaderStageCreateInfo shaderStageCreateInfos_triangle[3] = {
        vert.StageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT),
        geom.StageCreateInfo(VK_SHADER_STAGE_GEOMETRY_BIT),
        frag.StageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT)};

    auto Create = [] {
        // 先创建一个“图形管线创建信息包”，里面把常见状态都整理好了。
        graphicsPipelineCreateInfoPack pipelineCiPack;

        // 当前管线使用上面创建的空布局。
        pipelineCiPack.SetPipelineLayout(pipelineLayout_triangle);

        // 当前管线要在屏幕渲染通道里执行。
        pipelineCiPack.SetRenderPass(RenderPassAndFramebuffers().renderPass);

        // 三个顶点按三角形列表解释。
        pipelineCiPack.inputAssemblyStateCi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // 视口直接覆盖整个交换链图像范围。
        pipelineCiPack.viewports.emplace_back(
            0.0f,
            0.0f,
            static_cast<float>(windowSize.width),
            static_cast<float>(windowSize.height),
            0.0f,
            1.0f);

        // 裁剪矩形同样覆盖整个窗口。
        pipelineCiPack.scissors.emplace_back(VkOffset2D{}, windowSize);

        // 当前示例不开启多重采样。
        pipelineCiPack.multisampleStateCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // 颜色附件把 RGBA 四个分量都写出去。
        pipelineCiPack.colorBlendAttachmentStates.push_back({.colorWriteMask = 0b1111});

        // 把各 vector 里的状态数量和地址同步回原生 Vulkan 结构体。
        pipelineCiPack.UpdateAllArrays();

        // 当前管线现在包含顶点、几何、片段三个阶段。
        pipelineCiPack.SetShaderStages(shaderStageCreateInfos_triangle);

        // 真正创建 Vulkan 图形管线。
        pipeline_triangle.Create(pipelineCiPack);
    };

    auto Destroy = [] {
        // 交换链重建前先主动析构旧管线，避免继续引用旧视口和旧渲染目标。
        pipeline_triangle.~pipeline();
    };

    // 交换链重建后需要重新创建依赖交换链尺寸的图形管线。
    graphicsBase::Base().AddCallback_CreateSwapchain(Create);

    // 交换链销毁前要先销毁旧图形管线。
    graphicsBase::Base().AddCallback_DestroySwapchain(Destroy);

    // 首次启动时先创建一次。
    Create();
}

int main()
{
    // 先完成窗口、实例、设备与交换链初始化。
    if (!InitializeWindow(defaultWindowSize))
        return -1;

    // 拿到屏幕渲染通道和与交换链图像配套的帧缓冲。
    const auto& [renderPass, framebuffers] = RenderPassAndFramebuffers();

    // 创建管线布局。
    CreateLayout();

    // 创建三角形图形管线。
    CreatePipeline();

    // 先创建一个“初始为已完成状态”的栅栏，保证第一帧不用卡在等待上。
    fence fence(VK_FENCE_CREATE_SIGNALED_BIT);

    // 获取交换链图像成功后，呈现引擎会置位这个信号量。
    semaphore semaphore_imageIsAvailable;

    // 图形队列执行完命令缓冲区后，会置位这个信号量。
    semaphore semaphore_renderingIsOver;

    // 本节仍然只使用一个主命令缓冲区。
    commandBuffer commandBuffer;

    // 命令池从图形队列族分配命令缓冲区，并允许逐帧重录。
    commandPool commandPool(
        graphicsBase::Base().QueueFamilyIndex_Graphics(),
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    // 真正向命令池申请一个命令缓冲区对象。
    commandPool.AllocateBuffers(commandBuffer);

    // 清屏颜色继续保留为红色，这样三角形会画在醒目的背景上。
    VkClearValue clearColor = {.color = {1.0f, 0.0f, 0.0f, 1.0f}};

    while (!glfwWindowShouldClose(pWindow))
    {
        // 如果窗口被最小化，就先阻塞等待窗口恢复，避免无意义地继续渲染。
        while (glfwGetWindowAttrib(pWindow, GLFW_ICONIFIED))
            glfwWaitEvents();

        // 等上一帧的 GPU 工作执行完，并顺手把栅栏复位成未完成状态。
        fence.WaitAndReset();

        // 从交换链中取出当前这一帧要渲染的图像。
        graphicsBase::Base().SwapImage(semaphore_imageIsAvailable);

        // 记住当前取到的是第几张交换链图像，后面要用它选择对应帧缓冲。
        const auto i = graphicsBase::Base().CurrentImageIndex();

        // 开始录制这一帧的命令缓冲区；逐帧重录的主命令缓冲区最适合 one-time submit。
        commandBuffer.BeginOneTime();

        // 进入渲染通道，并把当前帧缓冲清成红色。
        renderPass.CmdBegin(commandBuffer, framebuffers[i], {{}, windowSize}, clearColor);

        // 绑定本节创建好的图形管线。
        pipeline_triangle.CmdBind(commandBuffer);

        // CPU 侧仍然只提交 3 个顶点，先生成一个输入三角形。
        // 几何着色器会在这个基础上额外再生成一个更小的内层三角形。
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);

        // 结束当前渲染通道。
        renderPass.CmdEnd(commandBuffer);

        // 结束命令录制，准备提交给图形队列。
        commandBuffer.End();

        // 提交命令时等待“图像可用”信号量，完成后发出“渲染结束”信号量并置位栅栏。
        graphicsBase::Base().SubmitCommandBuffer_Graphics(
            commandBuffer,
            semaphore_imageIsAvailable,
            semaphore_renderingIsOver,
            fence);

        // 呈现阶段等待“渲染结束”信号量，确保不会显示尚未完成的图像。
        graphicsBase::Base().PresentImage(semaphore_renderingIsOver);

        // 处理窗口事件。
        glfwPollEvents();

        // 刷新标题栏中的 FPS 显示。
        TitleFps();
    }

    // 退出前统一释放窗口与 Vulkan 资源。
    TerminateWindow();
    return 0;
}
