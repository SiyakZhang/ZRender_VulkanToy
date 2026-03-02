#define STB_IMAGE_IMPLEMENTATION
#include "GlfwGeneral.hpp"
#include "RPFB_Screen.hpp"

using namespace vulkan;

struct vertex
{
    // 矩形顶点在 NDC 中的位置。
    glm::vec2 position;

    // 对应的纹理坐标。
    glm::vec2 texCoord;
};

// 这一课仍然只需要一个“采样 2D 贴图”的描述符集布局。
descriptorSetLayout descriptorSetLayout_texture;

// 同一套布局下再挂一个片段 push constant，用来传 HDR 亮度缩放。
pipelineLayout pipelineLayout_texture;

// 用于显示真正 HDR 贴图的管线。
pipeline pipeline_texture;

// 用于显示“亮度校准参考图”的管线。
pipeline pipeline_calibration;

const easyVulkan::renderPassWithFramebuffers& RenderPassAndFramebuffers()
{
    // 这里只是做屏幕对比展示，继续用普通屏幕 render pass 即可。
    static const auto& rpwf = easyVulkan::CreateRpwf_Screen();
    return rpwf;
}

void CreateLayout()
{
    // 0 号 binding 绑定一张 combined image sampler 贴图。
    VkDescriptorSetLayoutBinding descriptorSetLayoutBinding_texture = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT};

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo_texture = {
        .bindingCount = 1,
        .pBindings = &descriptorSetLayoutBinding_texture};

    descriptorSetLayout_texture.Create(descriptorSetLayoutCreateInfo_texture);

    // HDR 相关片段着色器只额外需要一个 float 亮度缩放值。
    VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = uint32_t(sizeof(float))};

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        .setLayoutCount = 1,
        .pSetLayouts = descriptorSetLayout_texture.Address(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange};

    pipelineLayout_texture.Create(pipelineLayoutCreateInfo);
}

void CreatePipeline()
{
    // HDR 贴图预览仍然使用普通的顶点格式，只是片段着色器改成 HDR 版本。
    static shaderModule vert_texture("shader/Texture.vert.spv");
    static shaderModule frag_texture("shader/Texture_Hdr.frag.spv");
    static VkPipelineShaderStageCreateInfo shaderStageCreateInfos_texture[2] = {
        vert_texture.StageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT),
        frag_texture.StageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT)};

    // 亮度校准这条管线直接画全屏矩形，不需要顶点缓冲区。
    static shaderModule vert_calibration("shader/RenderToImage2d.vert.spv");
    static shaderModule frag_calibration("shader/HdrCalibration.frag.spv");
    static VkPipelineShaderStageCreateInfo shaderStageCreateInfos_calibration[2] = {
        vert_calibration.StageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT),
        frag_calibration.StageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT)};

    auto Create = [] {
        // 先创建真正显示 HDR 贴图的那条管线。
        graphicsPipelineCreateInfoPack pipelineCiPack_texture;

        pipelineCiPack_texture.SetPipelineLayout(pipelineLayout_texture);
        pipelineCiPack_texture.SetRenderPass(RenderPassAndFramebuffers().renderPass);

        // 顶点结构里只有 position 和 texCoord 两项。
        pipelineCiPack_texture.vertexInputBindings.emplace_back(0, uint32_t(sizeof(vertex)), VK_VERTEX_INPUT_RATE_VERTEX);
        pipelineCiPack_texture.vertexInputAttributes.emplace_back(0, 0, VK_FORMAT_R32G32_SFLOAT, uint32_t(offsetof(vertex, position)));
        pipelineCiPack_texture.vertexInputAttributes.emplace_back(1, 0, VK_FORMAT_R32G32_SFLOAT, uint32_t(offsetof(vertex, texCoord)));

        // 每 4 个顶点用 triangle strip 组成一个矩形。
        pipelineCiPack_texture.inputAssemblyStateCi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

        pipelineCiPack_texture.viewports.emplace_back(
            0.0f,
            0.0f,
            static_cast<float>(windowSize.width),
            static_cast<float>(windowSize.height),
            0.0f,
            1.0f);
        pipelineCiPack_texture.scissors.emplace_back(VkOffset2D{}, windowSize);

        pipelineCiPack_texture.multisampleStateCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // 本课不讨论透明混色，颜色附件直接整像素覆盖即可。
        pipelineCiPack_texture.colorBlendAttachmentStates.push_back({
            .colorWriteMask = 0b1111});

        pipelineCiPack_texture.UpdateAllArrays();
        pipelineCiPack_texture.SetShaderStages(shaderStageCreateInfos_texture);
        pipeline_texture.Create(pipelineCiPack_texture);

        // 再创建亮度校准那条全屏管线。
        graphicsPipelineCreateInfoPack pipelineCiPack_calibration;
        pipelineCiPack_calibration.SetPipelineLayout(pipelineLayout_texture);
        pipelineCiPack_calibration.SetRenderPass(RenderPassAndFramebuffers().renderPass);
        pipelineCiPack_calibration.inputAssemblyStateCi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        pipelineCiPack_calibration.viewports.emplace_back(
            0.0f,
            0.0f,
            static_cast<float>(windowSize.width),
            static_cast<float>(windowSize.height),
            0.0f,
            1.0f);
        pipelineCiPack_calibration.scissors.emplace_back(VkOffset2D{}, windowSize);
        pipelineCiPack_calibration.multisampleStateCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        pipelineCiPack_calibration.colorBlendAttachmentStates.push_back({
            .colorWriteMask = 0b1111});
        pipelineCiPack_calibration.UpdateAllArrays();
        pipelineCiPack_calibration.SetShaderStages(shaderStageCreateInfos_calibration);
        pipeline_calibration.Create(pipelineCiPack_calibration);
    };

    auto Destroy = [] {
        // 交换链重建时，两条屏幕管线都要一起销毁重建。
        pipeline_texture.~pipeline();
        pipeline_calibration.~pipeline();
    };

    graphicsBase::Base().AddCallback_CreateSwapchain(Create);
    graphicsBase::Base().AddCallback_DestroySwapchain(Destroy);
    Create();
}

