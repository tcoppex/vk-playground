#include "framework/backend/swapchain.h"
#include "framework/backend/context.h"

/* -------------------------------------------------------------------------- */

namespace {

bool IsSwapchainInvalid(VkResult const result, std::string_view msg) {
  switch (result) {
    case VK_SUCCESS:
      return false;

    case VK_SUBOPTIMAL_KHR:
      LOGW("The current swapchain is suboptimal ({}).", msg);
      return false;

    case VK_ERROR_OUT_OF_DATE_KHR:
      LOGW("[TODO] The swapchain need to be rebuilt ({}).", msg);
      return true;

    default:
      LOGE("{} : swapchain image issue.", msg);
     return true;
  }
}

}

/* -------------------------------------------------------------------------- */

void Swapchain::init(Context const& context, VkSurfaceKHR surface) {
  LOG_CHECK(VK_NULL_HANDLE != surface);
  LOG_CHECK(vkGetPhysicalDeviceSurfaceCapabilities2KHR);
  gpu_ = context.physical_device();
  device_ = context.device();

  // ---------

  /* Retrieve the GPU's capabilities for this surface. */
  VkPhysicalDeviceSurfaceInfo2KHR const surface_info2{
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
    .surface = surface,
  };
  VkSurfaceCapabilities2KHR capabilities2{
    .sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR
  };
  vkGetPhysicalDeviceSurfaceCapabilities2KHR(gpu_, &surface_info2, &capabilities2);
  auto const surface_capabilities = capabilities2.surfaceCapabilities;

  // ---------

  /* Configure the swapchain creation info. */
  if ((swapchain_ == VK_NULL_HANDLE) && (swapchain_create_info_.oldSwapchain == VK_NULL_HANDLE))
  {
    // [we do not need to repeat this step when we kept the previous swapchain]

    /* Determine the number of image to use in the swapchain. */
    uint32_t const min_image_count{ surface_capabilities.minImageCount };
    uint32_t const preferred_image_count{ std::max(kPreferredMaxImageCount, min_image_count) };
    uint32_t const max_image_count{
      (surface_capabilities.maxImageCount == 0u) ? preferred_image_count
                                                 : surface_capabilities.maxImageCount
    };
    image_count_ = std::clamp(preferred_image_count, min_image_count, max_image_count);
    LOGD("image count : {}", image_count_);

    /* Select best swap surface format and present mode. */
    auto const surface_format = select_surface_format(&surface_info2).surfaceFormat;

    swapchain_create_info_ = VkSwapchainCreateInfoKHR{
      .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .pNext            = nullptr,
      .flags            = VkSwapchainCreateFlagsKHR{},
      .surface          = VK_NULL_HANDLE,
      .minImageCount    = image_count_,
      .imageFormat      = surface_format.format,
      .imageColorSpace  = surface_format.colorSpace,
      .imageExtent      = {},
      .imageArrayLayers = 1u,
      .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                        | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                        ,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0u,
      .pQueueFamilyIndices = nullptr,
      .preTransform     = surface_capabilities.currentTransform,
      .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode      = select_present_mode(surface, kUseVSync),
      .clipped          = VK_TRUE,
      .oldSwapchain     = VK_NULL_HANDLE,
    };
  }

  swapchain_create_info_.surface      = surface;
  swapchain_create_info_.imageExtent  = surface_capabilities.currentExtent;
  swapchain_create_info_.oldSwapchain = swapchain_;

  // ---------

  /* Create the swapchain image. */
  swapchain_ = VK_NULL_HANDLE;
  CHECK_VK(vkCreateSwapchainKHR(
    device_, &swapchain_create_info_, nullptr, &swapchain_
  ));

  /* Create the swapchain resources (Views + Semaphores). */
  std::vector<VkImage> images(image_count_);
  CHECK_VK(vkGetSwapchainImagesKHR(
    device_, swapchain_, &image_count_, images.data()
  ));

  VkImageViewCreateInfo image_view_create_info{
    .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .viewType   = VK_IMAGE_VIEW_TYPE_2D,
    .format     = swapchain_create_info_.imageFormat,
    .components = {
      .r = VK_COMPONENT_SWIZZLE_R,
      .g = VK_COMPONENT_SWIZZLE_G,
      .b = VK_COMPONENT_SWIZZLE_B,
      .a = VK_COMPONENT_SWIZZLE_A,
    },
    .subresourceRange = {
      .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel   = 0u,
      .levelCount     = 1u,
      .baseArrayLayer = 0u,
      .layerCount     = 1u,
    },
  };

  swap_images_.resize(image_count_);
  swap_syncs_.resize(image_count_);

  for (uint32_t i = 0u; i < image_count_; ++i) {
    auto &buffer = swap_images_[i];
    auto &sync = swap_syncs_[i];

    buffer.image = images[i];
    buffer.format = swapchain_create_info_.imageFormat;
    image_view_create_info.image = buffer.image;
    CHECK_VK(vkCreateImageView(device_, &image_view_create_info, nullptr, &buffer.view));

    VkSemaphoreCreateInfo const semaphore_create_info{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };
    CHECK_VK(vkCreateSemaphore(device_, &semaphore_create_info, nullptr, &sync.wait_image_semaphore));
    CHECK_VK(vkCreateSemaphore(device_, &semaphore_create_info, nullptr, &sync.signal_present_semaphore));

#if !defined(NDEBUG)
    auto const s_index = std::to_string(i);
    context.set_debug_object_name(buffer.view,
      "Swapchain::ImageView::" + s_index
    );
    context.set_debug_object_name(sync.wait_image_semaphore,
      "Swapchain::Semaphore::WaitImage::" + s_index
    );
    context.set_debug_object_name(sync.signal_present_semaphore,
      "Swapchain::Semaphore::SignalPresent::" + s_index
    );
#endif
  }

  /* When using timeline semaphore, we need to transition images layout to present. */
  context.transition_images_layout(swap_images_,
    VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
  );
  need_rebuild_ = false;
}

