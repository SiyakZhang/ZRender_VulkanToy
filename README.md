# ZRender

这是一个按 `EasyVulkan.github.io` 主线课程逐课重建的 Vulkan 学习工程。

当前分支的目标不是一次性写完，而是把从 `Ch1-0` 开始的每个课程节点都整理成独立提交，方便按提交顺序学习。

## 当前进度

- `Ch1-0 准备工作`：建立工程骨架、公共头文件、基础编译配置。

## 环境准备

- Windows 10/11
- Visual Studio 2022
- Vulkan SDK
- GLFW
- GLM

## 目录说明

- `ZRender/`：主工程源码目录。
- `ZRender.sln`：Visual Studio 解决方案文件。

## 学习方式

- 按 commit 顺序阅读。
- 先看 `main.cpp` 的入口变化。
- 再看 `VKBase.h`、`GlfwGeneral.hpp` 等基础设施如何逐课扩展。

后续每一课都会尽量补充中文注释，优先解释“为什么这样写”和“这一段在 Vulkan 初始化流程中的位置”。
