#ifndef HELLOVK_FRAMEWORK_BACKEND_ALLOCATOR_H
#define HELLOVK_FRAMEWORK_BACKEND_ALLOCATOR_H

/* -------------------------------------------------------------------------- */

#include "framework/common.h"
#include "framework/backend/types.h"

/* -------------------------------------------------------------------------- */

class ResourceAllocator {
 public:
  static constexpr size_t kDefaultStagingBufferSize{ 32u * 1024u * 1024u };
  static constexpr bool kAutoAlignBufferSize{ false };

 public:
  ResourceAllocator() = default;

  ~ResourceAllocator() {}

  void init(VmaAllocatorCreateInfo alloc_create_info);

  void deinit();

  // ----- Buffer -----

  Buffer_t create_buffer(VkDeviceSize const size,
                         VkBufferUsageFlags2KHR const usage,
                         VmaMemoryUsage const memory_usage = VMA_MEMORY_USAGE_AUTO,
                         VmaAllocationCreateFlags const flags = {}) const;

  Buffer_t create_staging_buffer(size_t const bytesize = kDefaultStagingBufferSize, void const* host_data = nullptr, size_t host_data_size = 0);

  template<typename T>
  Buffer_t create_staging_buffer(std::span<T> const& host_data) {
    return create_staging_buffer(sizeof(T) * host_data.size(), host_data.data());
  }

  // ------------------------
  void write_buffer(
    Buffer_t const& dst_buffer, size_t const dst_offset, void const* host_data,
    size_t const host_offset, size_t const bytesize
  );

  void upload_host_to_device(void const* host_data, size_t const bytesize, Buffer_t const& dst_buffer) {
    write_buffer(dst_buffer, 0, host_data, 0, bytesize);
  }
  // ------------------------

  inline
  void destroy_buffer(Buffer_t const& buffer) const {
    vmaDestroyBuffer(allocator_, buffer.buffer, buffer.allocation);
  }

  void clear_staging_buffers();

  // ----- Image -----

  void create_image(VkImageCreateInfo const& image_info, Image_t *image) const;

  /* Create an image with view with identical format. */
  void create_image_with_view(VkImageCreateInfo const& image_info, VkImageViewCreateInfo const& view_info, Image_t *image) const;

  void destroy_image(Image_t *image) const;

 private:
  VkDevice device_{};
  VmaAllocator allocator_{};
  std::vector<Buffer_t> staging_buffers_{};
};

/* -------------------------------------------------------------------------- */

#endif
