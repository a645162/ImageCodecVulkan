# ---- shader 编译: glslc（Vulkan SDK）----
find_program(VIC_GLSLC glslc
  HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
  PATH_SUFFIXES Bin bin)
if(NOT VIC_GLSLC AND TARGET Vulkan::glslc)
  get_target_property(VIC_GLSLC Vulkan::glslc IMPORTED_LOCATION)
endif()
if(NOT VIC_GLSLC)
  message(FATAL_ERROR "glslc 未找到。请安装 Vulkan SDK 或设置 VULKAN_SDK 环境变量。")
endif()

set(VIC_SPV_DIR "${CMAKE_CURRENT_BINARY_DIR}/spv")
set(VIC_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(VIC_SPV_HEADER "${VIC_GEN_DIR}/vic_spv_data.h")
file(MAKE_DIRECTORY "${VIC_SPV_DIR}" "${VIC_GEN_DIR}")

set(VIC_SHADERS gradient_buffer gradient_texture)
set(VIC_SPV_FILES "")
foreach(s ${VIC_SHADERS})
  add_custom_command(
    OUTPUT "${VIC_SPV_DIR}/${s}.spv"
    COMMAND ${VIC_GLSLC} "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${s}.comp"
            -o "${VIC_SPV_DIR}/${s}.spv"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${s}.comp")
  list(APPEND VIC_SPV_FILES "${VIC_SPV_DIR}/${s}.spv")
endforeach()

add_custom_command(
  OUTPUT "${VIC_SPV_HEADER}"
  COMMAND ${CMAKE_COMMAND}
    "-DSPV_FILES=${VIC_SPV_FILES}"
    "-DOUT_HEADER=${VIC_SPV_HEADER}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/spv_to_header.cmake"
  DEPENDS ${VIC_SPV_FILES})
list(APPEND VIC_SPV_FILES "${VIC_SPV_HEADER}")

add_library(VulkanImageCodec.Core STATIC
  src/context.cpp
  src/exportable.cpp
  src/compute.cpp
  ${VIC_SPV_FILES})
target_include_directories(VulkanImageCodec.Core PUBLIC
  "${VIC_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${VIC_GEN_DIR}"
  # vic.hpp 为 umbrella 头，Core/GlInterop/Api 的 .cpp 内部也经它编译；
  # 故把兄弟库源码目录也纳入 PUBLIC include（GlInterop/Api 经 PUBLIC 链接 Core
  # 自动继承，消费者经 Api 同样获得全部目录）。
  "${VIC_SOURCE_DIR}/src/libs/Vulkan.Image.Codec.GlInterop/src"
  "${VIC_SOURCE_DIR}/src/libs/Vulkan.Image.Codec.Api/src")
target_link_libraries(VulkanImageCodec.Core PUBLIC Vulkan::Vulkan)
add_library(VulkanImageCodec::Core ALIAS VulkanImageCodec.Core)
