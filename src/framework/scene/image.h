#ifndef HELLO_VK_FRAMEWORK_SCENE_IMAGE_H
#define HELLO_VK_FRAMEWORK_SCENE_IMAGE_H

#include "framework/common.h"
#include "stb/stb_image.h"

namespace scene {

/* -------------------------------------------------------------------------- */

  // (wip)
struct Image {
  static constexpr int kDefaultNumChannels{ 4 }; //

  int width{};
  int height{};
  int channels{}; //

  std::unique_ptr<uint8_t, decltype(&stbi_image_free)> pixels{nullptr, stbi_image_free}; //

  uint32_t texture_index{UINT32_MAX};

  Image() = default;

  void clear_pixels() {
    pixels.reset();
  }
};

/* -------------------------------------------------------------------------- */

}  // namespace scene

#endif // HELLO_VK_FRAMEWORK_SCENE_IMAGE_H
