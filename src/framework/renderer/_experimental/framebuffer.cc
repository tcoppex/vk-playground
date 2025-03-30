#include "framework/renderer/_experimental/framebuffer.h"
#include "framework/backend/context.h"

/* -------------------------------------------------------------------------- */

void Framebuffer::release() {
  release_buffers();

  vkDestroyRenderPass(device_, render_pass_, nullptr);
  render_pass_ = VK_NULL_HANDLE;
}

/* -------------------------------------------------------------------------- */


void Framebuffer::resize(VkExtent2D const dimension) {
  dimension_ = dimension;

  release_buffers();

  VkFramebufferCreateInfo const framebuffer_create_info{
    .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
    .renderPass = render_pass_,
    .attachmentCount = static_cast<uint32_t>(image_views_.size()),
    .pAttachments = image_views_.data(),
    .width = dimension_.width,
    .height = dimension_.height,
    .layers = 1u,
  };

  if (use_depth_stencil_) {
    // depth_stencil_ = context.create_image_2d(dimension_.width, dimension_.height, 1u, desc_.depth_format);
    // image_views_[Framebuffer::DEPTH_STENCIL] = depth_stencil_.view;
  }

  for (size_t i = 0u; i < framebuffers_.size(); ++i) {
    image_views_[Framebuffer::COLOR] = desc_.image_views[i];
    CHECK_VK(vkCreateFramebuffer(device_, &framebuffer_create_info, nullptr, &framebuffers_[i]));
  }
}

/* -------------------------------------------------------------------------- */

Framebuffer::Framebuffer(
  Context const& context,
  std::shared_ptr<ResourceAllocator> allocator,
  Descriptor_t const& desc,
  uint32_t const* swap_index_ptr
) : device_(context.get_device())
  , allocator_(allocator)
  , swap_index_ptr_(swap_index_ptr)
  , dimension_(desc.dimension)
{
  setup(context, desc); //
  resize(desc.dimension); //
}

// ----------------------------------------------------------------------------

void Framebuffer::release_buffers() {
  for (auto& framebuffer : framebuffers_) {
    vkDestroyFramebuffer(device_, framebuffer, nullptr);
    framebuffer = VK_NULL_HANDLE;
  }
  allocator_->destroy_image(&depth_stencil_);
}

// ----------------------------------------------------------------------------

void Framebuffer::setup(Context const& context, Descriptor_t const& desc) {
  uint32_t const bufferCount{
    use_depth_stencil_ ? static_cast<uint32_t>(Framebuffer::kBufferNameCount)
                       : static_cast<uint32_t>(Framebuffer::kBufferNameCount - 1u)
  };

  clear_values_.resize(bufferCount);
  image_views_.resize(bufferCount);

  desc_ = desc; //
  framebuffers_.resize(desc_.image_views.size()); //

  // -----------

  /* Setup the RenderPass. */
  {
    /* Color Attachment. */
    std::vector<VkAttachmentDescription> attachments{
      desc.color_desc
    };
    std::vector<VkAttachmentReference> references{
      {
        .attachment = Framebuffer::COLOR,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      },
    };

    /* [Optional] Depth-Stencil Attachment. */
    use_depth_stencil_ = (desc.depth_format != VK_FORMAT_UNDEFINED);
    if (use_depth_stencil_) {
      attachments.push_back(
        {
          .format = desc.depth_format,
          .samples = VK_SAMPLE_COUNT_1_BIT,
          .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
          .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
          .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, //
          .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, //
          .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        }
      );
      references.push_back(
        {
          .attachment = Framebuffer::DEPTH_STENCIL,
          .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        }
      );
    }

    std::vector<VkSubpassDescription> const subpasses{
      {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1u,
        .pColorAttachments = &references[Framebuffer::COLOR],
        .pDepthStencilAttachment = use_depth_stencil_ ? &references[Framebuffer::DEPTH_STENCIL] : nullptr,
      }
    };
    VkRenderPassCreateInfo const render_pass_create_info{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = static_cast<uint32_t>(attachments.size()),
      .pAttachments = attachments.data(),
      .subpassCount = static_cast<uint32_t>(subpasses.size()),
      .pSubpasses = subpasses.data(),
    };
    CHECK_VK(vkCreateRenderPass(device_, &render_pass_create_info, nullptr, &render_pass_));
  }
}

/* -------------------------------------------------------------------------- */
