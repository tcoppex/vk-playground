#ifndef HELLO_VK_FRAMEWORK_SCENE_TEXTURE_H_
#define HELLO_VK_FRAMEWORK_SCENE_TEXTURE_H_

#include <vulkan/vulkan.h> // for VkSampler..

namespace scene {

/* -------------------------------------------------------------------------- */

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
