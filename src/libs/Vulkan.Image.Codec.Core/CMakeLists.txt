add_library(VulkanImageCodec.Core STATIC
  src/context.cpp
  src/exportable.cpp)
target_include_directories(VulkanImageCodec.Core PUBLIC
  "${CMAKE_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(VulkanImageCodec.Core PUBLIC Vulkan::Vulkan)
add_library(VulkanImageCodec::Core ALIAS VulkanImageCodec.Core)
