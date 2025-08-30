#include "framework/renderer/fx/postprocess/fragment/render_target_fx.h"

#include "framework/backend/command_encoder.h"

/* -------------------------------------------------------------------------- */

std::string RenderTargetFx::GetMapScreenVertexShaderName() {
  return std::string(FRAMEWORK_COMPILED_SHADERS_DIR "postprocess/mapscreen.vert.glsl");
}

// ----------------------------------------------------------------------------

bool RenderTargetFx::resize(VkExtent2D const dimension) {
  if (!render_target_) {
    createRenderTarget(dimension);
    return true;
  }
  return render_target_->resize(dimension);
}

// ----------------------------------------------------------------------------

void RenderTargetFx::release() {
  render_target_->release();
  PostGenericFx::release();
}

// ----------------------------------------------------------------------------

void RenderTargetFx::execute(CommandEncoder& cmd) {
  if (!isEnabled()) {
    return;
  }

  // -----------------------------
  auto pass = cmd.begin_rendering(render_target_);
  {
    prepareDrawState(pass);
    pushConstant(pass); //
    draw(pass);
  }
  cmd.end_rendering();
  // -----------------------------
}

// ----------------------------------------------------------------------------

backend::Image RenderTargetFx::getImageOutput(uint32_t index) const {
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

/* -------------------------------------------------------------------------- */
