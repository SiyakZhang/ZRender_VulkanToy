#define STB_IMAGE_IMPLEMENTATION
#include "GlfwGeneral.hpp"
#include "RPFB_Screen.hpp"

using namespace vulkan;

struct vertex
{
    // 立方体顶点在模型空间中的位置。
    glm::vec3 position;

    // 每个面的颜色，直接传给片段着色器显示。
    glm::vec4 color;
};

// 这条管线负责把彩色立方体绘制到带深度附件的屏幕 render pass 中。
pipelineLayout pipelineLayout_into3d;
pipeline pipeline_into3d;

const easyVulkan::renderPassWithFramebuffers& RenderPassAndFramebuffers()
{
    // 这一课开始改用“颜色附件 + 深度模板附件”的屏幕渲染路径。
    static const auto& rpwf = easyVulkan::CreateRpwf_ScreenWithDS();
    return rpwf;
}

void CreateLayout()
{
    // 顶点着色器需要一整个 mat4 投影矩阵，所以 push constant 大小是 64 字节。
    VkPushConstantRange pushConstantRange = {
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        64};

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange};

    // 当前这条 3D 管线不依赖描述符，只要 push constant 即可。
    pipelineLayout_into3d.Create(pipelineLayoutCreateInfo);
}

void CreatePipeline()
{
    // 顶点着色器负责把模型点位变换到裁剪空间。
    static shaderModule vert("shader/Into3d.vert.spv");

    // 默认使用彩色片段着色器。
    // 如果想直接观察深度分布，可以把这里改成 "shader/Into3d_visualizeDepth.frag.spv"。
    static shaderModule frag("shader/Into3d.frag.spv");

    VkPipelineShaderStageCreateInfo shaderStageCreateInfos[2] = {
        vert.StageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT),
        frag.StageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT)};

    auto Create = [] {
        graphicsPipelineCreateInfoPack pipelineCiPack;

        // 绑定当前课程使用的管线布局和 render pass。
        pipelineCiPack.SetPipelineLayout(pipelineLayout_into3d);
        pipelineCiPack.SetRenderPass(RenderPassAndFramebuffers().renderPass);

        // 第一个顶点缓冲区按“每顶点”读取 position/color。
        pipelineCiPack.vertexInputBindings.emplace_back(0, sizeof(vertex), VK_VERTEX_INPUT_RATE_VERTEX);

        // 第二个顶点缓冲区按“每实例”读取立方体整体偏移。
        pipelineCiPack.vertexInputBindings.emplace_back(1, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_INSTANCE);

        // location 0 对应 position。
        pipelineCiPack.vertexInputAttributes.emplace_back(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vertex, position));

        // location 1 对应 color。
        pipelineCiPack.vertexInputAttributes.emplace_back(1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vertex, color));

        // location 2 对应实例偏移量。
        pipelineCiPack.vertexInputAttributes.emplace_back(2, 1, VK_FORMAT_R32G32B32_SFLOAT, 0);

        // 每个面最终由两个三角形组成，所以这里使用 triangle list。
        pipelineCiPack.inputAssemblyStateCi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // 视口和裁剪矩形都覆盖整个窗口。
        pipelineCiPack.viewports.emplace_back(
            0.0f,
            0.0f,
            static_cast<float>(windowSize.width),
            static_cast<float>(windowSize.height),
            0.0f,
            1.0f);
        pipelineCiPack.scissors.emplace_back(VkOffset2D{}, windowSize);

        // 开启背面剔除，只保留朝向观察者的面。
        pipelineCiPack.rasterizationStateCi.cullMode = VK_CULL_MODE_BACK_BIT;
        pipelineCiPack.rasterizationStateCi.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        // 当前示例不启用 MSAA。
        pipelineCiPack.multisampleStateCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // 开启深度测试和深度写入，深度更小的片段才能通过。
        pipelineCiPack.depthStencilStateCi.depthTestEnable = VK_TRUE;
        pipelineCiPack.depthStencilStateCi.depthWriteEnable = VK_TRUE;
        pipelineCiPack.depthStencilStateCi.depthCompareOp = VK_COMPARE_OP_LESS;

        // 颜色附件照常写出 RGBA 四个分量。
        pipelineCiPack.colorBlendAttachmentStates.push_back({.colorWriteMask = 0b1111});

        pipelineCiPack.UpdateAllArrays();
        pipelineCiPack.SetShaderStages(shaderStageCreateInfos);

        pipeline_into3d.Create(pipelineCiPack);
    };

    auto Destroy = [] {
        // 交换链尺寸变化后，屏幕相关图形管线需要一起重建。
        pipeline_into3d.~pipeline();
    };

    graphicsBase::Base().AddCallback_CreateSwapchain(Create);
    graphicsBase::Base().AddCallback_DestroySwapchain(Destroy);
    Create();
}

