#define STB_IMAGE_IMPLEMENTATION
#include "GlfwGeneral.hpp"
#include "RPFB_Screen.hpp"

using namespace vulkan;

// 离屏画线那条管线。
pipelineLayout pipelineLayout_line;
pipeline pipeline_line;

// 把离屏画布采样到屏幕时使用的描述符集布局和图形管线。
descriptorSetLayout descriptorSetLayout_texture;
pipelineLayout pipelineLayout_screen;
pipeline pipeline_screen;

const easyVulkan::renderPassWithFramebuffers& RenderPassAndFramebuffers_Screen()
{
    // 屏幕这边继续沿用普通交换链 render pass。
    static const auto& rpwf = easyVulkan::CreateRpwf_Screen();
    return rpwf;
}

const easyVulkan::renderPassWithFramebuffer& RenderPassAndFramebuffer_Offscreen(VkExtent2D canvasSize)
{
    // 离屏这边使用单独的画布 render pass。
    static const auto& rpwf = easyVulkan::CreateRpwf_Canvas(canvasSize);
    return rpwf;
}

void CreateLayout()
{
    // 离屏画线的顶点着色器要吃一块 push constant：
    // 一个 vec2 画布尺寸 + 两个 vec2 端点坐标 = 24 字节。
    VkPushConstantRange pushConstantRange_offscreen = {
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        24};

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange_offscreen};

    // 创建离屏画线管线布局。
    pipelineLayout_line.Create(pipelineLayoutCreateInfo);

    // 屏幕通路需要采样离屏画布，所以先准备一个 combined image sampler 描述符。
    VkDescriptorSetLayoutBinding descriptorSetLayoutBinding_texture = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT};

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo_texture = {
        .bindingCount = 1,
        .pBindings = &descriptorSetLayoutBinding_texture};

    // 创建屏幕通路的描述符集布局。
    descriptorSetLayout_texture.Create(descriptorSetLayoutCreateInfo_texture);

    // 屏幕通路的 push constant 有两段范围：
    // 1. 顶点着色器用的窗口尺寸；
    // 2. 顶点/片段共用的画布尺寸。
    VkPushConstantRange pushConstantRanges_screen[] = {
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 16},
        {VK_SHADER_STAGE_FRAGMENT_BIT, 8, 8}};

    pipelineLayoutCreateInfo.pushConstantRangeCount = 2;
    pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges_screen;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayout_texture.Address();

    // 创建屏幕通路的管线布局。
    pipelineLayout_screen.Create(pipelineLayoutCreateInfo);
}

