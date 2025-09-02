#ifndef VKFRAMEWORK_RENDERER_FX_RAY_TRACING_FX_H_
#define VKFRAMEWORK_RENDERER_FX_RAY_TRACING_FX_H_

#include "framework/renderer/fx/postprocess/generic_fx.h"

/* -------------------------------------------------------------------------- */

class RayTracingFx : public virtual GenericFx {

 protected:
  std::string getShaderName() const final { return ""; } //

  std::vector<DescriptorSetLayoutParams> getDescriptorSetLayoutParams() const override {
    return {};
  }

  void createPipeline() final {
    auto shaders_map = createShaderModules();
    pipeline_ = renderer_ptr_->create_raytracing_pipeline(
      pipeline_layout_,
      pipelineDescriptor(shaders_map)
    );
    for (auto const& [_, shaders] : shaders_map) {
      for (auto const& shader : shaders) {
        context_ptr_->release_shader_module(shader);
      }
    }
  }

  void setImageInputs(std::vector<backend::Image> const& inputs) final {
  }
  void setBufferInputs(std::vector<backend::Buffer> const& inputs) final {
  }
  void execute(CommandEncoder& cmd) final {
  }

 protected:
  virtual backend::ShadersMap createShaderModules() const = 0;

  virtual RayTracingPipelineDescriptor_t pipelineDescriptor(backend::ShadersMap const& shaders_map) = 0;
};

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_RENDERER_FX_RAY_TRACING_FX_H_