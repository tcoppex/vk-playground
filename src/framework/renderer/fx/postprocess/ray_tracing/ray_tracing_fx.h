#ifndef VKFRAMEWORK_RENDERER_FX_RAY_TRACING_FX_H_
#define VKFRAMEWORK_RENDERER_FX_RAY_TRACING_FX_H_

#include "framework/renderer/fx/postprocess/generic_fx.h"

namespace backend {
struct TLAS;
}

/* -------------------------------------------------------------------------- */

class RayTracingFx : public virtual GenericFx {
 public:
  virtual void release() {
    allocator_ptr_->destroy_buffer(sbt_storage_);
    GenericFx::release();
  }

  virtual void setTLAS(backend::TLAS const& tlas);

 protected:
  std::string getShaderName() const final { return ""; } //

  DescriptorSetLayoutParamsBuffer getDescriptorSetLayoutParams() const override {
    return {
      {
        .binding = 0, //
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        .descriptorCount = 1u,
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
                    | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
      },
    };
  }

  void createPipeline();

  void setImageInputs(std::vector<backend::Image> const& inputs) final {
  }
  void setBufferInputs(std::vector<backend::Buffer> const& inputs) final {
  }

  void execute(CommandEncoder& cmd) final;

 protected:
  virtual backend::ShadersMap createShaderModules() const = 0;

  virtual RayTracingPipelineDescriptor_t pipelineDescriptor(backend::ShadersMap const& shaders_map) = 0;

  virtual void buildShaderBindingTable(RayTracingPipelineDescriptor_t const& desc);

 protected:
  backend::Buffer sbt_storage_{};
  RayTracingAddressRegion region_{};
};

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_RENDERER_FX_RAY_TRACING_FX_H_