void CreatePipeline(VkExtent2D canvasSize)
{
    // 离屏通路使用画线着色器。
    static shaderModule vert_offscreen("shader/Line.vert.spv");
    static shaderModule frag_offscreen("shader/Line.frag.spv");

    VkPipelineShaderStageCreateInfo shaderStageCreateInfos_line[2] = {
        vert_offscreen.StageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT),
        frag_offscreen.StageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT)};

    // 先创建离屏画线管线。
    {
        graphicsPipelineCreateInfoPack pipelineCiPack;

        pipelineCiPack.SetPipelineLayout(pipelineLayout_line);
        pipelineCiPack.SetRenderPass(RenderPassAndFramebuffer_Offscreen(canvasSize).renderPass);

        // 输入图元是线段列表。
        pipelineCiPack.inputAssemblyStateCi.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

        // 离屏画布的视口和裁剪范围都按画布尺寸来。
        pipelineCiPack.viewports.emplace_back(
            0.0f,
            0.0f,
            static_cast<float>(canvasSize.width),
            static_cast<float>(canvasSize.height),
            0.0f,
            1.0f);
        pipelineCiPack.scissors.emplace_back(VkOffset2D{}, canvasSize);

        // 线宽先固定为 1。
        pipelineCiPack.rasterizationStateCi.lineWidth = 1.0f;

        pipelineCiPack.multisampleStateCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        pipelineCiPack.colorBlendAttachmentStates.push_back({.colorWriteMask = 0b1111});

        pipelineCiPack.UpdateAllArrays();
        pipelineCiPack.SetShaderStages(shaderStageCreateInfos_line);

        pipeline_line.Create(pipelineCiPack);
    }

    // 屏幕通路使用“把画布纹理贴到屏幕矩形上”的着色器。
    static shaderModule vert_screen("shader/CanvasToScreen.vert.spv");
    static shaderModule frag_screen("shader/CanvasToScreen.frag.spv");

    static VkPipelineShaderStageCreateInfo shaderStageCreateInfos_screen[2] = {
        vert_screen.StageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT),
        frag_screen.StageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT)};

    auto Create = [] {
        graphicsPipelineCreateInfoPack pipelineCiPack;

        pipelineCiPack.SetPipelineLayout(pipelineLayout_screen);
        pipelineCiPack.SetRenderPass(RenderPassAndFramebuffers_Screen().renderPass);

        // 屏幕矩形仍然使用 triangle strip。
        pipelineCiPack.inputAssemblyStateCi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

        pipelineCiPack.viewports.emplace_back(
            0.0f,
            0.0f,
            static_cast<float>(windowSize.width),
            static_cast<float>(windowSize.height),
            0.0f,
            1.0f);
        pipelineCiPack.scissors.emplace_back(VkOffset2D{}, windowSize);

        pipelineCiPack.multisampleStateCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        pipelineCiPack.colorBlendAttachmentStates.push_back({.colorWriteMask = 0b1111});

        pipelineCiPack.UpdateAllArrays();
        pipelineCiPack.SetShaderStages(shaderStageCreateInfos_screen);

        pipeline_screen.Create(pipelineCiPack);
    };

    auto Destroy = [] {
        // 窗口尺寸变化后，屏幕通路管线需要按新视口重建。
        pipeline_screen.~pipeline();
    };

    graphicsBase::Base().AddCallback_CreateSwapchain(Create);
    graphicsBase::Base().AddCallback_DestroySwapchain(Destroy);
    Create();
}

