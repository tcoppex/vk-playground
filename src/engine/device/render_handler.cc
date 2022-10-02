#include "engine/device/render_handler.h"

#include "glm/glm.hpp"

#include "engine/device/context.h"
#include "engine/core.h"

// ----------------------------------------------------------------------------

namespace {

template<typename T>
T clamp(const T& v, const T& lo, const T& hi) {
  return (v < lo) ? lo : (v > hi) ? hi : v;
}

} // namespace

// ----------------------------------------------------------------------------

RenderHandler::RenderHandler(DeviceContext const& ctx) :
  ctx_(ctx),
  swapchain_(VK_NULL_HANDLE),
  render_pass_(VK_NULL_HANDLE)
{
  /* Set default clear values */
  clear_colors(0.0f, 0.0f, 0.0f, 1.0f);
  clear_depth(1.0f);
  clear_stencil(0u);
}

// ----------------------------------------------------------------------------

void RenderHandler::init(unsigned int w, unsigned int h) {
  width_ = w;
  height_ = h;

  init_swapchain();
  init_synchronization();
  init_queues();
  init_graphics_command_buffer_wrapper();
  init_depth_buffer();
  init_render_pass();
  init_framebuffers();

  /* Get ready for the first image to render */
  acquire_next_swapchain_image();
}

// ----------------------------------------------------------------------------

void RenderHandler::deinit() {
  /* Wait for the queue to be completely idle */
  vkQueueWaitIdle(queue_.graphics);

  /* Framebuffers */
  for (auto& framebuffer : framebuffers_) {
    vkDestroyFramebuffer(ctx_.device(), framebuffer, nullptr);
  }

  /* Synchronization objects */
  vkDestroyFence(ctx_.device(), draw_fence_, nullptr);
  vkDestroySemaphore(ctx_.device(), img_acquisition_semaphore_, nullptr);

  /* Render pass */
  vkDestroyRenderPass(ctx_.device(), render_pass_, nullptr);

  /* Depth buffer */
  vkDestroyImageView(ctx_.device(), depth_.view, nullptr);
  vkDestroyImage(ctx_.device(), depth_.image, nullptr);
  vkFreeMemory(ctx_.device(), depth_.mem, nullptr);

  /* Swapchain */
  for (auto& swapchain_buffer : swapchain_buffers_) {
    vkDestroyImageView(ctx_.device(), swapchain_buffer.view, nullptr);
  }
  vkDestroySwapchainKHR(ctx_.device(), swapchain_, nullptr);
}

// ----------------------------------------------------------------------------

void RenderHandler::begin() {
  assert(render_pass_ != VK_NULL_HANDLE);
  assert(framebuffers_.size() > 0);

  /* Begin the command buffer recording */
  cmd_.begin();

  /* Begin the render pass */
  cmd_.begin_render_pass(render_pass_, framebuffers_[current_buffer_index_]);
}

// ----------------------------------------------------------------------------

void RenderHandler::end() {
  /* End the render pass */
  cmd_.end_render_pass();

  /* End the command buffer recording */
  CHECK_VK( cmd_.end() );

  /* Queue the command buffer for execution */
  VkPipelineStageFlags pipe_stage_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submit_info;
  submit_info.pNext = nullptr;
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = &img_acquisition_semaphore_;
  submit_info.pWaitDstStageMask = &pipe_stage_flags;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = cmd_.handle_ptr();
  submit_info.signalSemaphoreCount = 0;
  submit_info.pSignalSemaphores = nullptr;
  CHECK_VK( vkQueueSubmit(queue_.graphics, 1, &submit_info, draw_fence_) );
}

// ----------------------------------------------------------------------------

void RenderHandler::display_and_swap() {
  /* Present the image to be rendered (after the fence completed) */
  VkPresentInfoKHR present_info;
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.pNext = nullptr;
  present_info.swapchainCount = 1u;
  present_info.pSwapchains = &swapchain_;
  present_info.pImageIndices = &current_buffer_index_;
  present_info.pWaitSemaphores = nullptr;
  present_info.waitSemaphoreCount = 0u;
  present_info.pResults = nullptr;

  /* Wait for the image to be available. */
  wait_for_fence(draw_fence_);

  /* Display the rendered frame. */
  CHECK_VK( vkQueuePresentKHR(queue_.present, &present_info) );

  /* Update the available buffer index used for rendering. */
  acquire_next_swapchain_image();
}

// ----------------------------------------------------------------------------

void RenderHandler::clear_colors(float r, float g, float b, float a) {
  clear_values_[0].color.float32[0] = r;
  clear_values_[0].color.float32[1] = g;
  clear_values_[0].color.float32[2] = b;
  clear_values_[0].color.float32[3] = a;
}

