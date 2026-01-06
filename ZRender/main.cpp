#define STB_IMAGE_IMPLEMENTATION
#include "GlfwGeneral.hpp"
#include "RPFB_Screen.hpp"

using namespace vulkan;

// 贴图版顶点不再携带颜色，而是携带位置和纹理坐标。
struct vertex
{
    // 顶点位置，对应 location 0。
    glm::vec2 position;

    // 纹理坐标，对应 location 1。
    glm::vec2 texCoord;
};

// 这一章开始给贴图准备专用的描述符集布局。
descriptorSetLayout descriptorSetLayout_texture;

// 图形管线布局。
pipelineLayout pipelineLayout_texture;

// 图形管线对象。
pipeline pipeline_texture;

const easyVulkan::renderPassWithFramebuffers& RenderPassAndFramebuffers()
{
    // 这一章继续沿用传统 render pass + framebuffer 路径。
    static const auto& rpwf = easyVulkan::CreateRpwf_Screen();
    return rpwf;
}

void CreateLayout()
{
    // 这里把 0 号 binding 声明成“带采样器的图像”描述符。
    VkDescriptorSetLayoutBinding descriptorSetLayoutBinding_texture = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT};

    // 当前只需要一个 binding。
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo_texture = {
        .bindingCount = 1,
        .pBindings = &descriptorSetLayoutBinding_texture};

    // 创建描述符集布局。
    descriptorSetLayout_texture.Create(descriptorSetLayoutCreateInfo_texture);

    // 管线布局需要知道后面会绑定哪几组描述符集布局。
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        .setLayoutCount = 1,
        .pSetLayouts = descriptorSetLayout_texture.Address()};

    // 创建图形管线布局。
    pipelineLayout_texture.Create(pipelineLayoutCreateInfo);
}

void CreatePipeline()
{
    // 顶点着色器改成贴图版。
    static shaderModule vert("shader/Texture.vert.spv");

    // 片段着色器负责采样纹理。
    static shaderModule frag("shader/Texture.frag.spv");

    // 当前图形管线由顶点和片段两个阶段组成。
    static VkPipelineShaderStageCreateInfo shaderStageCreateInfos_texture[2] = {
        vert.StageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT),
        frag.StageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT)};

    auto Create = [] {
        // 继续使用封装好的图形管线创建信息打包器。
        graphicsPipelineCreateInfoPack pipelineCiPack;

        // 指定管线布局。
        pipelineCiPack.SetPipelineLayout(pipelineLayout_texture);

        // 指定这条管线要在屏幕 render pass 中执行。
        pipelineCiPack.SetRenderPass(RenderPassAndFramebuffers().renderPass);

        // 这一章仍然只绑定一个逐顶点输入缓冲区。
        pipelineCiPack.vertexInputBindings.emplace_back(0, sizeof(vertex), VK_VERTEX_INPUT_RATE_VERTEX);

        // location 0 对应顶点结构里的 position。
        pipelineCiPack.vertexInputAttributes.emplace_back(
            0,
            0,
            VK_FORMAT_R32G32_SFLOAT,
            offsetof(vertex, position));

        // location 1 对应顶点结构里的 texCoord。
        pipelineCiPack.vertexInputAttributes.emplace_back(
            1,
            0,
            VK_FORMAT_R32G32_SFLOAT,
            offsetof(vertex, texCoord));

        // 这次改用 triangle strip，4 个顶点就能拼出一个矩形。
        pipelineCiPack.inputAssemblyStateCi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

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
        pipelineCiPack.SetShaderStages(shaderStageCreateInfos_texture);

        // 创建真正的 Vulkan 图形管线。
        pipeline_texture.Create(pipelineCiPack);
    };

    auto Destroy = [] {
        // 交换链重建前先销毁旧管线，避免持有旧尺寸相关状态。
        pipeline_texture.~pipeline();
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

    // 创建描述符集布局和管线布局。
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

    // 从磁盘加载一张 2D 贴图，并在内部完成上传和 mipmap 生成。
    texture2d texture("image/testImage.png", VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, true);

    // 根据前面封装好的默认参数创建采样器。
    VkSamplerCreateInfo samplerCreateInfo = texture::SamplerCreateInfo();
    sampler sampler(samplerCreateInfo);

    // 描述符池里准备 1 个 combined image sampler 类型的描述符名额即可。
    const VkDescriptorPoolSize descriptorPoolSizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}};

    // 创建描述符池。
    descriptorPool descriptorPool_texture(1, descriptorPoolSizes);

    // 从池里分配一个描述符集。
    descriptorSet descriptorSet_texture;
    descriptorPool_texture.AllocateSets(descriptorSet_texture, descriptorSetLayout_texture);

    // 把“采样器 + 图像视图 + 图像布局”一起写进描述符集。
    descriptorSet_texture.Write(texture.DescriptorImageInfo(sampler), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    // 矩形四个顶点的位置和对应的纹理坐标。
    const vertex vertices[] = {
        {{-0.5f, -0.5f}, {0.0f, 0.0f}},
        {{0.5f, -0.5f}, {1.0f, 0.0f}},
        {{-0.5f, 0.5f}, {0.0f, 1.0f}},
        {{0.5f, 0.5f}, {1.0f, 1.0f}}};

    // 创建顶点缓冲区，并把顶点数据传进去。
    vertexBuffer vertexBuffer_rectangle(sizeof(vertices));
    vertexBuffer_rectangle.TransferData(vertices);

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

        // 绑定顶点缓冲区。
        const VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffer_rectangle.Address(), &offset);

        // 绑定这一章使用的图形管线。
        pipeline_texture.CmdBind(commandBuffer);

        // 再把贴图描述符集绑定到 0 号 set。
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout_texture,
            0,
            1,
            descriptorSet_texture.Address(),
            0,
            nullptr);

        // 绘制 4 个顶点，按 triangle strip 组成一个矩形。
        vkCmdDraw(commandBuffer, 4, 1, 0, 0);

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