int main()
{
    // 本章仍然从常规窗口初始化开始。
    if (!InitializeWindow(defaultWindowSize))
        return -1;

    // 先把画布大小设成与窗口一致，便于观察。
    const VkExtent2D canvasSize = windowSize;

    // 一条 render pass 负责最终呈现到屏幕，另一条负责离屏画布。
    const auto& [renderPass_screen, framebuffers_screen] = RenderPassAndFramebuffers_Screen();
    const auto& [renderPass_offscreen, framebuffer_offscreen] = RenderPassAndFramebuffer_Offscreen(canvasSize);

    // 创建布局和两条图形管线。
    CreateLayout();
    CreatePipeline(canvasSize);

    // 常规逐帧同步对象。
    fence fence(VK_FENCE_CREATE_SIGNALED_BIT);
    semaphore semaphore_imageIsAvailable;
    semaphore semaphore_renderingIsOver;

    // 只使用一个主命令缓冲区。
    commandBuffer commandBuffer;
    commandPool commandPool(
        graphicsBase::Base().QueueFamilyIndex_Graphics(),
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    commandPool.AllocateBuffers(commandBuffer);

    // 画布后面要被采样，所以给它准备一个采样器。
    VkSamplerCreateInfo samplerCreateInfo = texture::SamplerCreateInfo();
    sampler sampler(samplerCreateInfo);

    // 再创建一个描述符，把离屏画布写进去。
    const VkDescriptorPoolSize descriptorPoolSizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}};
    descriptorPool descriptorPool_texture(1, descriptorPoolSizes);
    descriptorSet descriptorSet_texture;
    descriptorPool_texture.AllocateSets(descriptorSet_texture, descriptorSetLayout_texture);
    descriptorSet_texture.Write(easyVulkan::ca_canvas.DescriptorImageInfo(sampler), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    // 屏幕通路的清屏值用白底，便于观察离屏画布被贴回屏幕后的结果。
    VkClearValue clearColor = {.color = {1.0f, 1.0f, 1.0f, 1.0f}};

    // 先拿一下当前鼠标位置，作为第一条线段的两个端点。
    double mouseX = 0.0;
    double mouseY = 0.0;
    glfwGetCursorPos(pWindow, &mouseX, &mouseY);

    struct
    {
        // 画布尺寸，供离屏顶点着色器把像素坐标转成 NDC。
        glm::vec2 viewportSize;

        // 线段的两个端点。
        glm::vec2 offsets[2];
    } pushConstants_offscreen = {
        {float(canvasSize.width), float(canvasSize.height)},
        {{float(mouseX), float(mouseY)}, {float(mouseX), float(mouseY)}}};

    // 为了模拟画板效果：
    // 1. clearCanvas 为 true 时清空整张画布；
    // 2. index 用来交替更新两个端点中的一个。
    bool clearCanvas = true;
    bool index = false;

    while (!glfwWindowShouldClose(pWindow))
    {
        while (glfwGetWindowAttrib(pWindow, GLFW_ICONIFIED))
            glfwWaitEvents();

        fence.WaitAndReset();
        graphicsBase::Base().SwapImage(semaphore_imageIsAvailable);
        const uint32_t i = graphicsBase::Base().CurrentImageIndex();

        commandBuffer.BeginOneTime();

        // 若要求清空画布，就先在 render pass 外用 clear 命令把整张离屏画布刷掉。
        if (clearCanvas)
        {
            easyVulkan::CmdClearCanvas(commandBuffer, VkClearColorValue{});
            clearCanvas = false;
        }

        // 第一段：离屏画线，把鼠标路径渲染到 ca_canvas 上。
        renderPass_offscreen.CmdBegin(commandBuffer, framebuffer_offscreen, {{}, canvasSize});
        pipeline_line.CmdBind(commandBuffer);
        vkCmdPushConstants(commandBuffer, pipelineLayout_line, VK_SHADER_STAGE_VERTEX_BIT, 0, 24, &pushConstants_offscreen);
        vkCmdDraw(commandBuffer, 2, 1, 0, 0);
        renderPass_offscreen.CmdEnd(commandBuffer);

        // 第二段：把离屏画布采样到交换链图像上。
        renderPass_screen.CmdBegin(
            commandBuffer,
            framebuffers_screen[i],
            {{}, windowSize},
            clearColor);
        pipeline_screen.CmdBind(commandBuffer);

        // CanvasToScreen.vert 需要窗口尺寸和画布尺寸。
        const glm::vec2 windowSize_screen = {float(::windowSize.width), float(::windowSize.height)};
        vkCmdPushConstants(commandBuffer, pipelineLayout_screen, VK_SHADER_STAGE_VERTEX_BIT, 0, 8, &windowSize_screen);
        vkCmdPushConstants(
            commandBuffer,
            pipelineLayout_screen,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            8,
            8,
            &pushConstants_offscreen.viewportSize);

        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout_screen,
            0,
            1,
            descriptorSet_texture.Address(),
            0,
            nullptr);
        vkCmdDraw(commandBuffer, 4, 1, 0, 0);
        renderPass_screen.CmdEnd(commandBuffer);

        commandBuffer.End();

        graphicsBase::Base().SubmitCommandBuffer_Graphics(
            commandBuffer,
            semaphore_imageIsAvailable,
            semaphore_renderingIsOver,
            fence);
        graphicsBase::Base().PresentImage(semaphore_renderingIsOver);

        // 更新鼠标位置，并把新位置写进另一个端点槽位。
        glfwPollEvents();
        glfwGetCursorPos(pWindow, &mouseX, &mouseY);
        pushConstants_offscreen.offsets[index = !index] = {float(mouseX), float(mouseY)};

        // 按住左键时清空画布，松开后继续画。
        clearCanvas = glfwGetMouseButton(pWindow, GLFW_MOUSE_BUTTON_LEFT);

        TitleFps();
    }

    TerminateWindow();
    return 0;
}
