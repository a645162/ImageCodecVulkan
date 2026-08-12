#include "exportable.h"

namespace vic {

ExportableBuffer::ExportableBuffer(Context& ctx, VkDeviceSize size) : ctx_(&ctx), size_(size) {
  VkExternalMemoryBufferCreateInfo ext{};
  ext.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
  ext.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;

  VkBufferCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  ci.size = size;
  ci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  ci.pNext = &ext;
  if (vkCreateBuffer(ctx.vkDevice(), &ci, nullptr, &buffer_) != VK_SUCCESS)
    buffer_ = VK_NULL_HANDLE;

  VkMemoryRequirements req{};
  vkGetBufferMemoryRequirements(ctx.vkDevice(), buffer_, &req);
  VkDeviceMemory mem = VK_NULL_HANDLE;
  if (ctx.createExportableMemory(req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mem) == Result::Ok) {
    memory_ = mem;
    vkBindBufferMemory(ctx.vkDevice(), buffer_, memory_, 0);
  }
}

ExportableBuffer::~ExportableBuffer() { destroy(); }

ExportableBuffer::ExportableBuffer(ExportableBuffer&& other) noexcept
    : ctx_(other.ctx_), buffer_(other.buffer_), memory_(other.memory_), size_(other.size_) {
  other.buffer_ = VK_NULL_HANDLE;
  other.memory_ = VK_NULL_HANDLE;
  other.exportedHandle_ = nullptr;
}

ExportableBuffer& ExportableBuffer::operator=(ExportableBuffer&& other) noexcept {
  if (this != &other) {
    destroy();
    ctx_ = other.ctx_;
    buffer_ = other.buffer_;
    memory_ = other.memory_;
    size_ = other.size_;
    other.buffer_ = VK_NULL_HANDLE;
    other.memory_ = VK_NULL_HANDLE;
    other.exportedHandle_ = nullptr;
  }
  return *this;
}

void* ExportableBuffer::exportHandle() {
  if (exportedHandle_) return exportedHandle_;
  VkMemoryGetWin32HandleInfoKHR gi{};
  gi.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
  gi.memory = memory_;
  gi.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
  auto fn = ctx_->vkGetMemoryWin32Handle();
  if (!fn || fn(ctx_->vkDevice(), &gi, &exportedHandle_) != VK_SUCCESS)
    exportedHandle_ = nullptr;
  return exportedHandle_;
}

void ExportableBuffer::destroy() {
  if (memory_) vkFreeMemory(ctx_->vkDevice(), memory_, nullptr);
  if (buffer_) vkDestroyBuffer(ctx_->vkDevice(), buffer_, nullptr);
  memory_ = VK_NULL_HANDLE;
  buffer_ = VK_NULL_HANDLE;
}

ExportableImage::ExportableImage(Context& ctx, uint32_t width, uint32_t height, VkFormat format)
    : ctx_(&ctx), width_(width), height_(height) {
  VkExternalMemoryImageCreateInfo ext{};
  ext.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
  ext.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;

  VkImageCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  ci.imageType = VK_IMAGE_TYPE_2D;
  ci.format = format;
  ci.extent = {width, height, 1};
  ci.mipLevels = 1;
  ci.arrayLayers = 1;
  ci.samples = VK_SAMPLE_COUNT_1_BIT;
  ci.tiling = VK_IMAGE_TILING_OPTIMAL;
  ci.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  ci.pNext = &ext;
  if (vkCreateImage(ctx.vkDevice(), &ci, nullptr, &image_) != VK_SUCCESS)
    image_ = VK_NULL_HANDLE;

  VkMemoryRequirements req{};
  vkGetImageMemoryRequirements(ctx.vkDevice(), image_, &req);
  VkDeviceMemory mem = VK_NULL_HANDLE;
  if (ctx.createExportableMemory(req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mem) == Result::Ok) {
    memory_ = mem;
    vkBindImageMemory(ctx.vkDevice(), image_, memory_, 0);
  }

  if (image_) {
    VkImageViewCreateInfo vci{};
    vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image = image_;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = format;
    vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(ctx.vkDevice(), &vci, nullptr, &view_) != VK_SUCCESS)
      view_ = VK_NULL_HANDLE;
  }
}

ExportableImage::~ExportableImage() { destroy(); }

ExportableImage::ExportableImage(ExportableImage&& other) noexcept
    : ctx_(other.ctx_), image_(other.image_), view_(other.view_), memory_(other.memory_),
      width_(other.width_), height_(other.height_) {
  other.image_ = VK_NULL_HANDLE;
  other.view_ = VK_NULL_HANDLE;
  other.memory_ = VK_NULL_HANDLE;
  other.exportedHandle_ = nullptr;
}

ExportableImage& ExportableImage::operator=(ExportableImage&& other) noexcept {
  if (this != &other) {
    destroy();
    ctx_ = other.ctx_;
    image_ = other.image_;
    view_ = other.view_;
    memory_ = other.memory_;
    width_ = other.width_;
    height_ = other.height_;
    other.image_ = VK_NULL_HANDLE;
    other.view_ = VK_NULL_HANDLE;
    other.memory_ = VK_NULL_HANDLE;
    other.exportedHandle_ = nullptr;
  }
  return *this;
}

void* ExportableImage::exportHandle() {
  if (exportedHandle_) return exportedHandle_;
  VkMemoryGetWin32HandleInfoKHR gi{};
  gi.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
  gi.memory = memory_;
  gi.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
  auto fn = ctx_->vkGetMemoryWin32Handle();
  if (!fn || fn(ctx_->vkDevice(), &gi, &exportedHandle_) != VK_SUCCESS)
    exportedHandle_ = nullptr;
  return exportedHandle_;
}

void ExportableImage::destroy() {
  if (view_) vkDestroyImageView(ctx_->vkDevice(), view_, nullptr);
  if (memory_) vkFreeMemory(ctx_->vkDevice(), memory_, nullptr);
  if (image_) vkDestroyImage(ctx_->vkDevice(), image_, nullptr);
  view_ = VK_NULL_HANDLE;
  memory_ = VK_NULL_HANDLE;
  image_ = VK_NULL_HANDLE;
}

}  // namespace vic
