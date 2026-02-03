#pragma once

#include "VKBase+.h"

using namespace vulkan;

// 继续直接引用交换链图像的尺寸。
// 后面创建每一张交换链图像对应的 framebuffer 时都会用到它。
const VkExtent2D& windowSize = graphicsBase::Base().SwapchainCreateInfo().imageExtent;

namespace easyVulkan
{
    struct renderPassWithFramebuffer
    {
        // 离屏画布这边只需要一个 framebuffer。
        renderPass renderPass;
        framebuffer framebuffer;
    };

    struct renderPassWithFramebuffers
    {
        // 这是屏幕渲染使用的 render pass。
        renderPass renderPass;

        // 传统路径下，每一张交换链图像都对应一个 framebuffer。
        std::vector<framebuffer> framebuffers;
    };

    const renderPassWithFramebuffers& CreateRpwf_Screen()
    {
        // 用静态对象缓存结果，避免重复创建同一套屏幕渲染资源。
        static renderPassWithFramebuffers rpwf;

        // 当前示例只有一个颜色附件，它最终会写入交换链图像。
        VkAttachmentDescription attachmentDescriptions[] = {{
            .format = graphicsBase::Base().SwapchainCreateInfo().imageFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR}};

        // 子通道里唯一的颜色附件索引就是 0。
        VkAttachmentReference attachmentReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

        // 当前 render pass 仍然只包含一个图形子通道。
        VkSubpassDescription subpassDescriptions[] = {{
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &attachmentReference}};

        // 这条依赖负责把外部状态过渡到颜色附件输出阶段。
        VkSubpassDependency subpassDependencies[] = {{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT}};

        // 创建 render pass。
        rpwf.renderPass.Create(attachmentDescriptions, subpassDescriptions, subpassDependencies);

        auto CreateFramebuffers = [] {
            // 交换链里有几张图像，这里就准备几个 framebuffer。
            rpwf.framebuffers.resize(graphicsBase::Base().SwapchainImageCount());

            // 为每一张交换链 image view 创建对应的 framebuffer。
            for (size_t i = 0; i < graphicsBase::Base().SwapchainImageCount(); ++i)
            {
                // 当前 framebuffer 只挂一个颜色附件，也就是这张交换链 image view。
                VkImageView attachment = graphicsBase::Base().SwapchainImageView(static_cast<uint32_t>(i));

                // 利用封装好的辅助接口创建 framebuffer。
                rpwf.framebuffers[i].Create(rpwf.renderPass, attachment, windowSize);
            }
        };

        auto DestroyFramebuffers = [] {
            // 清空容器时会顺带析构内部的 framebuffer 包装对象。
            rpwf.framebuffers.clear();
        };

        // 首次进入时先创建当前交换链对应的 framebuffer 集合。
        CreateFramebuffers();

        // 后续重复调用时直接返回缓存对象，不再重复注册回调。
        ExecuteOnce(rpwf);

        // 交换链重建后需要重新创建 framebuffer。
        graphicsBase::Base().AddCallback_CreateSwapchain(CreateFramebuffers);

        // 交换链销毁前先销毁旧 framebuffer。
        graphicsBase::Base().AddCallback_DestroySwapchain(DestroyFramebuffers);

        return rpwf;
    }

    // 这张颜色附件会被当作离屏画布使用。
    inline colorAttachment ca_canvas;

