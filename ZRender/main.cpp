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

// 这一课仍然只需要一个“采样 2D 贴图”的描述符集布局和管线布局。
descriptorSetLayout descriptorSetLayout_texture;
pipelineLayout pipelineLayout_texture;

// 左边显示直接 Alpha 贴图，右边显示预乘 Alpha 贴图。
pipeline pipeline_straightAlpha;
pipeline pipeline_premultipliedAlpha;

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

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        .setLayoutCount = 1,
        .pSetLayouts = descriptorSetLayout_texture.Address()};

    pipelineLayout_texture.Create(pipelineLayoutCreateInfo);
}

void CreatePipeline()
{
    // 两张对比图共用同一套贴图 shader。
    static shaderModule vert("shader/Texture.vert.spv");
    static shaderModule frag("shader/Texture.frag.spv");
    static VkPipelineShaderStageCreateInfo shaderStageCreateInfos_texture[2] = {
        vert.StageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT),
        frag.StageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT)};

    auto Create = [] {
        graphicsPipelineCreateInfoPack pipelineCiPack;

        pipelineCiPack.SetPipelineLayout(pipelineLayout_texture);
        pipelineCiPack.SetRenderPass(RenderPassAndFramebuffers().renderPass);

        // 顶点结构里只有 position 和 texCoord 两项。
        pipelineCiPack.vertexInputBindings.emplace_back(0, sizeof(vertex), VK_VERTEX_INPUT_RATE_VERTEX);
        pipelineCiPack.vertexInputAttributes.emplace_back(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(vertex, position));
        pipelineCiPack.vertexInputAttributes.emplace_back(1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(vertex, texCoord));

        // 每 4 个顶点用 triangle strip 组成一个矩形。
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

        // 先创建“直接 Alpha”管线：
        // src.rgb 乘 src.a，再和 dst.rgb 做 one-minus-src-alpha 混合。
        pipelineCiPack.colorBlendAttachmentStates.push_back({
            .blendEnable = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask = 0b1111});

        pipelineCiPack.UpdateAllArrays();
        pipelineCiPack.SetShaderStages(shaderStageCreateInfos_texture);
        pipeline_straightAlpha.Create(pipelineCiPack);

        // 再把源颜色混合因子改成 ONE，得到“预乘 Alpha”管线。
        // 因为预乘贴图的 RGB 已经提前乘过 A 了，所以这里不能再乘一次。
        pipelineCiPack.colorBlendAttachmentStates[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        pipeline_premultipliedAlpha.Create(pipelineCiPack);
    };

    auto Destroy = [] {
        // 交换链重建时两条对比管线都一起重建。
        pipeline_straightAlpha.~pipeline();
        pipeline_premultipliedAlpha.~pipeline();
    };

    graphicsBase::Base().AddCallback_CreateSwapchain(Create);
    graphicsBase::Base().AddCallback_DestroySwapchain(Destroy);
    Create();
}

int main()
{
    if (!InitializeWindow({1280, 720}))
        return -1;

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

    // 先把原始 PNG 作为“直接 Alpha”版本读进来。
    VkExtent2D imageExtent{};
    auto pImageData = texture::LoadFile("image/testImage.png", imageExtent, FormatInfo(VK_FORMAT_R8G8B8A8_UNORM));
    if (!pImageData)
        return -1;

    // 左边这张纹理直接保持原始 RGBA 数据。
    texture2d texture_straightAlpha(pImageData.get(), imageExtent, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM);

    // 右边这张纹理则通过 helper 在 GPU 上先做一次“RGB *= A”。
    easyVulkan::fCreateTexture2d_multiplyAlpha function(VK_FORMAT_R8G8B8A8_UNORM, true, nullptr);
    texture2d texture_premultipliedAlpha = function(pImageData.get(), imageExtent, VK_FORMAT_R8G8B8A8_UNORM);

    // 两张纹理共用同一个采样器。
    VkSamplerCreateInfo samplerCreateInfo = texture::SamplerCreateInfo();
    sampler sampler(samplerCreateInfo);

    // 为左右两张图各分配一个描述符集。
    VkDescriptorPoolSize descriptorPoolSizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2}};
    descriptorPool descriptorPool(2, descriptorPoolSizes);
    descriptorSet descriptorSet_straightAlpha;
    descriptorSet descriptorSet_premultipliedAlpha;
    descriptorPool.AllocateSets(descriptorSet_straightAlpha, descriptorSetLayout_texture);
    descriptorPool.AllocateSets(descriptorSet_premultipliedAlpha, descriptorSetLayout_texture);
    descriptorSet_straightAlpha.Write(texture_straightAlpha.DescriptorImageInfo(sampler), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    descriptorSet_premultipliedAlpha.Write(texture_premultipliedAlpha.DescriptorImageInfo(sampler), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    // 让图片在窗口里放大显示，便于观察边缘滤波差异。
    const float w = 2.0f * imageExtent.width * 10.0f / windowSize.width;
    const float halfH = imageExtent.height * 10.0f / windowSize.height;

    // 前 4 个顶点放左边，后 4 个顶点放右边。
    vertex vertices[] = {
        {{-w, -halfH}, {0, 0}},
        {{0, -halfH}, {1, 0}},
        {{-w, halfH}, {0, 1}},
        {{0, halfH}, {1, 1}},

        {{0, -halfH}, {0, 0}},
        {{w, -halfH}, {1, 0}},
        {{0, halfH}, {0, 1}},
        {{w, halfH}, {1, 1}},
    };

    vertexBuffer vertexBuffer(sizeof(vertices));
    vertexBuffer.TransferData(vertices);

    // 背景故意清成纯红，方便看透明边缘是否被脏色污染。
    VkClearValue clearColor = {.color = {1.0f, 0.0f, 0.0f, 1.0f}};

    while (!glfwWindowShouldClose(pWindow))
    {
        while (glfwGetWindowAttrib(pWindow, GLFW_ICONIFIED))
            glfwWaitEvents();

        graphicsBase::Base().SwapImage(semaphore_imageIsAvailable);
        const uint32_t imageIndex = graphicsBase::Base().CurrentImageIndex();

        commandBuffer.BeginOneTime();
        renderPass.CmdBegin(commandBuffer, framebuffers[imageIndex], {{}, windowSize}, clearColor);

        // 只需要绑定一次顶点缓冲区。
        const VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffer.Address(), &offset);

        // 左边先画“直接 Alpha”。
        pipeline_straightAlpha.CmdBind(commandBuffer);
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout_texture,
            0,
            1,
            descriptorSet_straightAlpha.Address(),
            0,
            nullptr);
        vkCmdDraw(commandBuffer, 4, 1, 0, 0);

        // 右边再画“预乘 Alpha”。
        pipeline_premultipliedAlpha.CmdBind(commandBuffer);
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout_texture,
            0,
            1,
            descriptorSet_premultipliedAlpha.Address(),
            0,
            nullptr);
        vkCmdDraw(commandBuffer, 4, 1, 4, 0);

        renderPass.CmdEnd(commandBuffer);
        commandBuffer.End();

        graphicsBase::Base().SubmitCommandBuffer_Graphics(
            commandBuffer,
            semaphore_imageIsAvailable,
            semaphore_renderingIsOver,
            fence);
        graphicsBase::Base().PresentImage(semaphore_renderingIsOver);

        glfwPollEvents();
        TitleFps();

        fence.WaitAndReset();
    }

    TerminateWindow();
    return 0;
}
