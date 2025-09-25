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

  void init(Context const& context, VkSurfaceKHR surface);

  void deinit(bool keep_previous_swapchain = false);

  void acquire_next_image();

  void present_and_swap(VkQueue const queue);

  [[nodiscard]]
  VkExtent2D get_surface_size() const noexcept {
    return swapchain_create_info_.imageExtent;
  }

  [[nodiscard]]
  uint32_t get_image_count() const noexcept {
    return image_count_;
  }

  [[nodiscard]]
  VkFormat get_color_format() const noexcept {
    return swap_images_[0u].format;
  }

  [[nodiscard]]
  std::vector<backend::Image> const& get_swap_images() const noexcept {
    return swap_images_;
  }

  [[nodiscard]]
  backend::Image const& get_current_swap_image() const noexcept {
    return swap_images_[current_swap_index_];
  }

  [[nodiscard]]
  SwapSynchronizer_t const& get_current_synchronizer() const noexcept {
    return swap_syncs_[current_swap_index_];
  }

  [[nodiscard]]
  uint32_t const& get_current_swap_index() const noexcept {
    return current_swap_index_;
  }

 private:
  [[nodiscard]]
  VkSurfaceFormat2KHR select_surface_format(VkPhysicalDeviceSurfaceInfo2KHR const* surface_info2) const;

  [[nodiscard]]
  VkPresentModeKHR select_present_mode(VkSurfaceKHR surface, bool use_vsync) const;

 private:
  /* Copy references */
  VkPhysicalDevice gpu_{};
  VkDevice device_{};

  VkSwapchainCreateInfoKHR swapchain_create_info_{};
  VkSwapchainKHR swapchain_{};

  std::vector<backend::Image> swap_images_{};
  std::vector<SwapSynchronizer_t> swap_syncs_{};

  uint32_t image_count_{};
  uint32_t current_swap_index_{};
  uint32_t next_swap_index_{};

  bool need_rebuild_ = true;
};

/* -------------------------------------------------------------------------- */

#endif
