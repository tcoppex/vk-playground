#include "framework/renderer/fx/postprocess/generic_fx.h"
#include "framework/backend/context.h"
// #include <filesystem> 

/* -------------------------------------------------------------------------- */

void GenericFx::init(Renderer const& renderer) {
  FxInterface::init(renderer);
  allocator_ptr_ = renderer.context().allocator_ptr();
}

void GenericFx::setup(VkExtent2D const dimension) {
  LOG_CHECK(nullptr != context_ptr_);
  createPipelineLayout();
  createPipeline();
  descriptor_set_ = renderer_ptr_->create_descriptor_set(descriptor_set_layout_); //
}

void GenericFx::release() {
  if (pipeline_layout_ != VK_NULL_HANDLE) {
    renderer_ptr_->destroy_pipeline(pipeline_);
    renderer_ptr_->destroy_pipeline_layout(pipeline_layout_); //
    renderer_ptr_->destroy_descriptor_set_layout(descriptor_set_layout_);
    pipeline_layout_ = VK_NULL_HANDLE;
  }
}

// std::string GenericFx::name() const {
//   std::filesystem::path fn(getShaderName());
//   return fn.stem().string();
// }

void GenericFx::createPipelineLayout() {
  descriptor_set_layout_ = renderer_ptr_->create_descriptor_set_layout(
    getDescriptorSetLayoutParams()
  );
  pipeline_layout_ = renderer_ptr_->create_pipeline_layout({
    .setLayouts = { descriptor_set_layout_ },
    .pushConstantRanges = getPushConstantRanges()
  });
}

/* -------------------------------------------------------------------------- */