    const renderPassWithFramebuffer& CreateRpwf_Canvas(VkExtent2D canvasSize = windowSize)
    {
        // 离屏画布这边只创建一个 framebuffer 即可。
        static renderPassWithFramebuffer rpwf;

        // 画布本身既要能当颜色附件来渲染，
        // 也要能在后续被采样，还要支持被 clear 命令写入。
        ca_canvas.Create(
            graphicsBase::Base().SwapchainCreateInfo().imageFormat,
            canvasSize,
            1,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

        // 这张离屏画布开始渲染前要保留原来的内容，
        // 渲染结束后则回到 shader-read-only，方便后面采样到屏幕。
        VkAttachmentDescription attachmentDescription = {
            .format = graphicsBase::Base().SwapchainCreateInfo().imageFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

        // 子通道里唯一的颜色附件索引就是 0。
        VkAttachmentReference attachmentReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

        // 离屏画布 render pass 同样只需要一个图形子通道。
        VkSubpassDescription subpassDescription = {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &attachmentReference};

        // 这里准备两条依赖：
        // 1. 开始渲染前，把“之前被片段着色器采样”的状态切回颜色附件写入；
        // 2. 结束渲染后，再把颜色附件写入结果过渡回片段着色器采样。
        VkSubpassDependency subpassDependencies[2] = {
            {
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask = 0,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
            },
            {
                .srcSubpass = 0,
                .dstSubpass = VK_SUBPASS_EXTERNAL,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
            }};

        // 创建离屏 render pass。
        VkRenderPassCreateInfo renderPassCreateInfo = {
            .attachmentCount = 1,
            .pAttachments = &attachmentDescription,
            .subpassCount = 1,
            .pSubpasses = &subpassDescription,
            .dependencyCount = 2,
            .pDependencies = subpassDependencies};
        rpwf.renderPass.Create(renderPassCreateInfo);

        // 创建离屏 framebuffer，它唯一的附件就是 ca_canvas。
        VkFramebufferCreateInfo framebufferCreateInfo = {
            .renderPass = rpwf.renderPass,
            .attachmentCount = 1,
            .pAttachments = ca_canvas.AddressOfImageView(),
            .width = canvasSize.width,
            .height = canvasSize.height,
            .layers = 1};
        rpwf.framebuffer.Create(framebufferCreateInfo);

        return rpwf;
    }

    void CmdClearCanvas(VkCommandBuffer commandBuffer, VkClearColorValue clearColor)
    {
        // 这条命令要在离屏 render pass 开始前调用。
        VkImageSubresourceRange imageSubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        // 先把画布从 shader-read-only 切到 transfer-dst，准备执行清屏。
        VkImageMemoryBarrier imageMemoryBarrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = ca_canvas.Image(),
            .subresourceRange = imageSubresourceRange};

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &imageMemoryBarrier);

        // 真正执行清屏。
        vkCmdClearColorImage(
            commandBuffer,
            ca_canvas.Image(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            &clearColor,
            1,
            &imageSubresourceRange);

        // 清屏结束后，再切回 shader-read-only。
        // 后续离屏 render pass 开始时，会通过子通道依赖把它正确过渡到 color-attachment-optimal。
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = 0;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &imageMemoryBarrier);
    }

    // 每一张交换链图像都要配一个对应的深度模板附件。
    inline std::vector<depthStencilAttachment> dsas_screenWithDS;

