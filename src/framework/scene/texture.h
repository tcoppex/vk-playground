#ifndef VKFRAMEWORK_SCENE_TEXTURE_H_
#define VKFRAMEWORK_SCENE_TEXTURE_H_

#include <vulkan/vulkan.h> // for VkSamplerCreateInfo..

namespace scene {

/* -------------------------------------------------------------------------- */

struct Sampler {
  Sampler() = default;

  Sampler(VkSamplerCreateInfo _info)
    : info(_info)
    , set_(true)
  {}

  bool use_default() const {
    return !set_;
  }

  // (should be changed to not use Vulkan)
  VkSamplerCreateInfo info{}; //

 private:
  bool set_{};
};

// ----------------------------------------------------------------------------

struct Texture {
 public:
  Texture() = default;

  Texture(uint32_t _host_image_index)
    : host_image_index(_host_image_index)
  {}

  Texture(uint32_t _host_image_index, Sampler _sampler)
    : host_image_index(_host_image_index)
    , sampler(_sampler)
  {}

  uint32_t channel_index() const {
    return host_image_index;
  }

  uint32_t host_image_index{UINT32_MAX};
  Sampler sampler{};
};

/* -------------------------------------------------------------------------- */

}  // namespace scene

#endif // VKFRAMEWORK_SCENE_TEXTURE_H_
