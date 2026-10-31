# File: src/libs/Vulkan.Image.Codec.Png/CMakeLists.txt
# Brief: CMake build configuration.
# Author: Haomin Kong
# Email: a645162@163.com
# Created: 2026-10-31

# VulkanImageCodec.Png —— PNG 编解码（平台无关）。
#
# 容器/CRC/zlib 在 CPU；行滤波（filter + 自适应选择）在 Vulkan compute。
# 与 CUDA 后端保持同一分工与同一套容器语义。
set(VIC_SPV_DIR "${CMAKE_CURRENT_BINARY_DIR}/spv")
set(VIC_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${VIC_SPV_DIR}" "${VIC_GEN_DIR}")

set(VIC_SPV_FILES "")
foreach(s png_filter)
  vic_add_shader_spv("${CMAKE_CURRENT_SOURCE_DIR}/src/kernels/${s}.comp"
                     "${VIC_SPV_DIR}/${s}.spv")
  list(APPEND VIC_SPV_FILES "${VIC_SPV_DIR}/${s}.spv")
endforeach()

set(VIC_SPV_HEADER "${VIC_GEN_DIR}/png_filter_spv_data.h")
vic_spv_to_header("${VIC_SPV_FILES}" "${VIC_SPV_HEADER}")

add_library(VulkanImageCodec.Png STATIC
  src/png_codec.h
  src/png_codec.cpp
  src/png_filter_vk.h
  src/png_filter_vk.cpp
  src/kernels/png_filter.comp
  "${VIC_SPV_HEADER}"
  ${VIC_SPV_FILES})

target_include_directories(VulkanImageCodec.Png PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(VulkanImageCodec.Png PRIVATE
  "${VIC_GEN_DIR}")

target_link_libraries(VulkanImageCodec.Png PUBLIC
  VulkanImageCodec::Common
  Vulkan::Vulkan)

add_library(VulkanImageCodec::Png ALIAS VulkanImageCodec.Png)
