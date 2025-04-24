#include "framework/fx/_experimental/fragment_fx.h"

#include "framework/backend/command_encoder.h"

/* -------------------------------------------------------------------------- */

std::string RenderTargetFx::GetMapScreenVertexShaderName() {
  return std::string(FRAMEWORK_COMPILED_SHADERS_DIR "postprocess/mapscreen.vert.glsl");
}

// ----------------------------------------------------------------------------

void RenderTargetFx::resize(VkExtent2D const dimension) {
  if (!render_target_) {
    createRenderTarget(dimension);
  } else {
    render_target_->resize(dimension);
  }
}

// ----------------------------------------------------------------------------

void RenderTargetFx::release() {
  render_target_->release();
  PostGenericFx::release();
}

// ----------------------------------------------------------------------------

void RenderTargetFx::setImageInputs(std::vector<backend::Image> const& inputs) {
  DescriptorSetWriteEntry write_entry{
    .binding = 0u,
    .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
  };
  for (auto const& input : inputs) {
    write_entry.images.push_back({
      .sampler = renderer_ptr_->get_default_sampler(), //
      .imageView = input.view,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    });
  }
  context_ptr_->update_descriptor_set(descriptor_set_, { write_entry });
}

// ----------------------------------------------------------------------------

void RenderTargetFx::setBufferInputs(std::vector<backend::Buffer> const& inputs) {
  DescriptorSetWriteEntry write_entry{
    .binding = 1u,
    .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
  };
  for (auto const& input : inputs) {
    write_entry.buffers.push_back({
      .buffer = input.buffer,
      .offset = 0,
      .range = VK_WHOLE_SIZE,
    });
  }
  context_ptr_->update_descriptor_set(descriptor_set_, { write_entry });
}

// ----------------------------------------------------------------------------

void RenderTargetFx::execute(CommandEncoder& cmd) {
  if (!isEnabled()) {
    return;
  }

  auto pass = cmd.begin_rendering(render_target_);
  {
    pass.set_viewport_scissor(render_target_->get_surface_size());
    pass.bind_pipeline(pipeline_);
    pass.bind_descriptor_set(descriptor_set_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    updatePushConstant(cmd);

    draw(pass);
  }
  cmd.end_rendering();
}

// ----------------------------------------------------------------------------

backend::Image const& RenderTargetFx::getImageOutput(uint32_t index) const {
  return render_target_->get_color_attachment(index); //
}

// ----------------------------------------------------------------------------

std::vector<backend::Image> const& RenderTargetFx::getImageOutputs() const {
  return render_target_->get_color_attachments();
}

/* -------------------------------------------------------------------------- */

void RenderTargetFx::createRenderTarget(VkExtent2D const dimension) {
  VkClearColorValue const debug_clear_value{ { 0.99f, 0.12f, 0.89f, 0.0f } }; //

#if 1
  render_target_ = renderer_ptr_->create_default_render_target();
#else
  render_target_ = renderer_ptr_->create_render_target({
    .color_formats = {
      VK_FORMAT_R32G32B32A32_SFLOAT,
    },
    .depth_stencil_format = VK_FORMAT_D24_UNORM_S8_UINT, //
    .size = dimension,
    .sampler = renderer_ptr_->get_default_sampler(),
  });
#endif

  render_target_->set_color_clear_value(debug_clear_value);
}

// ----------------------------------------------------------------------------

void RenderTargetFx::createPipeline() {
  auto shaders{context_ptr_->create_shader_modules({
    getVertexShaderName(),
    getShaderName()
  })};
  pipeline_ = renderer_ptr_->create_graphics_pipeline(pipeline_layout_, getGraphicsPipelineDescriptor(shaders));
  context_ptr_->release_shader_modules(shaders);
}

/* -------------------------------------------------------------------------- */
