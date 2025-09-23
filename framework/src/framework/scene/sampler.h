#ifndef VKFRAMEWORK_SCENE_SAMPLER_H_
#define VKFRAMEWORK_SCENE_SAMPLER_H_

#include "framework/backend/vk_utils.h" // for VkSamplerCreateInfo..

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

/* -------------------------------------------------------------------------- */

}  // namespace scene

#endif // VKFRAMEWORK_SCENE_SAMPLER_H_