    const renderPassWithFramebuffers& CreateRpwf_ScreenWithDS(VkFormat depthStencilFormat = VK_FORMAT_D24_UNORM_S8_UINT)
    {
        // 这套 render pass + framebuffer 组合专门给带深度测试的屏幕渲染使用。
        static renderPassWithFramebuffers rpwf;

        // 首次调用时记住使用哪一种深度模板格式，后续交换链重建继续沿用它。
        static VkFormat s_depthStencilFormat = depthStencilFormat;

        // 0 号附件是交换链颜色图像，1 号附件是深度模板图像。
        VkAttachmentDescription attachmentDescriptions[2] = {
            {
                .format = graphicsBase::Base().SwapchainCreateInfo().imageFormat,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            },
            {
                .format = s_depthStencilFormat,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = s_depthStencilFormat != VK_FORMAT_S8_UINT ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .stencilLoadOp = s_depthStencilFormat >= VK_FORMAT_S8_UINT ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            }};

        // 子通道里 0 号索引用作颜色附件，1 号索引用作深度模板附件。
        VkAttachmentReference attachmentReferences[2] = {
            {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
            {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},
        };

        // 这条子通道同时输出颜色并执行深度测试。
        VkSubpassDescription subpassDescription = {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = attachmentReferences,
            .pDepthStencilAttachment = attachmentReferences + 1,
        };

        // 深度附件会在 early-fragment-tests 阶段参与 clear 和测试。
        VkSubpassDependency subpassDependency = {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        };

        VkRenderPassCreateInfo renderPassCreateInfo = {
            .attachmentCount = 2,
            .pAttachments = attachmentDescriptions,
            .subpassCount = 1,
            .pSubpasses = &subpassDescription,
            .dependencyCount = 1,
            .pDependencies = &subpassDependency,
        };
        rpwf.renderPass.Create(renderPassCreateInfo);

        auto CreateFramebuffers = [] {
            // 交换链里有几张颜色图像，这里就创建几张深度附件和几套 framebuffer。
            dsas_screenWithDS.resize(graphicsBase::Base().SwapchainImageCount());
            rpwf.framebuffers.resize(graphicsBase::Base().SwapchainImageCount());

            for (auto& depthStencil : dsas_screenWithDS)
            {
                // 深度模板附件只在本次 render pass 内部使用，所以可以声明成 transient attachment。
                depthStencil.Create(s_depthStencilFormat, windowSize, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT);
            }

            VkFramebufferCreateInfo framebufferCreateInfo = {
                .renderPass = rpwf.renderPass,
                .attachmentCount = 2,
                .width = windowSize.width,
                .height = windowSize.height,
                .layers = 1,
            };

            for (size_t i = 0; i < graphicsBase::Base().SwapchainImageCount(); ++i)
            {
                // 每个 framebuffer 都绑定“当前交换链 image view + 对应的深度模板 image view”。
                VkImageView attachments[2] = {
                    graphicsBase::Base().SwapchainImageView(static_cast<uint32_t>(i)),
                    dsas_screenWithDS[i].ImageView(),
                };
                framebufferCreateInfo.pAttachments = attachments;
                rpwf.framebuffers[i].Create(framebufferCreateInfo);
            }
        };

        auto DestroyFramebuffers = [] {
            // 交换链销毁前一起释放深度附件和 framebuffer 容器。
            dsas_screenWithDS.clear();
            rpwf.framebuffers.clear();
        };

        CreateFramebuffers();

        ExecuteOnce(rpwf);
        graphicsBase::Base().AddCallback_CreateSwapchain(CreateFramebuffers);
        graphicsBase::Base().AddCallback_DestroySwapchain(DestroyFramebuffers);
        return rpwf;
    }

    // 这两张颜色附件分别存放法线/深度与颜色/高光度，供合成子通道读取。
    inline colorAttachment ca_deferredToScreen_normalZ;
    inline colorAttachment ca_deferredToScreen_albedoSpecular;

    // 延迟渲染的第一子通道同样需要深度模板附件做深度测试。
    inline depthStencilAttachment dsa_deferredToScreen;

    const renderPassWithFramebuffers& CreateRpwf_DeferredToScreen(VkFormat depthStencilFormat = VK_FORMAT_D24_UNORM_S8_UINT)
    {
        // 这套 render pass 负责“G-Buffer 填充 + 屏幕合成”两步流程。
        static renderPassWithFramebuffers rpwf;

        // 固定记录一次所选的深度模板格式，交换链重建时继续沿用。
        static VkFormat s_depthStencilFormat = depthStencilFormat;

        // 4 个附件分别是：交换链、法线+z、颜色+高光、深度模板。
        VkAttachmentDescription attachmentDescriptions[4] = {
            {
                .format = graphicsBase::Base().SwapchainCreateInfo().imageFormat,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            },
            {
                .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
            {
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
            {
                .format = s_depthStencilFormat,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .stencilLoadOp = s_depthStencilFormat >= VK_FORMAT_S8_UINT ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            }};

        // 第一个子通道往两张 G-Buffer 写数据，并使用深度测试。
        VkAttachmentReference attachmentReferences_subpass0[3] = {
            {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
            {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
            {3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},
        };

        // 第二个子通道把两张 G-Buffer 当输入附件读取，并写回交换链颜色附件。
        VkAttachmentReference attachmentReferences_subpass1[3] = {
            {1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        };

        VkSubpassDescription subpassDescriptions[2] = {
            {
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .colorAttachmentCount = 2,
                .pColorAttachments = attachmentReferences_subpass0,
                .pDepthStencilAttachment = attachmentReferences_subpass0 + 2,
            },
            {
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .inputAttachmentCount = 2,
                .pInputAttachments = attachmentReferences_subpass1,
                .colorAttachmentCount = 1,
                .pColorAttachments = attachmentReferences_subpass1 + 2,
            }};

        // 第一条依赖保证开始 G-Buffer 子通道前深度附件已可写。
        // 第二条依赖保证写完 G-Buffer 后，composition 子通道再读取输入附件。
        VkSubpassDependency subpassDependencies[2] = {
            {
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                .srcAccessMask = 0,
                .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
            },
            {
                .srcSubpass = 0,
                .dstSubpass = 1,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
            }};

        VkRenderPassCreateInfo renderPassCreateInfo = {
            .attachmentCount = 4,
            .pAttachments = attachmentDescriptions,
            .subpassCount = 2,
            .pSubpasses = subpassDescriptions,
            .dependencyCount = 2,
            .pDependencies = subpassDependencies,
        };
        rpwf.renderPass.Create(renderPassCreateInfo);

        auto CreateFramebuffers = [] {
            // 交换链里的每张图像都对应一套 framebuffer。
            rpwf.framebuffers.resize(graphicsBase::Base().SwapchainImageCount());

            // 两张 G-Buffer 都只在 render pass 内部使用，所以带上 transient + input attachment 用途。
            ca_deferredToScreen_normalZ.Create(
                VK_FORMAT_R16G16B16A16_SFLOAT,
                windowSize,
                1,
                VK_SAMPLE_COUNT_1_BIT,
                VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT);
            ca_deferredToScreen_albedoSpecular.Create(
                VK_FORMAT_R8G8B8A8_UNORM,
                windowSize,
                1,
                VK_SAMPLE_COUNT_1_BIT,
                VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT);
            dsa_deferredToScreen.Create(
                s_depthStencilFormat,
                windowSize,
                1,
                VK_SAMPLE_COUNT_1_BIT,
                VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT);

            VkImageView attachments[4] = {
                VK_NULL_HANDLE,
                ca_deferredToScreen_normalZ.ImageView(),
                ca_deferredToScreen_albedoSpecular.ImageView(),
                dsa_deferredToScreen.ImageView(),
            };

            VkFramebufferCreateInfo framebufferCreateInfo = {
                .renderPass = rpwf.renderPass,
                .attachmentCount = 4,
                .pAttachments = attachments,
                .width = windowSize.width,
                .height = windowSize.height,
                .layers = 1,
            };

            for (size_t i = 0; i < graphicsBase::Base().SwapchainImageCount(); ++i)
            {
                // 只有第 0 个附件需要随着交换链图像索引变化，其余三个都是共用附件。
                attachments[0] = graphicsBase::Base().SwapchainImageView(static_cast<uint32_t>(i));
                rpwf.framebuffers[i].Create(framebufferCreateInfo);
            }
        };

        auto DestroyFramebuffers = [] {
            // 交换链销毁前，先释放 G-Buffer 和深度附件，再清空 framebuffer 列表。
            ca_deferredToScreen_normalZ.~colorAttachment();
            ca_deferredToScreen_albedoSpecular.~colorAttachment();
            dsa_deferredToScreen.~depthStencilAttachment();
            rpwf.framebuffers.clear();
        };

        CreateFramebuffers();

        ExecuteOnce(rpwf);
        graphicsBase::Base().AddCallback_CreateSwapchain(CreateFramebuffers);
        graphicsBase::Base().AddCallback_DestroySwapchain(DestroyFramebuffers);
        return rpwf;
    }

    // 若需要把转换后的像素数据再拷回 CPU，可通过这个回调拿到 mip0 数据。
    using callback_copyData_t = void (*)(const void* pData, VkDeviceSize dataSize);

    class fCreateTexture2d_multiplyAlpha
    {
    protected:
        // 最终贴图格式。
        VkFormat format_final = VK_FORMAT_UNDEFINED;

        // 是否要顺手生成 mipmap。
        bool generateMipmap = false;

        // 若非空，就在生成完贴图后把第 0 级数据回传给调用方。
        callback_copyData_t callback_copyData = nullptr;

        // 这套小型 render pass + pipeline 专门用于“把 RGB 乘以 A”。
        renderPass renderPass;
        pipeline pipeline;

        void CmdTransferDataToImage(
            VkCommandBuffer commandBuffer,
            const uint8_t* pImageData,
            VkExtent2D extent,
            VkFormat format_initial,
            imageMemory& imageMemory_conversion,
            VkImage image) const
        {
            // 这里准备两种目标状态：
            // 1. 若后面直接进 color attachment 混色，就切到 COLOR_ATTACHMENT_OPTIMAL；
            // 2. 若后面还要先 blit 一次，就切到 TRANSFER_SRC_OPTIMAL。
            static constexpr imageOperation::imageMemoryBarrierParameterPack imbs[2] = {
                {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
                {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL},
            };

            const VkDeviceSize imageDataSize = VkDeviceSize(FormatInfo(format_initial).sizePerPixel) * extent.width * extent.height;

            // 先把原始像素写进 staging buffer。
            stagingBuffer::BufferData_MainThread(pImageData, imageDataSize);

            // image_copyTo 指 staging buffer 将直接 copy 到哪张图像。
            // image_conversion 指需要参与 blit 的那张“源图像”。
            // image_blitTo 指最终要被渲染成预乘 Alpha 的那张图像。
            VkImage image_copyTo = VK_NULL_HANDLE;
            VkImage image_conversion = VK_NULL_HANDLE;
            VkImage image_blitTo = VK_NULL_HANDLE;

            if (format_initial == format_final)
            {
                // 如果源格式和目标格式相同，就可以直接 copy 到最终图像。
                image_copyTo = image;
            }
            else
            {
                // 若格式不同，优先尝试把 staging buffer 直接别名成线性 tiling 图像。
                image_conversion = stagingBuffer::AliasedImage2d_MainThread(format_initial, extent);

                if (!image_conversion)
                {
                    // 如果别名失败，就创建一张 device-local 中转图像。
                    VkImageCreateInfo imageCreateInfo = {
                        .imageType = VK_IMAGE_TYPE_2D,
                        .format = format_initial,
                        .extent = {extent.width, extent.height, 1},
                        .mipLevels = 1,
                        .arrayLayers = 1,
                        .samples = VK_SAMPLE_COUNT_1_BIT,
                        .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT};

                    imageMemory_conversion.Create(imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                    image_copyTo = image_conversion = imageMemory_conversion.Image();
                }

                // 最终都要再 blit 到真正的目标图像上。
                image_blitTo = image;
            }

            if (image_copyTo)
            {
                // 先把 staging buffer 数据 copy 到 image_copyTo。
                VkBufferImageCopy region = {
                    .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                    .imageExtent = {extent.width, extent.height, 1}};

                imageOperation::CmdCopyBufferToImage(
                    commandBuffer,
                    stagingBuffer::Buffer_MainThread(),
                    image_copyTo,
                    region,
                    {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED},
                    imbs[bool(image_blitTo)]);
            }

            if (image_blitTo)
            {
                if (!image_copyTo)
                {
                    // 走别名路径时，线性 tiling 图像初始布局是 PREINITIALIZED，
                    // 这里先把它切成 transfer-src。
                    VkImageMemoryBarrier imageMemoryBarrier = {
                        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                        nullptr,
                        0,
                        VK_ACCESS_TRANSFER_READ_BIT,
                        VK_IMAGE_LAYOUT_PREINITIALIZED,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        VK_QUEUE_FAMILY_IGNORED,
                        VK_QUEUE_FAMILY_IGNORED,
                        image_conversion,
                        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

                    vkCmdPipelineBarrier(
                        commandBuffer,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        0,
                        0,
                        nullptr,
                        0,
                        nullptr,
                        1,
                        &imageMemoryBarrier);
                }

                // 再把源图像等尺寸 blit 到最终图像上，顺便完成格式转换。
                VkImageBlit region = {
                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                    {{}, {int32_t(extent.width), int32_t(extent.height), 1}},
                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                    {{}, {int32_t(extent.width), int32_t(extent.height), 1}}};

                imageOperation::CmdBlitImage(
                    commandBuffer,
                    image_conversion,
                    image_blitTo,
                    region,
                    {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED},
                    {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
            }
        }

        // 这套 helper pipeline 用一个固定的全屏矩形顶点着色器和“什么都不输出”的片段着色器。
        static constexpr const char* filepath_vert = "shader/RenderToImage2d_NoUV.vert.spv";
        static constexpr const char* filepath_frag = "shader/RenderNothing.frag.spv";

        static VkPipelineShaderStageCreateInfo Ssci_Vert()
        {
            static shaderModule shader;

            if (!shader)
            {
                shader.Create(filepath_vert);
                graphicsBase::Base().AddCallback_DestroyDevice([] { shader.~shaderModule(); });
            }

            return shader.StageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT);
        }

        static VkPipelineShaderStageCreateInfo Ssci_Frag()
        {
            static shaderModule shader;

            if (!shader)
            {
                shader.Create(filepath_frag);
                graphicsBase::Base().AddCallback_DestroyDevice([] { shader.~shaderModule(); });
            }

            return shader.StageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT);
        }

        static VkPipelineLayout PipelineLayout()
        {
            static pipelineLayout pipelineLayout;

            if (!pipelineLayout)
            {
                VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
                pipelineLayout.Create(pipelineLayoutCreateInfo);
                graphicsBase::Base().AddCallback_DestroyDevice([] { pipelineLayout.~pipelineLayout(); });
            }

            return pipelineLayout;
        }

    public:
        fCreateTexture2d_multiplyAlpha() = default;

        fCreateTexture2d_multiplyAlpha(VkFormat format_final, bool generateMipmap, callback_copyData_t callback_copyMipLevel0)
        {
            Instantiate(format_final, generateMipmap, callback_copyMipLevel0);
        }

        fCreateTexture2d_multiplyAlpha(fCreateTexture2d_multiplyAlpha&&) = default;

        texture2d operator()(const char* filepath, VkFormat format_initial) const
        {
            VkExtent2D extent{};
            auto pImageData = texture::LoadFile(filepath, extent, FormatInfo(format_initial));

            if (pImageData)
                return (*this)(pImageData.get(), extent, format_initial);

            return texture2d{};
        }

        texture2d operator()(const uint8_t* pImageData, VkExtent2D extent, VkFormat format_initial) const
        {
            texture2d texture;
            imageMemory imageMemory_conversion;

            // 如果后面要把 mip0 拷回 CPU，就先确保 staging buffer 对源/目标尺寸都够大。
            const uint32_t pixelCount = extent.width * extent.height;
            const VkDeviceSize imageDataSize_initial = VkDeviceSize(FormatInfo(format_initial).sizePerPixel) * pixelCount;
            const VkDeviceSize imageDataSize_final = VkDeviceSize(FormatInfo(format_final).sizePerPixel) * pixelCount;
            if (callback_copyData)
                stagingBuffer::Expand_MainThread(std::max(imageDataSize_initial, imageDataSize_final));

            // 通过局部派生类拿到 texture2d 里受保护的图像/视图成员。
            struct texture2d_local : texture2d
            {
                using texture::imageMemory;
                using texture::imageView;
                using texture2d::extent;
            };

            auto* pTexture = static_cast<texture2d_local*>(&texture);
            pTexture->extent = extent;

            // 先创建最终目标图像。
            const uint32_t mipLevelCount = generateMipmap ? texture::CalculateMipLevelCount(extent) : 1;
            VkImageCreateInfo imageCreateInfo = {
                .imageType = VK_IMAGE_TYPE_2D,
                .format = format_final,
                .extent = {extent.width, extent.height, 1},
                .mipLevels = mipLevelCount,
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT};
            pTexture->imageMemory.Create(imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            VkImage image = pTexture->imageMemory.Image();

            // 先为 mip0 建一个临时 image view，用它挂到 framebuffer 上做一次“预乘 Alpha 渲染”。
            pTexture->imageView.Create(image, VK_IMAGE_VIEW_TYPE_2D, format_final, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
            VkFramebufferCreateInfo framebufferCreateInfo = {
                .renderPass = renderPass,
                .attachmentCount = 1,
                .pAttachments = pTexture->imageView.Address(),
                .width = extent.width,
                .height = extent.height,
                .layers = 1};
            framebuffer framebuffer(framebufferCreateInfo);

            {
                auto& commandBuffer = graphicsBase::Plus().CommandBuffer_Transfer();
                commandBuffer.BeginOneTime();

                // 先把源数据搬进目标图像，并切到 color-attachment-optimal。
                CmdTransferDataToImage(commandBuffer, pImageData, extent, format_initial, imageMemory_conversion, image);

                // 开始那次“把 RGB 乘以 A”的离屏渲染。
                renderPass.CmdBegin(commandBuffer, framebuffer, {{}, extent});
                pipeline.CmdBind(commandBuffer);

                // 视口是动态状态，这里按当前贴图尺寸设置。
                VkViewport viewport = {
                    0.0f,
                    0.0f,
                    float(extent.width),
                    float(extent.height),
                    0.0f,
                    1.0f};
                vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

                // 画一个全屏矩形，真正的“RGB *= A”由混色状态完成。
                vkCmdDraw(commandBuffer, 4, 1, 0, 0);
                renderPass.CmdEnd(commandBuffer);

                if (callback_copyData)
                {
                    // 若调用方要拿回数据，就把乘完 Alpha 的 mip0 再拷回 staging buffer。
                    VkBufferImageCopy region = {
                        .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                        .imageExtent = {extent.width, extent.height, 1}};
                    vkCmdCopyImageToBuffer(
                        commandBuffer,
                        image,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        stagingBuffer::Buffer_MainThread(),
                        1,
                        &region);
                }

                // 若要生成 mipmap 或回读数据，就让图像继续保持 transfer-src；
                // 否则 render pass 已经把它收尾到 shader-read-only。
                if (mipLevelCount > 1 || callback_copyData)
                {
                    imageOperation::CmdGenerateMipmap2d(
                        commandBuffer,
                        image,
                        extent,
                        mipLevelCount,
                        1,
                        {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
                }

                commandBuffer.End();
                graphicsBase::Plus().ExecuteCommandBuffer_Graphics(commandBuffer);
            }

            if (callback_copyData)
            {
                // 把转换后的 mip0 数据交给外部回调处理。
                callback_copyData(stagingBuffer::MapMemory_MainThread(imageDataSize_final), imageDataSize_final);
                stagingBuffer::UnmapMemory_MainThread();
            }

            if (mipLevelCount > 1)
            {
                // 若生成了 mipmap，就把 image view 改成覆盖整条 mip 链。
                pTexture->imageView.~imageView();
                pTexture->imageView.Create(image, VK_IMAGE_VIEW_TYPE_2D, format_final, {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevelCount, 0, 1});
            }

            return texture;
        }

        void Instantiate(VkFormat format_final, bool generateMipmap, callback_copyData_t callback_copyMipLevel0)
        {
            this->format_final = format_final;
            this->generateMipmap = generateMipmap;
            callback_copyData = callback_copyMipLevel0;

            // 这条 render pass 只服务一张颜色附件，并依靠混色状态把 RGB 乘上 A。
            VkAttachmentDescription attachmentDescription = {
                .format = format_final,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

            VkAttachmentReference attachmentReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

            VkSubpassDescription subpassDescription = {
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .colorAttachmentCount = 1,
                .pColorAttachments = &attachmentReference};

            // 若后面还要回读数据或继续生成 mip，则 render pass 结束后保持 transfer-src。
            VkSubpassDependency subpassDependency = {
                .srcSubpass = 0,
                .dstSubpass = VK_SUBPASS_EXTERNAL,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT};

            VkRenderPassCreateInfo renderPassCreateInfo = {
                .attachmentCount = 1,
                .pAttachments = &attachmentDescription,
                .subpassCount = 1,
                .pSubpasses = &subpassDescription};

            if (generateMipmap || callback_copyData)
            {
                attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                renderPassCreateInfo.dependencyCount = 1;
                renderPassCreateInfo.pDependencies = &subpassDependency;
            }

            renderPass.Create(renderPassCreateInfo);

            // 再创建那条专门把 RGB 乘上 A 的图形管线。
            graphicsPipelineCreateInfoPack pipelineCiPack;
            pipelineCiPack.SetPipelineLayout(PipelineLayout());
            pipelineCiPack.SetRenderPass(renderPass);

            const VkPipelineShaderStageCreateInfo shaderStages[] = {
                Ssci_Vert(),
                Ssci_Frag()};
            pipelineCiPack.SetShaderStages(shaderStages);

            pipelineCiPack.inputAssemblyStateCi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            pipelineCiPack.scissors.emplace_back(
                VkOffset2D{},
                VkExtent2D{
                    graphicsBase::Base().PhysicalDeviceProperties().limits.maxFramebufferWidth,
                    graphicsBase::Base().PhysicalDeviceProperties().limits.maxFramebufferHeight});
            pipelineCiPack.multisampleStateCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

            // 混色公式核心是：
            // 新 RGB = 0 * src.rgb + dst.a * dst.rgb，即把原图 RGB 乘上原图 A；
            // 新 A   = 0 * src.a   + 1 * dst.a      ，保持原来的 Alpha 不变。
            pipelineCiPack.colorBlendAttachmentStates.push_back({
                .blendEnable = VK_TRUE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_ZERO,
                .dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask = 0b1111});

            // 视口在真正处理每张贴图时按实际尺寸动态设置。
            pipelineCiPack.dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
            pipelineCiPack.UpdateAllArrays();
            pipeline.Create(pipelineCiPack);
        }
    };

    void BootScreen(const char* imagePath, VkFormat imageFormat)
    {
        // 先把启动图从磁盘读进内存。
        VkExtent2D imageExtent{};
        auto pImageData = texture2d::LoadFile(imagePath, imageExtent, FormatInfo(imageFormat));
        if (!pImageData)
            return;

        // 把像素数据先写进主线程共用的 staging buffer。
        const VkDeviceSize imageDataSize = VkDeviceSize(FormatInfo(imageFormat).sizePerPixel) * imageExtent.width * imageExtent.height;
        stagingBuffer::BufferData_MainThread(pImageData.get(), imageDataSize);

        // 启动画面只做一次提交，所以这里临时准备同步对象和命令缓冲区即可。
        semaphore semaphore_imageIsAvailable;
        fence fence;
        commandBuffer commandBuffer;
        graphicsBase::Plus().CommandPool_Graphics().AllocateBuffers(commandBuffer);

        // 先取一张交换链图像出来，准备把启动图拷进去。
        graphicsBase::Base().SwapImage(semaphore_imageIsAvailable);

        // 开始录制一次性命令缓冲区。
        commandBuffer.BeginOneTime();

        // 拿到交换链图像的尺寸，用来判断是否需要 blit。
        const VkExtent2D swapchainImageSize = graphicsBase::Base().SwapchainCreateInfo().imageExtent;

        // 只要尺寸不同，或者像素格式不同，就不能直接 copy，必须先走 blit。
        const bool blit =
            imageExtent.width != swapchainImageSize.width ||
            imageExtent.height != swapchainImageSize.height ||
            imageFormat != graphicsBase::Base().SwapchainCreateInfo().imageFormat;

        // 若需要自己创建中转图像，就让这个对象托管其生命周期。
        imageMemory imageMemory_boot;

        if (blit)
        {
            // 优先尝试把 staging buffer 直接别名成一张线性 tiling 的源图像。
            VkImage image = stagingBuffer::AliasedImage2d_MainThread(imageFormat, imageExtent);

            if (image)
            {
                // 别名图像初始布局是 PREINITIALIZED，先转成 transfer-src。
                VkImageMemoryBarrier imageMemoryBarrier = {
                    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    nullptr,
                    0,
                    VK_ACCESS_TRANSFER_READ_BIT,
                    VK_IMAGE_LAYOUT_PREINITIALIZED,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_QUEUE_FAMILY_IGNORED,
                    VK_QUEUE_FAMILY_IGNORED,
                    image,
                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

                vkCmdPipelineBarrier(
                    commandBuffer,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    0,
                    0,
                    nullptr,
                    0,
                    nullptr,
                    1,
                    &imageMemoryBarrier);
            }
            else
            {
                // 若不能别名，就创建一张 device-local 中转图像。
                VkImageCreateInfo imageCreateInfo = {
                    .imageType = VK_IMAGE_TYPE_2D,
                    .format = imageFormat,
                    .extent = {imageExtent.width, imageExtent.height, 1},
                    .mipLevels = 1,
                    .arrayLayers = 1,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT};

                imageMemory_boot.Create(imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

                // 先把 staging buffer 里的像素数据 copy 到这张中转图像里。
                VkBufferImageCopy region_copy = {
                    .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                    .imageExtent = imageCreateInfo.extent};

                imageOperation::CmdCopyBufferToImage(
                    commandBuffer,
                    stagingBuffer::Buffer_MainThread(),
                    imageMemory_boot.Image(),
                    region_copy,
                    {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED},
                    {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL});

                // 后面统一从 image 这个句柄继续 blit 到交换链图像。
                image = imageMemory_boot.Image();
            }

            // 用 blit 把启动图缩放/格式转换到当前交换链图像里。
            VkImageBlit region_blit = {
                {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                {{}, {int32_t(imageExtent.width), int32_t(imageExtent.height), 1}},
                {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                {{}, {int32_t(swapchainImageSize.width), int32_t(swapchainImageSize.height), 1}}};

            imageOperation::CmdBlitImage(
                commandBuffer,
                image,
                graphicsBase::Base().SwapchainImage(graphicsBase::Base().CurrentImageIndex()),
                region_blit,
                {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED},
                {VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR},
                VK_FILTER_LINEAR);
        }
        else
        {
            // 若尺寸和格式都完全一致，直接从 staging buffer copy 到交换链图像即可。
            VkBufferImageCopy region_copy = {
                .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                .imageExtent = {imageExtent.width, imageExtent.height, 1}};

            imageOperation::CmdCopyBufferToImage(
                commandBuffer,
                stagingBuffer::Buffer_MainThread(),
                graphicsBase::Base().SwapchainImage(graphicsBase::Base().CurrentImageIndex()),
                region_copy,
                {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED},
                {VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR});
        }

        // 结束命令录制。
        commandBuffer.End();

        // 启动画面提交时，需要等待交换链图像先变为可用。
        VkPipelineStageFlags waitDstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo submitInfo = {
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = semaphore_imageIsAvailable.Address(),
            .pWaitDstStageMask = &waitDstStage,
            .commandBufferCount = 1,
            .pCommandBuffers = commandBuffer.Address()};

        // 提交并等待执行完成。
        graphicsBase::Base().SubmitCommandBuffer_Graphics(submitInfo, fence);
        fence.WaitAndReset();

        // 直接把这张已经写好的交换链图像呈现出去。
        graphicsBase::Base().PresentImage();

        // 释放临时命令缓冲区。
        graphicsBase::Plus().CommandPool_Graphics().FreeBuffers(commandBuffer);
    }
}
