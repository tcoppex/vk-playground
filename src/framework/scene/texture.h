#ifndef VKFRAMEWORK_SCENE_TEXTURE_H_
#define VKFRAMEWORK_SCENE_TEXTURE_H_

#include <vulkan/vulkan.h> // for VkSampler..

namespace scene {

/* -------------------------------------------------------------------------- */

struct Texture {
 public:
  Texture() = default;

  Texture(uint32_t _host_image_index, VkSampler _sampler)
    : host_image_index(_host_image_index)
    , sampler(_sampler)
  {}

  uint32_t channel_index() const {
    return host_image_index;
  }

  uint32_t host_image_index{UINT32_MAX};
  VkSampler sampler; //
};

/* -------------------------------------------------------------------------- */

}  // namespace scene

#endif // VKFRAMEWORK_SCENE_TEXTURE_H_