int main()
{
    // 3D 画面横向空间更大一点，直接按教程把窗口设成 1280x720。
    if (!InitializeWindow({1280, 720}))
        return -1;

    // 这里拿到的是“带深度附件”的屏幕 render pass 与所有 framebuffer。
    const auto& [renderPass, framebuffers] = RenderPassAndFramebuffers();

    CreateLayout();
    CreatePipeline();

    // 逐帧同步对象。
    fence fence;
    semaphore semaphore_imageIsAvailable;
    semaphore semaphore_renderingIsOver;

    // 仍然只申请一个主命令缓冲区循环复用。
    commandBuffer commandBuffer;
    commandPool commandPool(
        graphicsBase::Base().QueueFamilyIndex_Graphics(),
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    commandPool.AllocateBuffers(commandBuffer);

    // 24 个顶点分别描述立方体 6 个面的 4 个角点。
    vertex vertices[] = {
        // x+ 面。
        {{1, 1, -1}, {1, 0, 0, 1}},
        {{1, -1, -1}, {1, 0, 0, 1}},
        {{1, 1, 1}, {1, 0, 0, 1}},
        {{1, -1, 1}, {1, 0, 0, 1}},

        // x- 面。
        {{-1, 1, 1}, {0, 1, 1, 1}},
        {{-1, -1, 1}, {0, 1, 1, 1}},
        {{-1, 1, -1}, {0, 1, 1, 1}},
        {{-1, -1, -1}, {0, 1, 1, 1}},

        // y+ 面。
        {{1, 1, -1}, {0, 1, 0, 1}},
        {{1, 1, 1}, {0, 1, 0, 1}},
        {{-1, 1, -1}, {0, 1, 0, 1}},
        {{-1, 1, 1}, {0, 1, 0, 1}},

        // y- 面。
        {{1, -1, -1}, {1, 0, 1, 1}},
        {{-1, -1, -1}, {1, 0, 1, 1}},
        {{1, -1, 1}, {1, 0, 1, 1}},
        {{-1, -1, 1}, {1, 0, 1, 1}},

        // z+ 面。
        {{1, 1, 1}, {0, 0, 1, 1}},
        {{1, -1, 1}, {0, 0, 1, 1}},
        {{-1, 1, 1}, {0, 0, 1, 1}},
        {{-1, -1, 1}, {0, 0, 1, 1}},

        // z- 面。
        {{-1, 1, -1}, {1, 1, 0, 1}},
        {{-1, -1, -1}, {1, 1, 0, 1}},
        {{1, 1, -1}, {1, 1, 0, 1}},
        {{1, -1, -1}, {1, 1, 0, 1}},
    };

    // 把逐顶点数据上传到第一个顶点缓冲区。
    vertexBuffer vertexBuffer_perVertex(sizeof(vertices));
    vertexBuffer_perVertex.TransferData(vertices);

    // 这 12 个偏移量对应 12 个实例，让多个立方体排成由近到远的阵列。
    glm::vec3 instanceOffsets[] = {
        {-4, -4, 6}, {4, -4, 6},
        {-4, 4, 10}, {4, 4, 10},
        {-4, -4, 14}, {4, -4, 14},
        {-4, 4, 18}, {4, 4, 18},
        {-4, -4, 22}, {4, -4, 22},
        {-4, 4, 26}, {4, 4, 26},
    };

    // 把逐实例偏移上传到第二个顶点缓冲区。
    vertexBuffer vertexBuffer_perInstance(sizeof(instanceOffsets));
    vertexBuffer_perInstance.TransferData(instanceOffsets);

    // 每个面由两个三角形组成，所以一共是 6 个面 * 6 个索引。
    uint16_t indices[36] = {0, 1, 2, 2, 1, 3};

    // 后面 5 个面的索引只是在第一个面的基础上整体平移 4 个顶点。
    for (size_t faceIndex = 1; faceIndex < 6; ++faceIndex)
    {
        for (size_t triangleIndex = 0; triangleIndex < 6; ++triangleIndex)
        {
            indices[faceIndex * 6 + triangleIndex] = static_cast<uint16_t>(indices[triangleIndex] + faceIndex * 4);
        }
    }

    // 索引缓冲区负责复用每个面的 4 个顶点。
    indexBuffer indexBuffer(sizeof(indices));
    indexBuffer.TransferData(indices);

    // 这里直接生成左手系、深度范围为 [0, 1] 的无限远透视投影矩阵。
    // 再调用 FlipVertical 处理 GLSL 与 Vulkan 在 y 方向上的差异。
    glm::mat4 proj = FlipVertical(
        glm::infinitePerspectiveLH_ZO(
            glm::radians(60.0f),
            static_cast<float>(windowSize.width) / static_cast<float>(windowSize.height),
            0.1f));

    // 颜色附件清成黑色，深度附件清成 1.0，表示“当前最远”。
    VkClearValue clearValues[2] = {
        {.color = {0.0f, 0.0f, 0.0f, 1.0f}},
        {.depthStencil = {1.0f, 0}},
    };

    while (!glfwWindowShouldClose(pWindow))
    {
        while (glfwGetWindowAttrib(pWindow, GLFW_ICONIFIED))
            glfwWaitEvents();

        graphicsBase::Base().SwapImage(semaphore_imageIsAvailable);
        const uint32_t imageIndex = graphicsBase::Base().CurrentImageIndex();

        commandBuffer.BeginOneTime();

        // 开始带深度附件的 render pass。
        renderPass.CmdBegin(commandBuffer, framebuffers[imageIndex], {{}, windowSize}, clearValues);

        // 绑定本课唯一一条 3D 图形管线。
        pipeline_into3d.CmdBind(commandBuffer);

        // 同时绑定逐顶点缓冲区和逐实例缓冲区。
        VkBuffer vertexBuffers[2] = {vertexBuffer_perVertex, vertexBuffer_perInstance};
        VkDeviceSize vertexBufferOffsets[2] = {};
        vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, vertexBufferOffsets);

        // 绑定索引缓冲区，后续 drawIndexed 会按索引顺序取顶点。
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

        // 把投影矩阵作为 push constant 传给顶点着色器。
        vkCmdPushConstants(commandBuffer, pipelineLayout_into3d, VK_SHADER_STAGE_VERTEX_BIT, 0, 64, &proj);

        // 一次绘制 36 个索引、12 个实例。
        vkCmdDrawIndexed(commandBuffer, 36, 12, 0, 0, 0);

        renderPass.CmdEnd(commandBuffer);
        commandBuffer.End();

        // 因为 render pass 里对子通道的等待阶段设到了 early-fragment-tests，
        // 所以这里等待交换链图像可用时，最晚也要在该阶段之前完成等待。
        graphicsBase::Base().SubmitCommandBuffer_Graphics(
            commandBuffer,
            semaphore_imageIsAvailable,
            semaphore_renderingIsOver,
            fence,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
        graphicsBase::Base().PresentImage(semaphore_renderingIsOver);

        glfwPollEvents();
        TitleFps();

        // 本帧 GPU 执行结束后再进入下一帧。
        fence.WaitAndReset();
    }

    TerminateWindow();
    return 0;
}
