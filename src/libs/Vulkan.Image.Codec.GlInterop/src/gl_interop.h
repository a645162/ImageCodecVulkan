#pragma once
#include <VulkanImageCodec/vic.hpp>

namespace vic {

class GlInterop {
public:
  static Result init();
  static bool available();
  static uint32_t importBuffer(void* handle, uint64_t size);
  static uint32_t importTexture(void* handle, uint32_t w, uint32_t h);
  static void releaseObject(uint32_t obj, bool isTexture);
};

}  // namespace vic
