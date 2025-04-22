#ifndef HELLO_VK_FRAMEWORK_SCENE_TEXTURE_H_
#define HELLO_VK_FRAMEWORK_SCENE_TEXTURE_H_

#include "framework/common.h"
#include "stb/stb_image.h"

namespace scene {

/* -------------------------------------------------------------------------- */

// (wip)
struct Texture {
  static constexpr int kDefaultNumChannels{ 4 }; //

  int width{};
  int height{};
  int channels{}; //

  std::unique_ptr<uint8_t, decltype(&stbi_image_free)> pixels{nullptr, stbi_image_free}; //

  uint32_t texture_index{UINT32_MAX};

  Texture() = default;

  void clear_pixels() {
    pixels.reset();
  }
};

/* -------------------------------------------------------------------------- */

}  // namespace scene

#endif // HELLO_VK_FRAMEWORK_SCENE_TEXTURE_H_