#ifdef _WIN32
float GetSdrWhiteLevel()
{
    // 这里只取当前活动显示路径中的第一个输出设备做演示。
    UINT32 pathInfoCount = 1;
    DISPLAYCONFIG_PATH_INFO pathInfo{};
    UINT32 modeInfoCount = 1;
    DISPLAYCONFIG_MODE_INFO modeInfo{};

    const LONG result = QueryDisplayConfig(
        QDC_ONLY_ACTIVE_PATHS,
        &pathInfoCount,
        &pathInfo,
        &modeInfoCount,
        &modeInfo,
        nullptr);

    if (result != ERROR_SUCCESS && result != ERROR_INSUFFICIENT_BUFFER)
        return 0.0f;

    // Win32 允许读取系统设置里的“HDR/SDR 亮度平衡”。
    DISPLAYCONFIG_SDR_WHITE_LEVEL sdrWhiteLevel{};
    sdrWhiteLevel.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL;
    sdrWhiteLevel.header.size = sizeof(DISPLAYCONFIG_SDR_WHITE_LEVEL);
    sdrWhiteLevel.header.adapterId = pathInfo.targetInfo.adapterId;
    sdrWhiteLevel.header.id = pathInfo.targetInfo.id;

    if (DisplayConfigGetDeviceInfo(&sdrWhiteLevel.header) != ERROR_SUCCESS)
        return 0.0f;

    // 文档里返回值的 1000 对应 80nit，所以这里按教程换算成 nit。
    return sdrWhiteLevel.SDRWhiteLevel / 12.5f;
}

// 读取失败时退回到常用的 HDR 参考白 203nit。
float sdrWhiteLevel = [] {
    const float detectedValue = GetSdrWhiteLevel();
    return detectedValue > 0.0f ? detectedValue : 203.0f;
}();
#else
// 非 Windows 平台先直接用 HDR 参考白 203nit。
float sdrWhiteLevel = 203.0f;
#endif

