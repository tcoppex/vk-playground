#include "framework/renderer/framebuffer.h"
#include "framework/backend/context.h"

/* -------------------------------------------------------------------------- */

void Framebuffer::release() {
  for (auto& framebuffer : framebuffers_) {
    vkDestroyFramebuffer(device_, framebuffer, nullptr);
    framebuffer = VK_NULL_HANDLE;
  }

  vkDestroyRenderPass(device_, render_pass_, nullptr);
  render_pass_ = VK_NULL_HANDLE;

  allocator_->destroy_image(&depth_stencil_);
}

/* -------------------------------------------------------------------------- */

Framebuffer::Framebuffer(
  Context const& context,
  std::shared_ptr<ResourceAllocator> allocator,
  uint32_t const* swap_index_ptr,
  Descriptor_t const& desc
) : device_(context.get_device())
  , allocator_(allocator)
  , swap_index_ptr_(swap_index_ptr)
  , dimension_(desc.dimension)
{
  clear_values_.resize(Framebuffer::kBufferNameCount, {});
  depth_stencil_ = context.create_image_2d(dimension_.width, dimension_.height, 1u, desc.depth_format);
  init_render_pass(desc.color_format);
  init_framebuffer(desc);
}

// ----------------------------------------------------------------------------

void Framebuffer::init_render_pass(VkFormat const color_format) {
  VkImageLayout const kColorFinalLayout{
    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR //
  };

  std::vector<VkAttachmentDescription> const attachments{
    // Color
    {
      .format = color_format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = kColorFinalLayout, //
    },

    // Depth-Stencil
    {
      .format = depth_stencil_.format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, //
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, //
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    },
  };

  std::vector<VkAttachmentReference> const references{
    {
      .attachment = Framebuffer::COLOR,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    },
    {
      .attachment = Framebuffer::DEPTH_STENCIL,
      .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    },
  };

  /* Defines subpass */
  std::vector<VkSubpassDescription> const subpasses{
    {
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = 1u,
      .pColorAttachments = &references[Framebuffer::COLOR],
      .pDepthStencilAttachment = &references[Framebuffer::DEPTH_STENCIL],
    }
  };

  /* Create the render pass */
  VkRenderPassCreateInfo const render_pass_create_info{
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .attachmentCount = static_cast<uint32_t>(attachments.size()),
    .pAttachments = attachments.data(),
    .subpassCount = static_cast<uint32_t>(subpasses.size()),
    .pSubpasses = subpasses.data(),
  };
  CHECK_VK(vkCreateRenderPass(device_, &render_pass_create_info, nullptr, &render_pass_));
}

// ----------------------------------------------------------------------------

void Framebuffer::init_framebuffer(Descriptor_t const& desc) {
  std::vector<VkImageView> attachments(Framebuffer::kBufferNameCount);

  VkFramebufferCreateInfo const framebuffer_create_info{
    .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
    .renderPass = render_pass_,
    .attachmentCount = static_cast<uint32_t>(attachments.size()),
    .pAttachments = attachments.data(),
    .width = dimension_.width,
    .height = dimension_.height,
    .layers = 1u,
  };

  framebuffers_.resize(desc.image_views.size());

  attachments[Framebuffer::DEPTH_STENCIL] = depth_stencil_.view;
  for (size_t i = 0u; i < framebuffers_.size(); ++i) {
    attachments[Framebuffer::COLOR] = desc.image_views[i];
    CHECK_VK( vkCreateFramebuffer(device_, &framebuffer_create_info, nullptr, &framebuffers_[i]) );
  }
}

/* -------------------------------------------------------------------------- */
