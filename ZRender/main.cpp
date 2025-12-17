#define STB_IMAGE_IMPLEMENTATION
#include "GlfwGeneral.hpp"
#include "RPFB_Screen.hpp"
#include "VKBase+.h"

using namespace vulkan;

// CPU 侧顶点结构必须与顶点着色器声明的输入布局一一对应。
struct vertex
{
    // 顶点位置，对应 location 0。
    glm::vec2 position;

    // 顶点颜色，对应 location 1。
    glm::vec4 color;
};

// 图形管线布局。
pipelineLayout pipelineLayout_triangle;

// 图形管线对象。
pipeline pipeline_triangle;

const easyVulkan::renderPassWithFramebuffers& RenderPassAndFramebuffers()
{
    // 这一章继续沿用传统 render pass + framebuffer 路径。
    static const auto& rpwf = easyVulkan::CreateRpwf_Screen();
    return rpwf;
}

void CreateLayout()
{
    // Push constant 想要在着色器里可见，必须先在管线布局里声明对应范围。
    VkPushConstantRange pushConstantRange = {
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        24};

    // 本章要传 3 个 vec2 给顶点着色器。
    // 一个 vec2 是 8 字节，所以总大小正好是 24 字节。
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange};

    // 用带 push constant range 的布局来创建管线布局。
    pipelineLayout_triangle.Create(pipelineLayoutCreateInfo);
}

void CreatePipeline()
{
    // 顶点着色器改成 push constant 版本。
    static shaderModule vert("shader/PushConstant.vert.spv");

    // 片段着色器继续复用之前的版本。
    static shaderModule frag("shader/VertexBuffer.frag.spv");

    // 当前图形管线由顶点和片段两个阶段组成。
    static VkPipelineShaderStageCreateInfo shaderStageCreateInfos_triangle[2] = {
        vert.StageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT),
        frag.StageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT)};

    auto Create = [] {
        // 继续使用封装好的图形管线创建信息打包器。
        graphicsPipelineCreateInfoPack pipelineCiPack;

        // 指定管线布局。
        pipelineCiPack.SetPipelineLayout(pipelineLayout_triangle);

        // 指定这条管线要在屏幕 render pass 中执行。
        pipelineCiPack.SetRenderPass(RenderPassAndFramebuffers().renderPass);

        // 这一章重新回到单个逐顶点输入缓冲区。
        pipelineCiPack.vertexInputBindings.emplace_back(0, sizeof(vertex), VK_VERTEX_INPUT_RATE_VERTEX);

        // location 0 对应顶点结构里的 position。
        pipelineCiPack.vertexInputAttributes.emplace_back(
            0,
            0,
            VK_FORMAT_R32G32_SFLOAT,
            offsetof(vertex, position));

        // location 1 对应顶点结构里的 color。
        pipelineCiPack.vertexInputAttributes.emplace_back(
            1,
            0,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            offsetof(vertex, color));

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

        // 本节依然不开启多重采样。
        pipelineCiPack.multisampleStateCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // 颜色附件照常写出 RGBA 四个分量。
        pipelineCiPack.colorBlendAttachmentStates.push_back({.colorWriteMask = 0b1111});

        // 同步内部数组指针与数量。
        pipelineCiPack.UpdateAllArrays();

        // 挂上顶点和片段两个着色器阶段。
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

    // 交换链销毁前销毁旧图形管线。
    graphicsBase::Base().AddCallback_DestroySwapchain(Destroy);

    // 首次启动时先创建一次。
    Create();
}

int main()
{
    // 这一章只需要常规初始化流程即可。
    if (!InitializeWindow(defaultWindowSize))
        return -1;

    // 拿到屏幕 render pass 和每张交换链图像对应的 framebuffer。
    const auto& [renderPass, framebuffers] = RenderPassAndFramebuffers();

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

    // 三个实例共用同一份三角形顶点数据。
    const vertex vertices[] = {
        {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{-0.5f, 0.5f}, {0.0f, 1.0f, 0.0f, 1.0f}},
        {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f, 1.0f}}};

    // 创建顶点缓冲区，并把顶点数据传进去。
    vertexBuffer vertexBuffer_triangle(sizeof(vertices));
    vertexBuffer_triangle.TransferData(vertices);

    // push constant 里直接写三组位置偏移。
    // 后面顶点着色器会用 gl_InstanceIndex 取出当前实例对应的偏移量。
    const glm::vec2 pushConstants[] = {
        {0.0f, 0.0f},
        {-0.5f, 0.0f},
        {0.5f, 0.0f}};

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

        // 记录当前交换链图像索引。
        const uint32_t i = graphicsBase::Base().CurrentImageIndex();

        // 开始录制这一帧的主命令缓冲区。
        commandBuffer.BeginOneTime();

        // 开始 render pass，并把当前 framebuffer 清成红色。
        renderPass.CmdBegin(commandBuffer, framebuffers[i], {{}, windowSize}, clearColor);

        // 绑定单个逐顶点输入缓冲区。
        const VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffer_triangle.Address(), &offset);

        // 绑定这一章使用的图形管线。
        pipeline_triangle.CmdBind(commandBuffer);

        // 把三组实例偏移一次性写入 push constant。
        // 因为这块数据会直接录进命令缓冲区，所以每次重录命令时都要重新写。
        vkCmdPushConstants(
            commandBuffer,
            pipelineLayout_triangle,
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(pushConstants),
            &pushConstants);

        // 每个实例绘制 3 个顶点，一共绘制 3 个实例。
        vkCmdDraw(commandBuffer, 3, 3, 0, 0);

        // 结束 render pass。
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
