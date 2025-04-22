#ifndef HELLO_VK_FRAMEWORK_SCENE_TEXTURE_H_
#define HELLO_VK_FRAMEWORK_SCENE_TEXTURE_H_

extern "C" {
#include <stb/stb_image.h>
}

#include <vulkan/vulkan.h> // for VkSampler..

#include "framework/common.h"

namespace scene {

/* -------------------------------------------------------------------------- */

struct ImageData {
 public:
  static constexpr int32_t kDefaultNumChannels{ 4 }; //

 public:
  ImageData() = default;

  ImageData(stbi_uc const* buffer_data, uint32_t const buffer_size)
    : ImageData()
  {
    load(buffer_data, buffer_size);
  }

  bool load(stbi_uc const* buffer_data, uint32_t const buffer_size) {
    auto pixels_data = stbi_load_from_memory(
      buffer_data,
      static_cast<int32_t>(buffer_size),
      &width,
      &height,
      &channels,
      kDefaultNumChannels
    );
    if (pixels_data) {
      pixels.reset(pixels_data);
    }
    return nullptr != pixels_data;
  }

  void release() {
    pixels.reset();
  }

 public:
  int32_t width{};
  int32_t height{};
  int32_t channels{}; //

  std::unique_ptr<uint8_t, decltype(&stbi_image_free)> pixels{nullptr, stbi_image_free}; //
};

// ----------------------------------------------------------------------------

struct Texture {
 public:
  Texture() = default;

  uint32_t getChannelIndex() const {
    return host_image_index; //
  }

 public:
  uint32_t texture_index{UINT32_MAX}; //
  uint32_t host_image_index{UINT32_MAX};
  VkSampler sampler;
};

/* -------------------------------------------------------------------------- */

}  // namespace scene

#endif // HELLO_VK_FRAMEWORK_SCENE_TEXTURE_H_
