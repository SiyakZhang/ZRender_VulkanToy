#define STB_IMAGE_IMPLEMENTATION
#include "GlfwGeneral.hpp"
#include "RPFB_Screen.hpp"

using namespace vulkan;

struct vertex
{
    // 模型空间中的顶点位置。
    glm::vec3 position;

    // 模型空间中的法线，用于后续光照计算。
    glm::vec3 normal;

    // xyz 存基础颜色，w 存高光强度。
    glm::vec4 albedoSpecular;
};

// 第一个子通道负责生成 G-Buffer。
descriptorSetLayout descriptorSetLayout_gBuffer;
pipelineLayout pipelineLayout_gBuffer;
pipeline pipeline_gBuffer;

// 第二个子通道负责把 G-Buffer 合成为最终屏幕颜色。
descriptorSetLayout descriptorSetLayout_composition;
pipelineLayout pipelineLayout_composition;
pipeline pipeline_composition;

const easyVulkan::renderPassWithFramebuffers& RenderPassAndFramebuffers()
{
    // 这一课切换到“两个子通道”的延迟渲染 render pass。
    static const auto& rpwf = easyVulkan::CreateRpwf_DeferredToScreen();
    return rpwf;
}

void CreateLayout()
{
    // G-Buffer 子通道只需要一个 uniform buffer，里面放投影矩阵和观察矩阵。
    VkDescriptorSetLayoutBinding descriptorSetLayoutBinding_gBuffer = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT};

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
        .bindingCount = 1,
        .pBindings = &descriptorSetLayoutBinding_gBuffer};

    descriptorSetLayout_gBuffer.Create(descriptorSetLayoutCreateInfo);

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        .setLayoutCount = 1,
        .pSetLayouts = descriptorSetLayout_gBuffer.Address()};

    pipelineLayout_gBuffer.Create(pipelineLayoutCreateInfo);

    // Composition 子通道要读一份完整的场景常量和两张输入附件。
    VkDescriptorSetLayoutBinding descriptorSetLayoutBindings_composition[2] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
            .descriptorCount = 2,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        }};

    descriptorSetLayoutCreateInfo.bindingCount = 2;
    descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings_composition;
    descriptorSetLayout_composition.Create(descriptorSetLayoutCreateInfo);

    pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayout_composition.Address();
    pipelineLayout_composition.Create(pipelineLayoutCreateInfo);
}

