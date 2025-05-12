# ZRender_VulkanToy

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A C++20 toy project for learning Vulkan graphics programming. Minimalist design for educational purposes.Only tested 64-bit.
---

## 🔧 Dependencies

| Dependency | Version      | Installation Guide                      |
|------------|--------------|----------------------------------------|
| Vulkan SDK | 1.3+         | [LunarG Vulkan SDK](https://vulkan.lunarg.com/) |
| GLM        | 0.9.9+       | Header-only (`#include <glm/glm.hpp>`) |
| GLFW3      | 3.3+         | System package or vcpkg                |

### 🐧 Ubuntu
```bash
sudo apt install libglfw3-dev libglm-dev
```

### 🍎 macOS (Homebrew)
```bash
brew install glfw glm
```

### 🪟 Windows (vcpkg)
```powershell
vcpkg install glfw3 glm
```

---

## 🛠️ Build Instructions

```bash
# Clone with submodules
git clone https://github.com/yourname/ZRender_VulkanToy.git
cd ZRender_VulkanToy

# Create build directory
mkdir build && cd build

# Configure (Linux/macOS)
cmake .. -DCMAKE_BUILD_TYPE=Release

# Configure (Windows with vcpkg)
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build . --config Release --parallel
```

---

## ▶️ Usage

After successful build:
```bash
./ZRender_VulkanToy
```

Press `Esc` to exit or close the window.

---

## 📄 License (MIT)

Copyright (c) 2023-2024 YourName

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

**Full license available in [LICENSE](LICENSE) file.**
```

---

### ✅ Key Features
- Modern C++20 codebase
- Cross-platform rendering core
- Minimal abstraction approach
- Immediate mode rendering toy
- Clean shader integration template

This template prioritizes easy copy-pasting for experimentation while maintaining proper module separation for learning purposes.