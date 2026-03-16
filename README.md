# ZRender_VulkanToy

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

一个面向学习过程的 Vulkan 教学工程。

这个仓库不是通用渲染引擎模板，也不是“只放一个三角形示例”的最小 Demo。它更像一条可回放的学习轨迹：以 `EasyVulkan.github.io` 的课程主线为参考，把 Vulkan 初始化、资源管理、渲染流程和若干进阶主题拆成一系列连续演进的提交，方便沿着提交历史一步步回看工程是怎么长出来的。

当前项目主要在 Windows x64 + MSVC 环境下验证，代码使用 C++20，并尽量保持“看得懂、能对照、方便改”的教学风格。

---

## 为什么这个仓库适合学习

- 使用 C++20 编写，尽量避免为了“工程感”而做过度抽象。
- 以提交历史为主线，可以直接按 `git log --reverse` 的顺序学习。
- 在保留最小可运行示例的同时，逐步补上贴图、离屏渲染、动态渲染、HDR、运行期编译 GLSL 等内容。
- 代码里持续补充中文注释，优先解释“为什么这样写”和“这一步在 Vulkan 流程里的位置”。

---

## 当前覆盖的主题

- Vulkan 实例、设备、交换链与渲染循环
- Render Pass / Framebuffer / Pipeline / Descriptor / Sampler
- Vertex / Index / Uniform / Push Constant
- 2D 贴图、Mipmap、贴图数组
- 离屏渲染、深度测试、延迟渲染
- sRGB、HDR、运行期编译 GLSL
- Visual Studio 与 CMake 双入口构建

---

## 🔧 Requirements

| Dependency | Version | Notes |
|------------|---------|-------|
| Visual Studio 2022 | v143 toolset | 当前主要编译环境 |
| Vulkan SDK | 1.4+ | 需要 `VULKAN_SDK` 环境变量 |
| GLFW3 | 3.4 预编译包 | 当前工程链接的是 MSVC 版 `glfw3dll.lib` |
| GLM | 0.9.9+ | Header-only |
| CMake | 3.24+ | 用于 CLion / Visual Studio CMake 模式 |

---

## 🚀 Quick Start

### Visual Studio

直接打开 `ZRender.sln`，选择 `Debug|x64` 或 `Release|x64` 即可构建和运行。

### CMake

这份仓库同时保留了 `Visual Studio .sln` 和 `CMake` 两套入口，方便对照学习，也方便在 CLion 中直接打开。

`CMakeLists.txt` 默认优先读取环境变量 `VULKAN_SDK`，并把 `GLFW_ROOT`、`GLM_ROOT` 做成了可覆盖变量。由于当前仓库使用的是 MSVC 版 GLFW / shaderc 预编译库，所以在 CLion 中也建议选择 `Visual Studio` 工具链。

如果依赖路径和当前教学环境不同，可以在首次配置时显式传入：

```powershell
cmake --preset vs2022-x64 -DGLFW_ROOT=你的GLFW目录 -DGLM_ROOT=你的GLM目录
```

生成工程：

```powershell
cmake --preset vs2022-x64
```

编译 Debug：

```powershell
cmake --build --preset debug
```

编译 Release：

```powershell
cmake --build --preset release
```

构建完成后，CMake 会自动把 `glfw3.dll`、`shader/`、`image/` 复制到输出目录，方便直接运行。

---

## 📁 Project Layout

- `ZRender/`：主工程源码目录。
- `ZRender/shader/`：示例中使用的 GLSL 与 SPIR-V 文件。
- `ZRender/image/`：贴图、HDR 图等资源。
- `ZRender.sln`：Visual Studio 解决方案入口。
- `CMakeLists.txt`：CMake 工程入口。
- `CMakePresets.json`：预设好的 VS2022 x64 配置。

---

## ▶️ Usage

程序启动后会运行当前阶段的示例内容。

- 可以直接在 Visual Studio 中启动调试。
- 也可以运行构建输出目录中的 `ZRender.exe`。
- 关闭窗口即可退出程序。

---

## ✅ Key Features

- Modern C++20 codebase
- 面向学习过程的 Vulkan 基础封装
- 可按提交历史逐步阅读的演进式项目结构
- 包含贴图、离屏渲染、深度、延迟渲染、HDR 等主题
- 支持运行期编译 GLSL 与 CMake / Visual Studio 双入口构建

---

## 学习建议

- 按提交顺序阅读，观察工程如何一步步扩展。
- 先看 `main.cpp` 的入口变化。
- 再看 `VKBase.h`、`VKBase+.h`、`GlfwGeneral.hpp` 等基础设施如何逐步补齐。
- 遇到渲染效果变化时，再对照 `shader/` 和 `image/` 目录里的资源。

---

## 📄 License (MIT)

本项目使用 MIT License，完整条款见 [LICENSE](LICENSE)。
