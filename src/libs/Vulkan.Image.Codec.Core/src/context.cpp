#include "context.h"
#include <vector>
#include <cstring>

namespace vic {
namespace {

bool pickPhysicalDevice(VkInstance inst, VkPhysicalDevice& out, DeviceInfo& info,
                        uint32_t& computeFamily, bool& externalWin32) {
  uint32_t n = 0;
  vkEnumeratePhysicalDevices(inst, &n, nullptr);
  if (n == 0) return false;
  std::vector<VkPhysicalDevice> devs(n);
  vkEnumeratePhysicalDevices(inst, &n, devs.data());

  auto score = [](VkPhysicalDevice d) {
    VkPhysicalDeviceProperties p;
    vkGetPhysicalDeviceProperties(d, &p);
    return p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 2
         : p.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 1 : 0;
  };

  for (int pass = 2; pass >= 0; --pass) {
    for (auto d : devs) {
      if (score(d) != pass) continue;
      VkPhysicalDeviceProperties p;
      vkGetPhysicalDeviceProperties(d, &p);
      uint32_t qn = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(d, &qn, nullptr);
      std::vector<VkQueueFamilyProperties> qf(qn);
      vkGetPhysicalDeviceQueueFamilyProperties(d, &qn, qf.data());
      int bestQ = -1;
      for (uint32_t i = 0; i < qn; ++i)
        if (qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT) { bestQ = static_cast<int>(i); break; }
      if (bestQ < 0) continue;

      uint32_t en = 0;
      vkEnumerateDeviceExtensionProperties(d, nullptr, &en, nullptr);
      std::vector<VkExtensionProperties> exts(en);
      vkEnumerateDeviceExtensionProperties(d, nullptr, &en, exts.data());
      bool extWin32 = false;
      for (auto& e : exts)
        if (std::strcmp(e.extensionName, VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME) == 0)
          extWin32 = true;

      out = d;
      info.name = p.deviceName;
      info.apiVersion = p.apiVersion;
      info.supportsExternalMemoryWin32 = extWin32;
      computeFamily = static_cast<uint32_t>(bestQ);
      externalWin32 = extWin32;
      return true;
    }
  }
  return false;
}

}  // namespace

Context::~Context() { destroy(); }

Context::Context(Context&& other) noexcept { *this = std::move(other); }

Context& Context::operator=(Context&& other) noexcept {
  if (this != &other) {
    destroy();
    instance_ = other.instance_;
    physicalDevice_ = other.physicalDevice_;
    device_ = other.device_;
    queue_ = other.queue_;
    queueFamily_ = other.queueFamily_;
    info_ = std::move(other.info_);
    vkGetMemoryWin32Handle_ = other.vkGetMemoryWin32Handle_;
    extBufferProps_ = other.extBufferProps_;
    imageFormatProps2_ = other.imageFormatProps2_;
    enabledExternalWin32_ = other.enabledExternalWin32_;
    other.instance_ = VK_NULL_HANDLE;
    other.physicalDevice_ = VK_NULL_HANDLE;
    other.device_ = VK_NULL_HANDLE;
    other.queue_ = VK_NULL_HANDLE;
    other.vkGetMemoryWin32Handle_ = nullptr;
    other.extBufferProps_ = nullptr;
    other.imageFormatProps2_ = nullptr;
    other.enabledExternalWin32_ = false;
  }
  return *this;
}

Result Context::init() {
  VkApplicationInfo app{};
  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.pApplicationName = "VulkanImageCodec";
  app.apiVersion = VK_API_VERSION_1_0;

  // VK_KHR_external_memory_capabilities 是设备级 VK_KHR_external_memory 的声明依赖，
  // 提供 vkGetPhysicalDeviceExternalBuffer/ImageFormatProperties(KHR) 导出能力查询。
  // VK_KHR_get_physical_device_properties2 提供 vkGetPhysicalDeviceImageFormatProperties2KHR
  // （不启用则 loader 返回 null，image 导出能力查询失效）。
  std::vector<const char*> instExts;
  instExts.push_back(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME);
  instExts.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

  VkInstanceCreateInfo ici{};
  ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  ici.pApplicationInfo = &app;
  ici.enabledExtensionCount = static_cast<uint32_t>(instExts.size());
  ici.ppEnabledExtensionNames = instExts.data();
  if (vkCreateInstance(&ici, nullptr, &instance_) != VK_SUCCESS)
    return Result::InitFailed;

  // vcpkg vulkan-1.lib 不导出这两个 -KHR 符号，经 vkGetInstanceProcAddr 运行时解析。
  extBufferProps_ = reinterpret_cast<PFN_vkGetPhysicalDeviceExternalBufferPropertiesKHR>(
      vkGetInstanceProcAddr(instance_, "vkGetPhysicalDeviceExternalBufferPropertiesKHR"));
  imageFormatProps2_ = reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties2KHR>(
      vkGetInstanceProcAddr(instance_, "vkGetPhysicalDeviceImageFormatProperties2KHR"));

  bool externalWin32 = false;
  if (!pickPhysicalDevice(instance_, physicalDevice_, info_, queueFamily_, externalWin32)) {
    vkDestroyInstance(instance_, nullptr);
    instance_ = VK_NULL_HANDLE;
    return Result::InitFailed;
  }
  enabledExternalWin32_ = externalWin32;

  float priority = 1.0f;
  VkDeviceQueueCreateInfo dq{};
  dq.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  dq.queueFamilyIndex = queueFamily_;
  dq.queueCount = 1;
  dq.pQueuePriorities = &priority;

  std::vector<const char*> devExts;
  if (enabledExternalWin32_) {
    devExts.push_back(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
    devExts.push_back(VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME);
  }

  VkDeviceCreateInfo dci{};
  dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  dci.queueCreateInfoCount = 1;
  dci.pQueueCreateInfos = &dq;
  dci.enabledExtensionCount = static_cast<uint32_t>(devExts.size());
  dci.ppEnabledExtensionNames = devExts.empty() ? nullptr : devExts.data();
  if (vkCreateDevice(physicalDevice_, &dci, nullptr, &device_) != VK_SUCCESS) {
    vkDestroyInstance(instance_, nullptr);
    instance_ = VK_NULL_HANDLE;
    return Result::InitFailed;
  }

  vkGetDeviceQueue(device_, queueFamily_, 0, &queue_);
  vkGetMemoryWin32Handle_ = reinterpret_cast<PFN_vkGetMemoryWin32HandleKHR>(
      vkGetDeviceProcAddr(device_, "vkGetMemoryWin32HandleKHR"));
  return Result::Ok;
}

void Context::destroy() {
  if (device_) vkDestroyDevice(device_, nullptr);
  if (instance_) vkDestroyInstance(instance_, nullptr);
  device_ = VK_NULL_HANDLE;
  instance_ = VK_NULL_HANDLE;
}

Result Context::createExportableMemory(const VkMemoryRequirements& req,
                                       VkMemoryPropertyFlags props,
                                       bool exportable,
                                       VkDeviceMemory& out) const {
  VkPhysicalDeviceMemoryProperties mp{};
  vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &mp);

  // 导出内存类型校验（Vulkan 1.4 机制）：
  // 资源在创建时已通过 VkExternalMemory(Buffer|Image)CreateInfo::handleTypes
  // 声明导出句柄类型，规范保证 vkGet(Buffer|Image)MemoryRequirements 返回的
  // memoryTypeBits 已过滤为"仅包含支持该句柄类型的 memory type"
  // （见规范 External Memory 章节：不支持句柄类型的类型会被过滤掉）。
  // 因此从 req.memoryTypeBits 中选取的类型天然满足导出句柄的类型要求
  // （覆盖旧 VUID-VkExportMemoryAllocateInfo-handleTypes-00656）。
  // exportable 为按 VUID-09860 对具体资源查询 VkExternalBufferProperties /
  // VkExternalImageFormatProperties 得到的 EXPORTABLE_BIT 支持性结果，
  // 仅在该资源确实可导出时才链入 VkExportMemoryAllocateInfo。
  int best = -1;
  for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
    if ((req.memoryTypeBits & (1u << i)) == 0) continue;
    if ((mp.memoryTypes[i].propertyFlags & props) != props) continue;
    best = static_cast<int>(i);
    if (mp.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) break;
  }
  if (best < 0) return Result::Error;

  VkMemoryAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  ai.allocationSize = req.size;
  ai.memoryTypeIndex = static_cast<uint32_t>(best);

  VkExportMemoryAllocateInfo exportInfo{};
  if (enabledExternalWin32_ && exportable) {
    exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    exportInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    ai.pNext = &exportInfo;
  }

  if (vkAllocateMemory(device_, &ai, nullptr, &out) != VK_SUCCESS)
    return Result::Error;
  return Result::Ok;
}

}  // namespace vic