void CreatePipeline()
{
    // G-Buffer 阶段：把位置相关信息整理进两个颜色附件。
    static shaderModule vert_gBuffer("shader/GBuffer.vert.spv");
    static shaderModule frag_gBuffer("shader/GBuffer.frag.spv");
    static VkPipelineShaderStageCreateInfo shaderStageCreateInfos_gBuffer[2] = {
        vert_gBuffer.StageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT),
        frag_gBuffer.StageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT)};

    // Composition 阶段：读取输入附件并计算最终光照颜色。
    static shaderModule vert_composition("shader/Composition.vert.spv");
    static shaderModule frag_composition("shader/Composition.frag.spv");
    static VkPipelineShaderStageCreateInfo shaderStageCreateInfos_composition[2] = {
        vert_composition.StageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT),
        frag_composition.StageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT)};

    // 这里用 specialization constant 覆盖片段着色器里的 shininess 常量。
    static constexpr int32_t shininess = 64;
    static VkSpecializationMapEntry mapEntry = {1, 0, sizeof(shininess)};
    static VkSpecializationInfo specializationInfo = {1, &mapEntry, sizeof(shininess), &shininess};
    shaderStageCreateInfos_composition[1].pSpecializationInfo = &specializationInfo;

    auto Create = [] {
        // 先创建 G-Buffer 管线。
        {
            graphicsPipelineCreateInfoPack pipelineCiPack;

            pipelineCiPack.SetPipelineLayout(pipelineLayout_gBuffer);
            pipelineCiPack.SetRenderPass(RenderPassAndFramebuffers().renderPass, 0);

            // 第一个顶点缓冲区按顶点读取 position/normal/albedoSpecular。
            pipelineCiPack.vertexInputBindings.emplace_back(0, sizeof(vertex), VK_VERTEX_INPUT_RATE_VERTEX);

            // 第二个顶点缓冲区按实例读取立方体平移偏移。
            pipelineCiPack.vertexInputBindings.emplace_back(1, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_INSTANCE);

            pipelineCiPack.vertexInputAttributes.emplace_back(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vertex, position));
            pipelineCiPack.vertexInputAttributes.emplace_back(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vertex, normal));
            pipelineCiPack.vertexInputAttributes.emplace_back(2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vertex, albedoSpecular));
            pipelineCiPack.vertexInputAttributes.emplace_back(3, 1, VK_FORMAT_R32G32B32_SFLOAT, 0);

            pipelineCiPack.inputAssemblyStateCi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

            pipelineCiPack.viewports.emplace_back(
                0.0f,
                0.0f,
                static_cast<float>(windowSize.width),
                static_cast<float>(windowSize.height),
                0.0f,
                1.0f);
            pipelineCiPack.scissors.emplace_back(VkOffset2D{}, windowSize);

            pipelineCiPack.rasterizationStateCi.cullMode = VK_CULL_MODE_BACK_BIT;
            pipelineCiPack.rasterizationStateCi.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            pipelineCiPack.multisampleStateCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

            // G-Buffer 阶段需要正常做深度测试，把最近的表面信息留下来。
            pipelineCiPack.depthStencilStateCi.depthTestEnable = VK_TRUE;
            pipelineCiPack.depthStencilStateCi.depthWriteEnable = VK_TRUE;
            pipelineCiPack.depthStencilStateCi.depthCompareOp = VK_COMPARE_OP_LESS;

            // 这个子通道会往两张颜色附件同时输出，所以准备两份 color blend 状态。
            pipelineCiPack.colorBlendAttachmentStates.resize(2);
            pipelineCiPack.colorBlendAttachmentStates[0].colorWriteMask = 0b1111;
            pipelineCiPack.colorBlendAttachmentStates[1].colorWriteMask = 0b1111;

            pipelineCiPack.UpdateAllArrays();
            pipelineCiPack.SetShaderStages(shaderStageCreateInfos_gBuffer);

            pipeline_gBuffer.Create(pipelineCiPack);
        }

        // 再创建 Composition 管线。
        {
            graphicsPipelineCreateInfoPack pipelineCiPack;

            pipelineCiPack.SetPipelineLayout(pipelineLayout_composition);
            pipelineCiPack.SetRenderPass(RenderPassAndFramebuffers().renderPass, 1);

            // 全屏合成阶段只需要 4 个顶点组成一个 triangle strip 矩形。
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
            pipelineCiPack.SetShaderStages(shaderStageCreateInfos_composition);

            pipeline_composition.Create(pipelineCiPack);
        }
    };

    auto Destroy = [] {
        // 交换链重建时，两条依赖屏幕尺寸和 render pass 的图形管线都要重建。
        pipeline_gBuffer.~pipeline();
        pipeline_composition.~pipeline();
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

    // 仍然只录制一个主命令缓冲区。
    commandBuffer commandBuffer;
    commandPool commandPool(
        graphicsBase::Base().QueueFamilyIndex_Graphics(),
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    commandPool.AllocateBuffers(commandBuffer);

    // 每个面的顶点都带一条法线和一组材质参数。
    vertex vertices[] = {
        // x+ 面。
        {{1, 1, -1}, {1, 0, 0}, glm::vec4(1)},
        {{1, -1, -1}, {1, 0, 0}, glm::vec4(1)},
        {{1, 1, 1}, {1, 0, 0}, glm::vec4(1)},
        {{1, -1, 1}, {1, 0, 0}, glm::vec4(1)},

        // x- 面。
        {{-1, 1, 1}, {-1, 0, 0}, glm::vec4(1)},
        {{-1, -1, 1}, {-1, 0, 0}, glm::vec4(1)},
        {{-1, 1, -1}, {-1, 0, 0}, glm::vec4(1)},
        {{-1, -1, -1}, {-1, 0, 0}, glm::vec4(1)},

        // y+ 面。
        {{1, 1, -1}, {0, 1, 0}, glm::vec4(1)},
        {{1, 1, 1}, {0, 1, 0}, glm::vec4(1)},
        {{-1, 1, -1}, {0, 1, 0}, glm::vec4(1)},
        {{-1, 1, 1}, {0, 1, 0}, glm::vec4(1)},

        // y- 面。
        {{1, -1, -1}, {0, -1, 0}, glm::vec4(1)},
        {{-1, -1, -1}, {0, -1, 0}, glm::vec4(1)},
        {{1, -1, 1}, {0, -1, 0}, glm::vec4(1)},
        {{-1, -1, 1}, {0, -1, 0}, glm::vec4(1)},

        // z+ 面。
        {{1, 1, 1}, {0, 0, 1}, glm::vec4(1)},
        {{1, -1, 1}, {0, 0, 1}, glm::vec4(1)},
        {{-1, 1, 1}, {0, 0, 1}, glm::vec4(1)},
        {{-1, -1, 1}, {0, 0, 1}, glm::vec4(1)},

        // z- 面。
        {{-1, 1, -1}, {0, 0, -1}, glm::vec4(1)},
        {{-1, -1, -1}, {0, 0, -1}, glm::vec4(1)},
        {{1, 1, -1}, {0, 0, -1}, glm::vec4(1)},
        {{1, -1, -1}, {0, 0, -1}, glm::vec4(1)},
    };

    vertexBuffer vertexBuffer_perVertex(sizeof(vertices));
    vertexBuffer_perVertex.TransferData(vertices);

    // 继续沿用上一课那组由近到远摆开的实例位置。
    glm::vec3 instanceOffsets[] = {
        {-4, -4, 6}, {4, -4, 6},
        {-4, 4, 10}, {4, 4, 10},
        {-4, -4, 14}, {4, -4, 14},
        {-4, 4, 18}, {4, 4, 18},
        {-4, -4, 22}, {4, -4, 22},
        {-4, 4, 26}, {4, 4, 26},
    };

    vertexBuffer vertexBuffer_perInstance(sizeof(instanceOffsets));
    vertexBuffer_perInstance.TransferData(instanceOffsets);

    // 每个面用两个三角形，也就是 6 个索引。
    uint16_t indices[36] = {0, 1, 2, 2, 1, 3};
    for (size_t faceIndex = 1; faceIndex < 6; ++faceIndex)
    {
        for (size_t triangleIndex = 0; triangleIndex < 6; ++triangleIndex)
        {
            indices[faceIndex * 6 + triangleIndex] = static_cast<uint16_t>(indices[triangleIndex] + faceIndex * 4);
        }
    }

    indexBuffer indexBuffer(sizeof(indices));
    indexBuffer.TransferData(indices);

    struct
    {
        // G-Buffer 顶点着色器要用的投影矩阵。
        glm::mat4 proj = FlipVertical(
            glm::infinitePerspectiveLH_ZO(
                glm::radians(60.0f),
                static_cast<float>(windowSize.width) / static_cast<float>(windowSize.height),
                0.1f));

        // 观察矩阵负责把世界坐标换到相机坐标。
        glm::mat4 view = glm::lookAtLH(
            glm::vec3(0, 0, 0),
            glm::vec3(0, 0, 1),
            glm::vec3(-1, 0, 0));

        // 当前启用的灯光数量。
        int32_t lightCount = 0;

        struct
        {
            // std140 布局下 vec3 需要按 16 字节对齐。
            alignas(16) glm::vec3 position;
            alignas(16) glm::vec3 color;
            float strength;
        } lights[8];
    } descriptorConstants;

    // 配三盏颜色不同、位置不同的点光源，便于观察延迟渲染结果。
    descriptorConstants.lightCount = 3;
    descriptorConstants.lights[0] = {{0.0f, 4.0f, 6.0f}, {1.0f, 0.0f, 0.0f}, 100.0f};
    descriptorConstants.lights[1] = {{0.0f, 0.0f, 16.0f}, {0.0f, 1.0f, 0.0f}, 100.0f};
    descriptorConstants.lights[2] = {{0.0f, -4.0f, 6.0f}, {0.0f, 0.0f, 1.0f}, 100.0f};

    // 统一放进一个 uniform buffer，两个子通道按需读取不同的前缀范围。
    uniformBuffer uniformBuffer(sizeof(descriptorConstants));
    uniformBuffer.TransferData(descriptorConstants);

    // 描述符池需要：两个 uniform buffer 描述符，两个 input attachment 描述符。
    VkDescriptorPoolSize descriptorPoolSizes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 2},
    };

    descriptorPool descriptorPool(2, descriptorPoolSizes);
    descriptorSet descriptorSet_gBuffer;
    static descriptorSet descriptorSet_composition;
    descriptorPool.AllocateSets(descriptorSet_gBuffer, descriptorSetLayout_gBuffer);
    descriptorPool.AllocateSets(descriptorSet_composition, descriptorSetLayout_composition);

    // G-Buffer 阶段只读 proj + view，Composition 阶段则读取整个场景常量块。
    VkDescriptorBufferInfo bufferInfos[] = {
        {uniformBuffer, 0, sizeof(glm::mat4) * 2},
        {uniformBuffer, 0, VK_WHOLE_SIZE},
    };
    descriptorSet_gBuffer.Write(bufferInfos[0], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, 0);
    descriptorSet_composition.Write(bufferInfos[1], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, 0);

    // 交换链重建后，G-Buffer 图像视图会变化，所以输入附件描述符也要重写。
    auto UpdateDescriptorSet_InputAttachments = [] {
        VkDescriptorImageInfo imageInfos[2] = {
            {VK_NULL_HANDLE, easyVulkan::ca_deferredToScreen_normalZ.ImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {VK_NULL_HANDLE, easyVulkan::ca_deferredToScreen_albedoSpecular.ImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        };
        descriptorSet_composition.Write(imageInfos, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, 0);
    };
    graphicsBase::Base().AddCallback_CreateSwapchain(UpdateDescriptorSet_InputAttachments);
    UpdateDescriptorSet_InputAttachments();

    // 4 个附件的清屏值分别对应：交换链、法线+z、颜色+高光、深度。
    VkClearValue clearValues[4] = {
        {.color = {}},
        {.color = {}},
        {.color = {}},
        {.depthStencil = {1.0f, 0}},
    };

    while (!glfwWindowShouldClose(pWindow))
    {
        while (glfwGetWindowAttrib(pWindow, GLFW_ICONIFIED))
            glfwWaitEvents();

        graphicsBase::Base().SwapImage(semaphore_imageIsAvailable);
        const uint32_t imageIndex = graphicsBase::Base().CurrentImageIndex();

        commandBuffer.BeginOneTime();
        renderPass.CmdBegin(commandBuffer, framebuffers[imageIndex], {{}, windowSize}, clearValues);

        // 第一个子通道：把几何信息写进两张 G-Buffer。
        pipeline_gBuffer.CmdBind(commandBuffer);
        VkBuffer vertexBuffers[2] = {vertexBuffer_perVertex, vertexBuffer_perInstance};
        VkDeviceSize vertexBufferOffsets[2] = {};
        vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, vertexBufferOffsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout_gBuffer,
            0,
            1,
            descriptorSet_gBuffer.Address(),
            0,
            nullptr);
        vkCmdDrawIndexed(commandBuffer, 36, 12, 0, 0, 0);

        // 切到第二个子通道开始合成。
        renderPass.CmdNext(commandBuffer);

        // 第二个子通道：读取输入附件并算光照，输出到交换链图像。
        pipeline_composition.CmdBind(commandBuffer);
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout_composition,
            0,
            1,
            descriptorSet_composition.Address(),
            0,
            nullptr);
        vkCmdDraw(commandBuffer, 4, 1, 0, 0);

        renderPass.CmdEnd(commandBuffer);
        commandBuffer.End();

        // 第一子通道仍然会进行深度测试，所以等待图像可用的阶段仍不能晚于 early-fragment-tests。
        graphicsBase::Base().SubmitCommandBuffer_Graphics(
            commandBuffer,
            semaphore_imageIsAvailable,
            semaphore_renderingIsOver,
            fence,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
        graphicsBase::Base().PresentImage(semaphore_renderingIsOver);

        glfwPollEvents();
        TitleFps();

        fence.WaitAndReset();
    }

    TerminateWindow();
    return 0;
}