// ----------------------------------------------------------------------------

void Swapchain::deinit(bool keep_previous_swapchain) {
  LOGD("Destroy swapchain resources,{} keep previous handle.",
    keep_previous_swapchain ? "" : " don't"
  );

  if (swapchain_create_info_.oldSwapchain != VK_NULL_HANDLE) [[likely]] {
    vkDestroySwapchainKHR(device_, swapchain_create_info_.oldSwapchain, nullptr);
    swapchain_create_info_.oldSwapchain = VK_NULL_HANDLE;
  }

  if (!keep_previous_swapchain) [[unlikely]] {
    vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
  }

  for (auto& buffer : swap_images_) {
    vkDestroyImageView(device_, buffer.view, nullptr);
  }
  swap_images_.clear();

  for (auto& frame_sync : swap_syncs_) {
    vkDestroySemaphore(device_, frame_sync.wait_image_semaphore, nullptr);
    vkDestroySemaphore(device_, frame_sync.signal_present_semaphore, nullptr);
  }
  swap_syncs_.clear();

  need_rebuild_ = true;
}

// ----------------------------------------------------------------------------

void Swapchain::acquire_next_image() {
  LOG_CHECK(swapchain_ != VK_NULL_HANDLE);

  auto const& semaphore = current_synchronizer().wait_image_semaphore;

  constexpr uint64_t kFiniteAcquireTimeout = 1'000'000'000ull; // 1s
  auto const acquire_result = vkAcquireNextImageKHR(
    device_, swapchain_, kFiniteAcquireTimeout, semaphore, VK_NULL_HANDLE, &next_swap_index_
  );
  need_rebuild_ = IsSwapchainInvalid(acquire_result, __FUNCTION__);
}

// ----------------------------------------------------------------------------

void Swapchain::present_and_swap(VkQueue const queue) {
  LOG_CHECK(swapchain_ != VK_NULL_HANDLE);

  auto const& semaphore = current_synchronizer().signal_present_semaphore;

  VkPresentInfoKHR const present_info{
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .pNext = nullptr,
    .waitSemaphoreCount = 1u,
    .pWaitSemaphores = &semaphore,
    .swapchainCount = 1u,
    .pSwapchains = &swapchain_,
    .pImageIndices = &next_swap_index_,
    .pResults = nullptr,
  };
  auto const present_result = vkQueuePresentKHR(queue, &present_info);
  need_rebuild_ = IsSwapchainInvalid(present_result, __FUNCTION__);

  current_swap_index_ = (current_swap_index_ + 1u) % image_count_;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

VkSurfaceFormat2KHR Swapchain::select_surface_format(
  VkPhysicalDeviceSurfaceInfo2KHR const* surface_info2
) const {
  LOG_CHECK(vkGetPhysicalDeviceSurfaceFormats2KHR);

#ifdef ANDROID
  LOGV("> Start vkGetPhysicalDeviceSurfaceFormats2KHR");
#endif

  uint32_t image_format_count{0u};
  CHECK_VK( vkGetPhysicalDeviceSurfaceFormats2KHR(gpu_, surface_info2, &image_format_count, nullptr) );
  std::vector<VkSurfaceFormat2KHR> formats(image_format_count, {.sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR});
  CHECK_VK( vkGetPhysicalDeviceSurfaceFormats2KHR(gpu_, surface_info2, &image_format_count, formats.data()) );

#ifdef ANDROID
  LOGV("> End vkGetPhysicalDeviceSurfaceFormats2KHR");
#endif

  VkSurfaceFormat2KHR const default_format{
    .sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR,
    .surfaceFormat = {
      .format = VK_FORMAT_B8G8R8A8_UNORM,
      .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
    },
  };
  if ((formats.size() == 1ul)
   && (formats[0].surfaceFormat.format == VK_FORMAT_UNDEFINED)) {
    return default_format;
  }

  std::vector<VkSurfaceFormat2KHR> const preferred_formats{
    default_format,
    VkSurfaceFormat2KHR{
      .sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR,
      .surfaceFormat = {
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
      },
    },
  };

  for (auto const& available : formats) {
    for (auto const& preferred : preferred_formats) {
      if ((available.surfaceFormat.format == preferred.surfaceFormat.format)
       && (available.surfaceFormat.colorSpace == preferred.surfaceFormat.colorSpace)) {
        return available;
      }
    }
  }

  return formats[0u];
}

// ----------------------------------------------------------------------------

VkPresentModeKHR Swapchain::select_present_mode(VkSurfaceKHR surface, bool use_vsync) const {
  if (use_vsync) {
    return VK_PRESENT_MODE_FIFO_KHR;
  }

  uint32_t present_mode_count{0u};
  vkGetPhysicalDeviceSurfacePresentModesKHR(gpu_, surface, &present_mode_count, nullptr);
  std::vector<VkPresentModeKHR> present_modes(present_mode_count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(gpu_, surface, &present_mode_count, present_modes.data());

  bool mailbox_available = false;
  bool immediate_available = false;

  for (auto mode : present_modes) {
    if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      mailbox_available = true;
    }
    if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
      immediate_available = true;
    }
  }

  // Best (low latency with no tears)
  if (mailbox_available) {
    return VK_PRESENT_MODE_MAILBOX_KHR;
  }

  // Second best (low latency, with tears)
  if (immediate_available) {
    return VK_PRESENT_MODE_IMMEDIATE_KHR;
  }

  // Default mode available everywhere.
  return VK_PRESENT_MODE_FIFO_KHR;
}

/* -------------------------------------------------------------------------- */
