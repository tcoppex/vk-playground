#include "framework/backend/swapchain.h"
#include "framework/backend/context.h"

/* -------------------------------------------------------------------------- */

namespace {

bool CheckOutOfDataResult(VkResult const result, std::string_view const& msg) {
  switch (result) {
    case VK_ERROR_OUT_OF_DATE_KHR:
      LOGI("[TODO] The swapchain need to be rebuilt (%s).", msg.data());
    return true;

    case VK_SUCCESS:
    case VK_SUBOPTIMAL_KHR:
    break;

    default:
      LOGE("%s : swapchain image issue.", msg.data());
    break;
  }

  return false;
}

}

/* -------------------------------------------------------------------------- */

void Swapchain::init(Context const& context, VkSurfaceKHR const surface) {
  gpu_ = context.physical_device();
  device_ = context.device();
  surface_ = surface;

  LOG_CHECK(vkGetPhysicalDeviceSurfaceCapabilities2KHR);
  /* Retrieve the GPU's capabilities for this surface. */
  VkPhysicalDeviceSurfaceInfo2KHR const surface_info2{
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
    .surface = surface_,
  };
  VkSurfaceCapabilities2KHR capabilities2{
    .sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR
  };
  vkGetPhysicalDeviceSurfaceCapabilities2KHR(gpu_, &surface_info2, &capabilities2);

  /* Determine the number of image to use in the swapchain. */
  uint32_t const min_image_count{ capabilities2.surfaceCapabilities.minImageCount };
  uint32_t const preferred_image_count{ std::max(kPreferredMaxImageCount, min_image_count) };
  uint32_t const max_image_count{
    (capabilities2.surfaceCapabilities.maxImageCount == 0u) ? preferred_image_count
                                                            : capabilities2.surfaceCapabilities.maxImageCount
  };

  /* Select best swap surface format and present mode. */
  VkSurfaceFormat2KHR const surface_format2 = select_surface_format(&surface_info2);
  VkPresentModeKHR const present_mode = select_present_mode(kUseVSync);

  // TODO: check case where it does not match screen extent.
  surface_size_ = capabilities2.surfaceCapabilities.currentExtent; //

  image_count_ = std::clamp(preferred_image_count, min_image_count, max_image_count);
  VkFormat const color_format = surface_format2.surfaceFormat.format;
  LOGD("Swapchain images : %u", image_count_);

  /* Create the swapchain image. */
  VkSwapchainCreateInfoKHR swapchain_create_info{
    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .surface = surface_,
    .minImageCount = image_count_,
    .imageFormat = color_format,
    .imageColorSpace = surface_format2.surfaceFormat.colorSpace,
    .imageExtent = surface_size_,
    .imageArrayLayers = 1u,
    .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                ,
    .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .preTransform = capabilities2.surfaceCapabilities.currentTransform,
    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    .presentMode = present_mode,
    .clipped = VK_TRUE,
  };
  CHECK_VK(vkCreateSwapchainKHR(device_, &swapchain_create_info, nullptr, &swapchain_));

  /* Create the swapchain resources. */
  std::vector<VkImage> images(image_count_);
  CHECK_VK(vkGetSwapchainImagesKHR(device_, swapchain_, &image_count_, images.data()));

  VkImageViewCreateInfo image_view_create_info{
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format = color_format,
    .components = {
      .r = VK_COMPONENT_SWIZZLE_R,
      .g = VK_COMPONENT_SWIZZLE_G,
      .b = VK_COMPONENT_SWIZZLE_B,
      .a = VK_COMPONENT_SWIZZLE_A,
    },
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0u,
      .levelCount = 1u,
      .baseArrayLayer = 0u,
      .layerCount = 1u,
    },
  };

  swap_images_.resize(image_count_);
  swap_syncs_.resize(image_count_);

  for (uint32_t i = 0u; i < image_count_; ++i) {
    auto &buffer = swap_images_[i];
    auto &sync = swap_syncs_[i];

    buffer.image = images[i];
    buffer.format = color_format;
    image_view_create_info.image = buffer.image;
    CHECK_VK(vkCreateImageView(device_, &image_view_create_info, nullptr, &buffer.view));

    VkSemaphoreCreateInfo const semaphore_create_info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    CHECK_VK(vkCreateSemaphore(device_, &semaphore_create_info, nullptr, &sync.wait_image_semaphore));
    CHECK_VK(vkCreateSemaphore(device_, &semaphore_create_info, nullptr, &sync.signal_present_semaphore));
  }

  /* When using timeline semaphore, we need to transition images layout to present. */
  context.transition_images_layout(swap_images_,
    VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR // VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR
  );
}

// ----------------------------------------------------------------------------

void Swapchain::deinit() {
  vkDestroySwapchainKHR(device_, swapchain_, nullptr);
  for (auto& buffer : swap_images_) {
    vkDestroyImageView(device_, buffer.view, nullptr);
  }
  for (auto& frame_sync : swap_syncs_) {
    vkDestroySemaphore(device_, frame_sync.wait_image_semaphore, nullptr);
    vkDestroySemaphore(device_, frame_sync.signal_present_semaphore, nullptr);
  }
}

// ----------------------------------------------------------------------------

uint32_t Swapchain::acquire_next_image() {
  LOG_CHECK(swapchain_ != VK_NULL_HANDLE);

  auto const& semaphore = get_current_synchronizer().wait_image_semaphore;

  VkResult const result = vkAcquireNextImageKHR(
    device_, swapchain_, UINT64_MAX, semaphore, VK_NULL_HANDLE, &next_swap_index_
  );
  // LOG_CHECK(current_swap_index_ == next_swap_index_);

  need_rebuild_ = CheckOutOfDataResult(result, __FUNCTION__);

  return current_swap_index_;
}

// ----------------------------------------------------------------------------

void Swapchain::present_and_swap(VkQueue const queue) {
  LOG_CHECK(swapchain_ != VK_NULL_HANDLE);

  auto const& semaphore = get_current_synchronizer().signal_present_semaphore;

  VkPresentInfoKHR const present_info{
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1u,
    .pWaitSemaphores = &semaphore,
    .swapchainCount = 1u,
    .pSwapchains = &swapchain_,
    .pImageIndices = &next_swap_index_,
  };
  VkResult const result = vkQueuePresentKHR(queue, &present_info);

  need_rebuild_ = CheckOutOfDataResult(result, __FUNCTION__);

  current_swap_index_ = (current_swap_index_ + 1u) % image_count_;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

VkSurfaceFormat2KHR Swapchain::select_surface_format(VkPhysicalDeviceSurfaceInfo2KHR const* surface_info2) const {
  LOG_CHECK(vkGetPhysicalDeviceSurfaceFormats2KHR);

#ifdef ANDROID
  // (Those could spit errors log on Android)
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

VkPresentModeKHR Swapchain::select_present_mode(bool use_vsync) const {
  if (use_vsync) {
    return VK_PRESENT_MODE_FIFO_KHR;
  }

  uint32_t present_mode_count{0u};
  vkGetPhysicalDeviceSurfacePresentModesKHR(gpu_, surface_, &present_mode_count, nullptr);
  std::vector<VkPresentModeKHR> present_modes(present_mode_count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(gpu_, surface_, &present_mode_count, present_modes.data());

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
