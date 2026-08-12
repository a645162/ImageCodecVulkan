#pragma once
// VulkanImageCodec —— Vulkan compute 结果零拷贝传输到 OpenGL 的 C++ RAII 库。
// 依赖：Vulkan (vcpkg), GL 仅通过本头不暴露；GL 对象以 uint32_t id 传递。
#include <cstdint>
#include <string>
#include <memory>

namespace vic {

enum class Result : int {
  Ok = 0,
  Error = 1,
  UnsupportedExtension = 2,  // 互操作扩展缺失（Vulkan 或 GL）→ 应视为 SKIP
  InitFailed = 3,
};

struct DeviceInfo {
  std::string name;
  uint32_t apiVersion = 0;
  bool supportsExternalMemoryWin32 = false;  // VK_KHR_external_memory_win32 可用
};

// ---- 实现分布：Core / GlInterop / Api（见 src/libs/*）----

class Context;          // Vulkan instance/device/queue（Core）
class ExportableBuffer; // 可导出 VkBuffer（Core）
class ExportableImage;  // 可导出 VkImage（Core）
class Compute;          // compute pipeline + dispatch（Core）
class GlInterop;        // GL 扩展加载 + buffer/texture 导入（GlInterop）
class GlImagePipeline;  // 高层：compute→GL 一步到位（Api）

}  // namespace vic
