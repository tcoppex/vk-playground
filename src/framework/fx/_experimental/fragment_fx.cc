
#include "framework/fx/_experimental/fragment_fx.h"

#include "framework/backend/command_encoder.h"

/* -------------------------------------------------------------------------- */

std::string FragmentFx::GetMapScreenVertexShaderName() {
  return std::string(FRAMEWORK_COMPILED_SHADERS_DIR "postprocess/passthrough.vert.glsl");
}

/* -------------------------------------------------------------------------- */

void FragmentFx::setup(VkExtent2D const dimension) {
  GenericFx::setup(dimension);
  is_setup_ = true;
}

// ----------------------------------------------------------------------------

void FragmentFx::resize(VkExtent2D const dimension) {
  if (!render_target_) {
    createRenderTarget(dimension);
  } else {
    render_target_->resize(dimension);
  }
}

// ----------------------------------------------------------------------------

void FragmentFx::release() {
  if (!is_setup_) {
    return;
  }
  render_target_->release();
  GenericFx::release();
}

// ----------------------------------------------------------------------------

void FragmentFx::setImageInputs(std::vector<backend::Image> const& inputs) {
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
  renderer_ptr_->update_descriptor_set(descriptor_set_, { write_entry });
}

// ----------------------------------------------------------------------------

void FragmentFx::setBufferInputs(std::vector<backend::Buffer> const& inputs) {
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
  renderer_ptr_->update_descriptor_set(descriptor_set_, { write_entry });
}

// ----------------------------------------------------------------------------

void FragmentFx::execute(CommandEncoder& cmd) {
  if (!isEnabled()) {
    return;
  }

  // auto pass = cmd.begin_render_pass(*framebuffer_);
  // pass.set_viewport_scissor(framebuffer_->get_surface_size());
  // cmd.end_render_pass();

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

backend::Image const& FragmentFx::getImageOutput(uint32_t index) const {
  return render_target_->get_color_attachment(index); //
}

// ----------------------------------------------------------------------------

std::vector<backend::Image> const& FragmentFx::getImageOutputs() const {
  return render_target_->get_color_attachments();
}

/* -------------------------------------------------------------------------- */

void FragmentFx::createRenderTarget(VkExtent2D const dimension) {
  VkClearColorValue const debug_clear_value{ { 0.99f, 0.12f, 0.89f, 0.0f } };

  // framebuffer_ = renderer_ptr_->create_framebuffer({
  //   .dimension = dimension,
  //   .color_desc = {
  //     .format = getImageOutputFormat(),
  //     .samples = VK_SAMPLE_COUNT_1_BIT,
  //     .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
  //     .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
  //     .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
  //     .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,

  //     .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, //
  //     .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, //
  //   },
  //   .match_swapchain_output_count = false, //
  // });
  // framebuffer_->set_clear_color(debug_clear_value);

  render_target_ = renderer_ptr_->create_default_render_target();
  render_target_->set_color_clear_value(debug_clear_value);
}

// ----------------------------------------------------------------------------

void FragmentFx::createPipeline() {
  auto shaders{context_ptr_->create_shader_modules({
    getVertexShaderName(),
    getShaderName()
  })};

  pipeline_ = renderer_ptr_->create_graphics_pipeline(pipeline_layout_, {
    .vertex = {
      .module = shaders[0u].module
    },
    .fragment = {
      .module = shaders[1u].module,
      .targets = {
        // { .format = framebuffer_->get_color_attachment().format },
        { .format = render_target_->get_color_attachment().format },
      }
    },
    .primitive = {
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .cullMode = VK_CULL_MODE_NONE,
    },

    // .renderPass = framebuffer_->get_render_pass(),
  });

  context_ptr_->release_shader_modules(shaders);
}

/* -------------------------------------------------------------------------- */
