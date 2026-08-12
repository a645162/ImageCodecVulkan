#pragma once
#include <VulkanImageCodec/vic.hpp>
// 本 SDK 的 vk_platform.h 不再自动定义 VK_USE_PLATFORM_WIN32_KHR，
// 需显式声明后 vulkan.h 才会包含 vulkan_win32.h（提供 Win32 handle 类型）。
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR 1
#endif
#include <vulkan/vulkan.h>
#include <vector>

namespace vic {

class Context {
public:
  Context() = default;
  ~Context();
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;
  Context(Context&& other) noexcept;
  Context& operator=(Context&& other) noexcept;

  Result init();

  const DeviceInfo& deviceInfo() const { return info_; }
  VkInstance vkInstance() const { return instance_; }
  VkPhysicalDevice vkPhysicalDevice() const { return physicalDevice_; }
  VkDevice vkDevice() const { return device_; }
  VkQueue vkQueue() const { return queue_; }
  uint32_t queueFamilyIndex() const { return queueFamily_; }
  PFN_vkGetMemoryWin32HandleKHR vkGetMemoryWin32Handle() const { return vkGetMemoryWin32Handle_; }
  // VK_KHR_external_memory_capabilities / VK_KHR_get_physical_device_properties2：
  // vcpkg 的 vulkan-1.lib 不导出这两个 -KHR 符号，只能经 vkGetInstanceProcAddr 运行时解析。
  PFN_vkGetPhysicalDeviceExternalBufferPropertiesKHR
  vkPhysicalDeviceExternalBufferProperties() const { return extBufferProps_; }
  PFN_vkGetPhysicalDeviceImageFormatProperties2KHR
  vkPhysicalDeviceImageFormatProperties2() const { return imageFormatProps2_; }

  Result createExportableMemory(const VkMemoryRequirements& req,
                                VkMemoryPropertyFlags props,
                                bool exportable,
                                VkDeviceMemory& out) const;

private:
  void destroy();

  VkInstance instance_ = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue queue_ = VK_NULL_HANDLE;
  uint32_t queueFamily_ = 0;
  DeviceInfo info_;
  PFN_vkGetMemoryWin32HandleKHR vkGetMemoryWin32Handle_ = nullptr;
  PFN_vkGetPhysicalDeviceExternalBufferPropertiesKHR extBufferProps_ = nullptr;
  PFN_vkGetPhysicalDeviceImageFormatProperties2KHR imageFormatProps2_ = nullptr;
  bool enabledExternalWin32_ = false;
};

}  // namespace vic