// ----------------------------------------------------------------------------

void RenderHandler::clear_depth(float depth) {
  clear_values_[1u].depthStencil.depth = depth;
}

// ----------------------------------------------------------------------------

void RenderHandler::clear_stencil(uint32_t stencil) {
  clear_values_[1u].depthStencil.stencil = stencil;
}

// ----------------------------------------------------------------------------

void RenderHandler::resize(unsigned int w, unsigned int h) {
  width_ = w;
  height_ = h;

  /// @todo need to reallocate buffer at correct resolution.
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

void RenderHandler::init_swapchain() {
  /* Retrieve surface capabilities. */
  VkSurfaceCapabilitiesKHR capabilities;
  CHECK_VK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    ctx_.gpu_, ctx_.surface_, &capabilities
  ));

  /* Try to match swapchain resolution with surface resolution. */
  VkExtent2D swapchain_extent;
  if (UINT32_MAX == capabilities.currentExtent.width) {
    /* if surface resolution is undefined, set to app resolution. */
    const VkExtent2D &min_ext = capabilities.minImageExtent;
    const VkExtent2D &max_ext = capabilities.maxImageExtent;
    swapchain_extent.width  = clamp(width_, min_ext.width, max_ext.width);
    swapchain_extent.height = clamp(height_, min_ext.height, max_ext.height);
  } else {
    swapchain_extent = capabilities.currentExtent;
  }

  /* Set the swapchain present mode. */
  // Default mode available everywhere.
  VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

  // Retrieve present modes.
  uint32_t present_mode_count;
  CHECK_VK(vkGetPhysicalDeviceSurfacePresentModesKHR(
    ctx_.gpu_, ctx_.surface_, &present_mode_count, nullptr
  ));
  VkPresentModeKHR *present_modes = new VkPresentModeKHR[present_mode_count]();
  CHECK_VK(vkGetPhysicalDeviceSurfacePresentModesKHR(
    ctx_.gpu_, ctx_.surface_, &present_mode_count, present_modes
  ));

  // Search for the best available swapchain present modes.
  for (uint32_t i=0u; i < present_mode_count; ++i) {
    auto mode = present_modes[i];

    // Best (lowest latency with no tears)
    if (VK_PRESENT_MODE_MAILBOX_KHR == mode) {
      present_mode = mode;
      break;
    }
    // Second best (fastest, with tears)
    if (VK_PRESENT_MODE_IMMEDIATE_KHR == mode) {
      present_mode = mode;
    }
  }
  delete [] present_modes;

  /* Number of images tu use on the swapchains. */
  uint32_t num_swapchains_images = std::min(capabilities.minImageCount + 0,
                                            capabilities.maxImageCount);

  /* Transform applied to the surface. */
  VkSurfaceTransformFlagBitsKHR transform = capabilities.currentTransform;
  if (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
    transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  }

  /* Set a composite alpha mode. */
  VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

  VkSwapchainCreateInfoKHR swapchain_info;
  swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchain_info.pNext = nullptr;
  swapchain_info.surface = ctx_.surface_;
  swapchain_info.minImageCount = num_swapchains_images;
  swapchain_info.imageFormat = ctx_.format_;
  swapchain_info.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
  swapchain_info.imageExtent = swapchain_extent;
  swapchain_info.imageArrayLayers = 1;
  swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swapchain_info.queueFamilyIndexCount = 0;
  swapchain_info.pQueueFamilyIndices = nullptr;
  swapchain_info.preTransform = transform;
  swapchain_info.compositeAlpha = composite_alpha;
  swapchain_info.presentMode = present_mode;
  swapchain_info.clipped = true;
  swapchain_info.oldSwapchain = VK_NULL_HANDLE;

  // If the graphics and present queues are from different queue families,
  // we either have to explicitly transfer ownership of images between the
  // queues, or we have to create the swapchain with imageSharingMode
  // as VK_SHARING_MODE_CONCURRENT.
  uint32_t queue_indices[2u] = {
    ctx_.selected_queue_index_.graphics,
    ctx_.selected_queue_index_.present
  };
  if (ctx_.selected_queue_index_.graphics == ctx_.selected_queue_index_.present) {
    swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_info.queueFamilyIndexCount = GLM_COUNTOF(queue_indices);
    swapchain_info.pQueueFamilyIndices = queue_indices;
  }

  /* Setup Swapchain buffers. */
  CHECK_VK( vkCreateSwapchainKHR(ctx_.device(), &swapchain_info, nullptr, &swapchain_) );
  CHECK_VK( vkGetSwapchainImagesKHR(ctx_.device(), swapchain_, &num_swapchains_images, nullptr) );
  std::vector<VkImage> images(num_swapchains_images);
  CHECK_VK( vkGetSwapchainImagesKHR(ctx_.device(), swapchain_, &num_swapchains_images, images.data()) );

  // set a generic imageview info.
  VkImageViewCreateInfo view_info;
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.pNext = nullptr;
  view_info.format = ctx_.format_;
  view_info.components.r = VK_COMPONENT_SWIZZLE_R;
  view_info.components.g = VK_COMPONENT_SWIZZLE_G;
  view_info.components.b = VK_COMPONENT_SWIZZLE_B;
  view_info.components.a = VK_COMPONENT_SWIZZLE_A;
  view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.baseArrayLayer = 0;
  view_info.subresourceRange.layerCount = 1;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.flags = 0u;

  // fill swapchain buffers.
  swapchain_buffers_.resize(num_swapchains_images);
  for (unsigned int i=0u; i<swapchain_buffers_.size(); ++i) {
    const auto &img = images[i];

    view_info.image = img;
    swapchain_buffers_[i].image = img;
    CHECK_VK(vkCreateImageView(
      ctx_.device(), &view_info, nullptr, &swapchain_buffers_[i].view)
    );
  }
}

