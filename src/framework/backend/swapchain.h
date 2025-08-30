#ifndef VKFRAMEWORK_BACKEND_SWAPCHAIN_H
#define VKFRAMEWORK_BACKEND_SWAPCHAIN_H

/* -------------------------------------------------------------------------- */

#include "framework/backend/types.h"
class Context;

/* -------------------------------------------------------------------------- */

class Swapchain {
 public:
  static constexpr uint32_t kPreferredMaxImageCount{ 3u };
  static constexpr bool kUseVSync{ true };

  struct SwapSynchronizer_t {
    VkSemaphore wait_image_semaphore{};
    VkSemaphore signal_present_semaphore{};
  };

 public:
  Swapchain() = default;

  void init(Context const& context, VkSurfaceKHR const surface);

  void deinit();

  uint32_t acquire_next_image();

  void present_and_swap(VkQueue const queue);

  inline VkExtent2D get_surface_size() const {
    return surface_size_;
  }

  inline uint32_t get_image_count() const {
    return image_count_;
  }

  inline VkFormat get_color_format() const {
    return swap_images_[0u].format;
  }

  inline std::vector<backend::Image> const& get_swap_images() const {
    return swap_images_;
  }

  inline backend::Image const& get_current_swap_image() const {
    return swap_images_[current_swap_index_];
  }

  inline SwapSynchronizer_t const& get_current_synchronizer() const {
    return swap_syncs_[current_swap_index_];
  }

  inline uint32_t const& get_current_swap_index() const {
    return current_swap_index_;
  }

 private:
  VkSurfaceFormat2KHR select_surface_format(VkPhysicalDeviceSurfaceInfo2KHR const* surface_info2) const;

  VkPresentModeKHR select_present_mode(bool use_vsync) const;

 private:
  VkPhysicalDevice gpu_{};
  VkDevice device_{};
  VkSurfaceKHR surface_{};
  VkExtent2D surface_size_{};

  VkSwapchainKHR swapchain_{};

  std::vector<backend::Image> swap_images_{};
  std::vector<SwapSynchronizer_t> swap_syncs_{};

  uint32_t image_count_{};
  uint32_t current_swap_index_{};
  uint32_t next_swap_index_{};

  bool need_rebuild_ = false;
};

/* -------------------------------------------------------------------------- */

#endif
