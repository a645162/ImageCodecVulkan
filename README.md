# ImageCodecVulkan

Vulkan compute 结果**不出显存**直接传输到 OpenGL 的零拷贝互操作库（Windows）。

## 特性
- `VulkanImageCodec::Core`：Vulkan 设备/上下文、可导出 Buffer/Image、compute pipeline。
- `VulkanImageCodec::GlInterop`：`wglGetProcAddress` 手写加载 `GL_EXT_memory_object(_win32)`，将 win32 handle 导入 GL buffer/texture。
- `VulkanImageCodec::Api`：`GlImagePipeline` 一步完成 compute→GL。
- 全程 CPU 零搬运：Vulkan `VK_KHR_external_memory_win32` 导出 → GL `glImportMemoryWin32HandleEXT` 导入同一块显存。

## 依赖
- vcpkg：`vulkan`（headers + loader）
- Vulkan SDK（提供构建期 `glslc`，用于编译 GLSL→SPIR-V）
- 系统：`opengl32.lib` / `user32.lib` / `gdi32.lib`
- 驱动需支持 `GL_EXT_memory_object` / `GL_EXT_memory_object_win32`（NVIDIA/AMD 现代桌面驱动均支持）

## 构建
```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVIC_BUILD_TESTS=ON
cmake --build build --config Debug
```

## 测试
```bash
ctest --test-dir build -C Debug --output-on-failure   # 或
python tests/run_test.py
```
退出码约定：0=PASS、2=SKIP（环境不支持扩展）、1=FAIL。

## 结构
```
include/VulkanImageCodec/vic.hpp   # 公共头（全部对外类声明）
src/libs/Vulkan.Image.Codec.Core/  # 设备/可导出资源/compute
src/libs/Vulkan.Image.Codec.GlInterop/ # GL 扩展加载 + 导入
src/libs/Vulkan.Image.Codec.Api/   # GlImagePipeline 高层封装
tests/                             # C++ 集成测试 + Python 驱动
```