// ----------------------------------------------------------------------------

void RenderHandler::init_synchronization() {
  /* Used to acquire the rendered image only when available. */
  VkSemaphoreCreateInfo semaphore_info;
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphore_info.pNext = nullptr;
  semaphore_info.flags = 0;
  CHECK_VK(vkCreateSemaphore(
    ctx_.device(), &semaphore_info, nullptr, &img_acquisition_semaphore_)
  );

  /* Used to wait for the GPU to finish rendering before presenting the surface. */
  VkFenceCreateInfo fence_info;
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.pNext = nullptr;
  fence_info.flags = 0;

  CHECK_VK( vkCreateFence(ctx_.device(), &fence_info, nullptr, &draw_fence_) );
}

// ----------------------------------------------------------------------------

void RenderHandler::init_queues() {
  /* Retrieve queue object */
  vkGetDeviceQueue(ctx_.device(), ctx_.selected_queue_index_.graphics, 0, &queue_.graphics);
  if (ctx_.selected_queue_index_.graphics == ctx_.selected_queue_index_.present) {
    queue_.present = queue_.graphics;
  } else {
    vkGetDeviceQueue(ctx_.device(), ctx_.selected_queue_index_.present, 0, &queue_.present);
  }
}

// ----------------------------------------------------------------------------

void RenderHandler::init_graphics_command_buffer_wrapper() {
  /* Wrapper around the graphics command buffer */
  cmd_.handle_ptr( &(ctx_.cmd_buffers_[0u]) );
  cmd_.render_area(0, 0, width_, height_);
  cmd_.clear_values(clear_values_, GLM_COUNTOF(clear_values_));
}

// ----------------------------------------------------------------------------

void RenderHandler::init_depth_buffer() {
  /* Check and change the default depth format if invalid. */
  depth_.format = GetValidDepthFormat(depth_.format);

  /* Create image. */
  VkImageCreateInfo image_info;
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.pNext = nullptr;
  image_info.flags = 0u;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.format = depth_.format;
  image_info.extent = { width_, height_, 1u };
  image_info.mipLevels = 1u;
  image_info.arrayLayers = 1u;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.pQueueFamilyIndices = nullptr;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  CHECK_VK( vkCreateImage(ctx_.device(), &image_info, nullptr, &depth_.image) );

  /* Retrieve memory requirements for the depth image. */
  VkMemoryRequirements requirements;
  vkGetImageMemoryRequirements(ctx_.device(), depth_.image, &requirements);

  /* Find the memory type index. */
  uint32_t memory_type_index = GetMemoryTypeIndex(
    ctx_.properties_.memory, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
  );
  assert(UINT32_MAX != memory_type_index);

  /* Allocate memory. */
  depth_.allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  depth_.allocate_info.pNext = nullptr;
  depth_.allocate_info.allocationSize = requirements.size;
  depth_.allocate_info.memoryTypeIndex = memory_type_index;
  CHECK_VK( vkAllocateMemory(ctx_.device(), &depth_.allocate_info, nullptr, &depth_.mem) );

  /* Bind memory to the image. */
  CHECK_VK( vkBindImageMemory(ctx_.device(), depth_.image, depth_.mem, 0) );

  /* Create an image view. */
  VkImageViewCreateInfo view_info;
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.pNext = nullptr;
  view_info.image = depth_.image;
  view_info.format = depth_.format;
  view_info.components.r = VK_COMPONENT_SWIZZLE_R;
  view_info.components.g = VK_COMPONENT_SWIZZLE_G;
  view_info.components.b = VK_COMPONENT_SWIZZLE_B;
  view_info.components.a = VK_COMPONENT_SWIZZLE_A;
  view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.baseArrayLayer = 0;
  view_info.subresourceRange.layerCount = 1;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.flags = 0;

  // Handle special format with stencil component.
  if (   depth_.format == VK_FORMAT_D16_UNORM_S8_UINT
      || depth_.format == VK_FORMAT_D24_UNORM_S8_UINT
      || depth_.format == VK_FORMAT_D32_SFLOAT_S8_UINT) {
    view_info.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
  }

  CHECK_VK( vkCreateImageView(ctx_.device(), &view_info, nullptr, &depth_.view) );
}

