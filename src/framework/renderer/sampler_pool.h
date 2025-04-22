#ifndef HELLOVK_FRAMEWORK_RENDERER_SAMPLER_POOL_H_
#define HELLOVK_FRAMEWORK_RENDERER_SAMPLER_POOL_H_

#include "framework/common.h"

/* -------------------------------------------------------------------------- */

class SamplerPool {
 public:
  SamplerPool() = default;

  ~SamplerPool() {
    assert( device_ == VK_NULL_HANDLE );
  }

  void init(VkDevice device) {
    device_ = device;
  }

  void deinit() {
    for (auto const& [_, sampler] : map_) {
      vkDestroySampler(device_, sampler, nullptr);
    }
    map_.clear();
    *this = {};
  }

  VkSampler getSampler(VkSamplerCreateInfo const& info) {
    if (auto it = map_.find(info); it != map_.end()) {
      return it->second;
    }
    VkSampler sampler = createSampler(info);
    map_[info] = sampler;
    return sampler;
  }

  void destroySampler(VkSampler sampler) {
    for (auto it = map_.begin(); it != map_.end(); ) {
      if (it->second == sampler) {
        vkDestroySampler(device_, it->second, nullptr);
        it = map_.erase(it);
      } else {
        ++it;
      }
    }
  }

 private:
  struct InfoHash {
    size_t operator()(VkSamplerCreateInfo const& info) const {
      size_t h(0);
      h = utils::HashCombine(h, info.magFilter);
      h = utils::HashCombine(h, info.minFilter);
      h = utils::HashCombine(h, info.mipmapMode);
      h = utils::HashCombine(h, info.addressModeU);
      h = utils::HashCombine(h, info.addressModeV);
      h = utils::HashCombine(h, info.addressModeW);
      h = utils::HashCombine(h, info.mipLodBias);
      h = utils::HashCombine(h, info.anisotropyEnable);
      h = utils::HashCombine(h, info.maxAnisotropy);
      h = utils::HashCombine(h, info.compareEnable);
      h = utils::HashCombine(h, info.compareOp);
      h = utils::HashCombine(h, info.minLod);
      h = utils::HashCombine(h, info.maxLod);
      h = utils::HashCombine(h, info.borderColor);
      h = utils::HashCombine(h, info.unnormalizedCoordinates);
      return h;
    }
  };

  struct InfoKeyEqual {
    bool operator()(VkSamplerCreateInfo const& lhs, VkSamplerCreateInfo const& rhs) const {
      return memcmp(&lhs, &rhs, sizeof(VkSamplerCreateInfo)) == 0;
    }
  };

  VkSampler createSampler(VkSamplerCreateInfo info) const {
    assert( device_ != VK_NULL_HANDLE );
    VkSampler sampler{};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    CHECK_VK( vkCreateSampler(device_, &info, nullptr, &sampler) );
    return sampler;
  }

 private:
  VkDevice device_{};

  std::unordered_map<VkSamplerCreateInfo, VkSampler, InfoHash, InfoKeyEqual> map_{};
};

/* -------------------------------------------------------------------------- */

#endif //  HELLOVK_FRAMEWORK_RENDERER_SAMPLER_POOL_H_