int main()
{
    // 这一课优先请求 HDR10 的 ST2084 色彩空间。
    PreInitialization_TrySetColorSpaceByOrder(VK_COLOR_SPACE_HDR10_ST2084_EXT);

    if (!InitializeWindow({1280, 720}))
        return -1;

    // 如果最终并没有拿到 HDR10 交换链，这个示例依旧能运行，只是画面不会符合教程预期。
    if (graphicsBase::Base().SwapchainCreateInfo().imageColorSpace != VK_COLOR_SPACE_HDR10_ST2084_EXT)
        std::cout << std::format("[ main ] WARNING\nHDR10 color space is not enabled, so this lesson may not display correctly.\n");

    const auto& [renderPass, framebuffers] = RenderPassAndFramebuffers();
    CreateLayout();
    CreatePipeline();

    // 逐帧同步对象。
    fence fence;
    semaphore semaphore_imageIsAvailable;
    semaphore semaphore_renderingIsOver;

    commandBuffer commandBuffer;
    commandPool commandPool(
        graphicsBase::Base().QueueFamilyIndex_Graphics(),
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    commandPool.AllocateBuffers(commandBuffer);

    // 这张 PNG 是“几乎纯白文字 + 纯白背景”的 SDR 校准参考图。
    texture2d referenceTexture("image/Ch8-5 reference.png", VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, true);

    // 这张 HDR 贴图用 32-bit float 读取，再落到常用的 R16G16B16A16_SFLOAT。
    texture2d hdrTexture("image/memorial.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT, true);

    // HDR 预览纹理用线性过滤和 mipmap。
    VkSamplerCreateInfo samplerCreateInfo = texture::SamplerCreateInfo();
    sampler sampler_texture(samplerCreateInfo);

    // 校准图更适合用最近点采样，避免白色文字边缘被滤波糊掉。
    samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
    samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
    samplerCreateInfo.anisotropyEnable = VK_FALSE;
    sampler sampler_calibration(samplerCreateInfo);

    // 为 HDR 贴图和校准贴图各分配一个描述符集。
    VkDescriptorPoolSize descriptorPoolSizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2}};
    descriptorPool descriptorPool(2, descriptorPoolSizes);
    descriptorSet descriptorSet_texture;
    descriptorSet descriptorSet_calibration;
    descriptorPool.AllocateSets(descriptorSet_texture, descriptorSetLayout_texture);
    descriptorPool.AllocateSets(descriptorSet_calibration, descriptorSetLayout_texture);
    descriptorSet_texture.Write(hdrTexture.DescriptorImageInfo(sampler_texture), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    descriptorSet_calibration.Write(referenceTexture.DescriptorImageInfo(sampler_calibration), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    // HDR 预览图按原始比例摆在屏幕中央。
    const float halfW = static_cast<float>(hdrTexture.Width()) / windowSize.width;
    const float halfH = static_cast<float>(hdrTexture.Height()) / windowSize.height;

    // 这组顶点只服务 HDR 贴图预览；校准画面会直接走全屏顶点着色器。
    vertex vertices[] = {
        {{-halfW, -halfH}, {0, 0}},
        {{halfW, -halfH}, {1, 0}},
        {{-halfW, halfH}, {0, 1}},
        {{halfW, halfH}, {1, 1}},
    };

    vertexBuffer vertexBuffer(sizeof(vertices));
    vertexBuffer.TransferData(vertices);

    // 背景清成黑色，便于观察高亮与近黑区域。
    VkClearValue clearColor = {.color = {}};

    // brightnessScale 表示“把 SDR 白映射到 10000nit 的多少比例”。
    static float brightnessScale = sdrWhiteLevel / 10000.0f;

    // 用滚轮微调亮度缩放值，便于做肉眼校准。
    glfwSetScrollCallback(
        pWindow,
        [](GLFWwindow*, double, double dy) {
            brightnessScale = std::clamp(brightnessScale + static_cast<float>(dy) * 10.0f / 10000.0f, 0.0f, 1.0f);
        });

    // 一开始先显示校准参考图，调好后再进 HDR 预览。
    bool showHdrTexture = false;

    while (!glfwWindowShouldClose(pWindow))
    {
        while (glfwGetWindowAttrib(pWindow, GLFW_ICONIFIED))
            glfwWaitEvents();

        graphicsBase::Base().SwapImage(semaphore_imageIsAvailable);
        const uint32_t imageIndex = graphicsBase::Base().CurrentImageIndex();

        commandBuffer.BeginOneTime();
        renderPass.CmdBegin(commandBuffer, framebuffers[imageIndex], {{}, windowSize}, clearColor);

        if (showHdrTexture)
        {
            // HDR 预览阶段：绑定普通贴图矩形。
            const VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffer.Address(), &offset);

            pipeline_texture.CmdBind(commandBuffer);
            vkCmdBindDescriptorSets(
                commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipelineLayout_texture,
                0,
                1,
                descriptorSet_texture.Address(),
                0,
                nullptr);

            // 把亮度缩放参数传给片段着色器。
            vkCmdPushConstants(commandBuffer, pipelineLayout_texture, VK_SHADER_STAGE_FRAGMENT_BIT, 0, uint32_t(sizeof(float)), &brightnessScale);

            // 画出 HDR 贴图矩形。
            vkCmdDraw(commandBuffer, 4, 1, 0, 0);
        }
        else
        {
            // 校准阶段：直接画全屏矩形。
            pipeline_calibration.CmdBind(commandBuffer);
            vkCmdBindDescriptorSets(
                commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipelineLayout_texture,
                0,
                1,
                descriptorSet_calibration.Address(),
                0,
                nullptr);

            // 同样把亮度缩放值塞给片段着色器。
            vkCmdPushConstants(commandBuffer, pipelineLayout_texture, VK_SHADER_STAGE_FRAGMENT_BIT, 0, uint32_t(sizeof(float)), &brightnessScale);

            // 全屏顶点着色器内部自己根据 gl_VertexIndex 生成矩形。
            vkCmdDraw(commandBuffer, 4, 1, 0, 0);
        }

        renderPass.CmdEnd(commandBuffer);
        commandBuffer.End();

        graphicsBase::Base().SubmitCommandBuffer_Graphics(
            commandBuffer,
            semaphore_imageIsAvailable,
            semaphore_renderingIsOver,
            fence);
        graphicsBase::Base().PresentImage(semaphore_renderingIsOver);

        glfwPollEvents();

        // 校准完成后，按住左键就切到真正的 HDR 贴图预览。
        if (!showHdrTexture && glfwGetMouseButton(pWindow, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
            showHdrTexture = true;

        TitleFps();

        fence.WaitAndReset();
    }

    TerminateWindow();
    return 0;
}