// ----------------------------------------------------------------------------

void RenderHandler::init_render_pass() {
  assert(ctx_.format_ != VK_FORMAT_UNDEFINED);
  assert(depth_.format != VK_FORMAT_UNDEFINED);

  // The initial layout for the color and depth attachments will be
  // LAYOUT_UNDEFINED because at the start of the renderpass, we don't
  // care about their contents. At the start of the subpass, the color
  // attachment's layout will be transitioned to LAYOUT_COLOR_ATTACHMENT_OPTIMAL
  // and the depth stencil attachment's layout will be transitioned to
  // LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL.  At the end of the renderpass,
  // the color attachment's layout will be transitioned to
  // LAYOUT_PRESENT_SRC_KHR to be ready to present.  This is all done as part
  // of the renderpass, no barriers are necessary.
  VkAttachmentDescription attachments[2u];
  // color
  attachments[0].format = ctx_.format_;
  attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
  attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  attachments[0].flags = 0;

  // depth
  attachments[1].format = depth_.format;
  attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
  attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  attachments[1].flags = 0;

  VkAttachmentReference references[2u];
  // color
  references[0].attachment = 0;
  references[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  // depth
  references[1].attachment = 1;
  references[1].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  /* Defines subpass */
  VkSubpassDescription subpass;
  subpass.flags = 0;
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.inputAttachmentCount = 0;
  subpass.pInputAttachments = nullptr;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &references[0];
  subpass.pResolveAttachments = nullptr;
  subpass.pDepthStencilAttachment = &references[1];
  subpass.preserveAttachmentCount = 0;
  subpass.pPreserveAttachments = nullptr;

  /* Create the render pass */
  VkRenderPassCreateInfo info;
  info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  info.pNext = nullptr;
  info.attachmentCount = GLM_COUNTOF(attachments);
  info.pAttachments = attachments;
  info.subpassCount = 1;
  info.pSubpasses = &subpass;
  info.dependencyCount = 0;
  info.pDependencies = nullptr;

  CHECK_VK( vkCreateRenderPass(ctx_.device(), &info, nullptr, &render_pass_) );
}

// ----------------------------------------------------------------------------

void RenderHandler::init_framebuffers() {
  assert(swapchain_buffers_.size() > 0u);
  assert(depth_.view != VK_NULL_HANDLE);
  assert(render_pass_ != VK_NULL_HANDLE);

  VkImageView attachments[2u];

  VkFramebufferCreateInfo info;
  info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  info.pNext = nullptr;
  info.flags = 0;
  info.renderPass = render_pass_;
  info.attachmentCount = GLM_COUNTOF(attachments);
  info.pAttachments = attachments;
  info.width = width_;
  info.height = height_;
  info.layers = 1u;

  framebuffers_.resize(swapchain_buffers_.size());

  attachments[1u] = depth_.view;
  for (uint32_t i = 0u; i < swapchain_buffers_.size(); ++i) {
    attachments[0u] = swapchain_buffers_[i].view;
    CHECK_VK( vkCreateFramebuffer(ctx_.device(), &info, nullptr, &framebuffers_[i]) );
  }
}

// ----------------------------------------------------------------------------

void RenderHandler::acquire_next_swapchain_image() {
  assert(swapchain_ != VK_NULL_HANDLE);
  assert(img_acquisition_semaphore_ != VK_NULL_HANDLE);

  /* Acquire the swapchain image */
  CHECK_VK(vkAcquireNextImageKHR(
    ctx_.device(), swapchain_, UINT64_MAX, img_acquisition_semaphore_, VK_NULL_HANDLE, &current_buffer_index_
  ));
}

// ----------------------------------------------------------------------------

void RenderHandler::wait_for_fence(VkFence& fence) {
  VkResult res;

  /* Wait for the fence to complete on the device. */
  do {
      res = vkWaitForFences(ctx_.device(), 1, &fence, VK_TRUE, kFenceTimeoutNanoseconds);
  } while (res == VK_TIMEOUT);
  assert(VK_SUCCESS == res);

  /* Reset the fence for its next use. */
  vkResetFences(ctx_.device(), 1, &fence);
}

// ----------------------------------------------------------------------------
