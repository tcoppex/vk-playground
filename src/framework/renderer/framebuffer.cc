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
  Descriptor_t const& desc,
  uint32_t const* swap_index_ptr
) : device_(context.get_device())
  , allocator_(allocator)
  , swap_index_ptr_(swap_index_ptr)
  , dimension_(desc.dimension)
{
  setup(context, desc); //
}

/* -------------------------------------------------------------------------- */

void Framebuffer::setup(Context const& context, Descriptor_t const& desc) {
  bool const useDepthStencil{
    desc.depth_format != VK_FORMAT_UNDEFINED
  };

  uint32_t const bufferCount{
    useDepthStencil ? static_cast<uint32_t>(Framebuffer::kBufferNameCount)
                    : static_cast<uint32_t>(Framebuffer::kBufferNameCount - 1u)
  };

  clear_values_.resize(bufferCount, {});

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
    if (useDepthStencil) {
      depth_stencil_ = context.create_image_2d(dimension_.width, dimension_.height, 1u, desc.depth_format);

      attachments.push_back(
        {
          .format = depth_stencil_.format,
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
        .pDepthStencilAttachment = useDepthStencil ? &references[Framebuffer::DEPTH_STENCIL] : nullptr,
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

  /* Setup internal framebuffers. */
  {
    framebuffers_.resize(desc.image_views.size());

    std::vector<VkImageView> views(bufferCount);
    VkFramebufferCreateInfo const framebuffer_create_info{
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass = render_pass_,
      .attachmentCount = bufferCount,
      .pAttachments = views.data(),
      .width = dimension_.width,
      .height = dimension_.height,
      .layers = 1u,
    };

    if (useDepthStencil) {
      views[Framebuffer::DEPTH_STENCIL] = depth_stencil_.view;
    }

    for (size_t i = 0u; i < framebuffers_.size(); ++i) {
      views[Framebuffer::COLOR] = desc.image_views[i];
      CHECK_VK(vkCreateFramebuffer(device_, &framebuffer_create_info, nullptr, &framebuffers_[i]));
    }
  }
}

/* -------------------------------------------------------------------------- */
