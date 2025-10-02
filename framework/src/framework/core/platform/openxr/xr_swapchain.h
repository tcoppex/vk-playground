#pragma once

/* -------------------------------------------------------------------------- */

#include "framework/core/common.h"
#include "framework/core/platform/openxr/xr_common.h"
#include "framework/core/platform/openxr/xr_vulkan_interface.h" //
#include "framework/core/platform/swapchain_interface.h" //

/* -------------------------------------------------------------------------- */

struct OpenXRSwapchain : public SwapchainInterface {
 public:
  virtual ~OpenXRSwapchain() = default;

  bool acquireNextImage() final {
    XrSwapchainImageAcquireInfo acquire_info{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    CHECK_XR_RET(xrAcquireSwapchainImage(
      handle, &acquire_info, &current_image_index
    ));
    XrSwapchainImageWaitInfo wait_info{
      .type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
      .timeout = XR_INFINITE_DURATION,
    };
    CHECK_XR_RET(xrWaitSwapchainImage(handle, &wait_info));
    return true;
  }

  bool finishFrame(VkQueue queue) final {
    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    CHECK_XR_RET(xrReleaseSwapchainImage(handle, &releaseInfo));
    return true;
  }

  VkExtent2D surfaceSize() const noexcept final {
    return {
      .width = create_info.width,
      .height = create_info.height,
    };
  }

  uint32_t imageCount() const noexcept final {
    return image_count;
  }

  VkFormat format() const noexcept final {
    return (VkFormat)create_info.format;
  }

  backend::Image current_image() const noexcept final {
    return images[current_image_index];
  }

 public:
  bool create(
    XrSession session,
    XrSwapchainCreateInfo const& info,
    std::shared_ptr<XRVulkanInterface> xr_graphics //
  ) {
    create_info = info;

    // Create the swapchain object.
    CHECK_XR_RET(xrCreateSwapchain(session, &info, &handle))

    // Retrieve swapchain images.
    CHECK_XR(xrEnumerateSwapchainImages(handle, 0, &image_count, nullptr));
    std::vector<XrSwapchainImageVulkanKHR> base_images(image_count, {
      .type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR
    });
    CHECK_XR(xrEnumerateSwapchainImages(
      handle, image_count, &image_count,
      reinterpret_cast<XrSwapchainImageBaseHeader*>(base_images.data())
    ));

    auto const aspect_mask =
        (info.usageFlags & XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
            ? VK_IMAGE_ASPECT_DEPTH_BIT
            : VK_IMAGE_ASPECT_COLOR_BIT
            ;

    auto view_info = VkImageViewCreateInfo{
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .viewType = (info.arraySize > 1u) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY
                                        : VK_IMAGE_VIEW_TYPE_2D,
      .format = (VkFormat)info.format,
      .components = {
          VK_COMPONENT_SWIZZLE_R,
          VK_COMPONENT_SWIZZLE_G,
          VK_COMPONENT_SWIZZLE_B,
          VK_COMPONENT_SWIZZLE_A,
      },
      .subresourceRange = {
          .aspectMask = aspect_mask,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0, //
          .layerCount = info.arraySize, //
      }
    };

    xr_graphics->allocateSwapchainImage(base_images, view_info, images);

    return true;
  }

    void destroy(std::shared_ptr<XRVulkanInterface> xr_graphics) {
      xr_graphics->releaseSwapchainImage(images);
      images.clear();
      if (handle != XR_NULL_HANDLE) {
        xrDestroySwapchain(handle);
        handle = XR_NULL_HANDLE;
      }
    }

    [[nodiscard]]
    XrExtent2Di extent() const noexcept {
      return {
        .width = static_cast<int32_t>(create_info.width),
        .height = static_cast<int32_t>(create_info.height)
      };
    }

    [[nodiscard]]
    XrRect2Di rect() const noexcept {
      return {
        .offset = XrOffset2Di{0, 0},
        .extent = extent()
      };
    }

 public:
  XrSwapchainCreateInfo create_info{};
  XrSwapchain handle{XR_NULL_HANDLE};
  std::vector<backend::Image> images{};
  uint32_t image_count{};
  uint32_t current_image_index{};
};

/* -------------------------------------------------------------------------- */
