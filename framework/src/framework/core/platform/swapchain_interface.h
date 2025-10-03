#pragma once

#include <vector>
#include "volk.h"

#include "framework/backend/types.h" //

/* -------------------------------------------------------------------------- */

class SwapchainInterface {
 public:
  // SwapchainInterface() = default;
  virtual ~SwapchainInterface();

  virtual bool acquireNextImage() = 0;

  // [todo: transform to accept a span of VkCommandBuffer]
  virtual bool submitFrame(VkQueue queue, VkCommandBuffer command_buffer) {
    std::vector<VkCommandBufferSubmitInfo> const cb_submit_infos{{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
      .commandBuffer = command_buffer,
    }};
    VkSubmitInfo2 const submit_info_2{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
      .commandBufferInfoCount = static_cast<uint32_t>(cb_submit_infos.size()),
      .pCommandBufferInfos = cb_submit_infos.data(),
    };
    return vkQueueSubmit2(queue, 1u, &submit_info_2, nullptr) == VK_SUCCESS;
  }

  virtual bool finishFrame(VkQueue queue) = 0;

  virtual VkExtent2D surfaceSize() const noexcept = 0;

  virtual uint32_t imageCount() const noexcept = 0;

  virtual VkFormat format() const noexcept = 0;

  virtual backend::RenderingViewInfo renderingViewInfo() const noexcept = 0;

  virtual backend::Image currentImage() const noexcept = 0;

  // virtual VkImage current_image() const noexcept;
  // virtual VkImageView current_image_view() const noexcept;

};

/* -------------------------------------------------------------------------- */
