cmake_minimum_required(VERSION 3.16)
project(VulkanImageCodec LANGUAGES CXX)

# Keep all exported include paths rooted at this repository when the codec is
# consumed with add_subdirectory() from an application such as ImageViewerQt.
set(VIC_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# ---- vcpkg toolchain bootstrap（与 ImageCodecOpenCL 相同模式）----
if(WIN32)
  set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "vcpkg triplet" FORCE)
endif()
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "vcpkg toolchain")
  elseif(EXISTS "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "vcpkg toolchain")
  endif()
endif()
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  message(WARNING
    "vcpkg toolchain not located. Pass -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake "
    "or set env VCPKG_ROOT.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
  add_compile_options(/utf-8)
endif()

option(VIC_BUILD_TESTS "Build ctest suite" OFF)
option(VIC_BUILD_EXAMPLES "Build examples" OFF)

# Vulkan 由 vcpkg vulkan port 提供（headers + loader）。
find_package(Vulkan REQUIRED)

# ---- GLSL -> SPIR-V ----
# Vulkan SDK 带 glslc；Linux 发行版通常只提供 glslang-tools（glslangValidator）。
# 两者都能出 SPIR-V，命令行只差 -o 的位置，这里统一抹平——否则「Linux 上连 Core
# 都 configure 不过去」，而编解码与指标本身一行平台代码都没有。
find_program(VIC_GLSLC NAMES glslc
  HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
  PATH_SUFFIXES Bin bin)
if(NOT VIC_GLSLC AND TARGET Vulkan::glslc)
  get_target_property(VIC_GLSLC Vulkan::glslc IMPORTED_LOCATION)
endif()

find_program(VIC_GLSLANG_VALIDATOR NAMES glslangValidator glslang
  HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
  PATH_SUFFIXES Bin bin)

if(VIC_GLSLC)
  set(VIC_SHADER_COMPILER "${VIC_GLSLC}")
  set(VIC_SHADER_MODE glslc)
elseif(VIC_GLSLANG_VALIDATOR)
  set(VIC_SHADER_COMPILER "${VIC_GLSLANG_VALIDATOR}")
  set(VIC_SHADER_MODE glslang)
else()
  message(FATAL_ERROR
    "未找到 GLSL 编译器（glslc 或 glslangValidator）。请安装 Vulkan SDK；"
    "Linux 也可 apt install glslang-tools，或设置 VULKAN_SDK 环境变量。")
endif()
message(STATUS "VIC shader compiler: ${VIC_SHADER_COMPILER} (${VIC_SHADER_MODE})")

# 把 .comp 编译成 .spv。生成物是构建产物，不提交进仓库。
function(vic_add_shader_spv shader out_spv)
  if(VIC_SHADER_MODE STREQUAL "glslc")
    add_custom_command(
      OUTPUT "${out_spv}"
      COMMAND "${VIC_SHADER_COMPILER}" "${shader}" -o "${out_spv}"
      DEPENDS "${shader}"
      VERBATIM)
  else()
    add_custom_command(
      OUTPUT "${out_spv}"
      COMMAND "${VIC_SHADER_COMPILER}" -V "${shader}" -o "${out_spv}"
      DEPENDS "${shader}"
      VERBATIM)
  endif()
endfunction()

# 一组 .spv -> 一个 uint32_t 数组头（供 vkCreateShaderModule 直接使用）。
function(vic_spv_to_header spv_files out_header)
  add_custom_command(
    OUTPUT "${out_header}"
    COMMAND ${CMAKE_COMMAND}
      "-DSPV_FILES=${spv_files}"
      "-DOUT_HEADER=${out_header}"
      -P "${VIC_SOURCE_DIR}/cmake/spv_to_header.cmake"
    DEPENDS ${spv_files}
    VERBATIM)
endfunction()

add_subdirectory(src)
if(VIC_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
