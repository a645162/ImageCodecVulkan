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

add_subdirectory(src)
if(VIC_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
