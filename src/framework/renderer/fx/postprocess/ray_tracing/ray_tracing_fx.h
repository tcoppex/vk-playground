#ifndef VKFRAMEWORK_RENDERER_FX_PP_RT_RAY_TRACING_FX_H_
#define VKFRAMEWORK_RENDERER_FX_PP_RT_RAY_TRACING_FX_H_

// #include "framework/renderer/fx/postprocess/generic_fx.h"
#include "framework/renderer/fx/postprocess/post_generic_fx.h"

namespace backend {
struct TLAS;
}

/* -------------------------------------------------------------------------- */

class RayTracingFx : public virtual PostGenericFx {
 public:
  virtual void release() {
    allocator_ptr_->destroy_buffer(sbt_storage_);
    releaseOutputImagesAndBuffers();
    GenericFx::release();
  }

  virtual void setup(VkExtent2D const dimension) override {
    PostGenericFx::setup(dimension);
    
    context_ptr_->update_descriptor_set(descriptor_set_, {
      {
        .binding = 1, //
        .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .images = {
          {
            .imageView = out_images_[0].view,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
          }
        },
      },
    });
  }

  virtual void updateTLAS(backend::TLAS const& tlas);

  void execute(CommandEncoder& cmd) final;

 public:
  // ----------------------
  bool resize(VkExtent2D const dimension) override;

  backend::Image getImageOutput(uint32_t index = 0u) const final {
    return out_images_[index];
  }

  std::vector<backend::Image> const& getImageOutputs() const final {
    return out_images_;
  }

  backend::Buffer getBufferOutput(uint32_t index = 0u) const final {
    return out_buffers_[index];
  }

  std::vector<backend::Buffer> const& getBufferOutputs() const final {
    return out_buffers_;
  }
  // ----------------------

  void setImageInputs(std::vector<backend::Image> const& inputs) override {} //

  void setBufferInputs(std::vector<backend::Buffer> const& inputs) override {} //

 protected:
  virtual void resetMemoryBarriers();

  virtual backend::ShadersMap createShaderModules() const = 0;

  virtual RayTracingPipelineDescriptor_t pipelineDescriptor(backend::ShadersMap const& shaders_map) = 0;

  virtual void buildShaderBindingTable(RayTracingPipelineDescriptor_t const& desc);

 protected:
  DescriptorSetLayoutParamsBuffer getDescriptorSetLayoutParams() const override {
    return {
      {
        .binding = 0, //
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        .descriptorCount = 1u,
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
                    | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        .bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
      },
      {
        .binding = 1, //
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
        .bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
      },
    };
  }

  void createPipeline();

  void releaseOutputImagesAndBuffers() {
    for (auto &image : out_images_) {
      allocator_ptr_->destroy_image(&image);
    }
    out_images_.clear();

    for (auto &buffer : out_buffers_) {
      allocator_ptr_->destroy_buffer(buffer);
    }
    out_buffers_.clear();
  }

 protected:
  backend::Buffer sbt_storage_{};
  RayTracingAddressRegion region_{};

 protected:
  VkExtent2D dimension_{};
  std::vector<backend::Image> out_images_{};
  std::vector<backend::Buffer> out_buffers_{};

  struct {
    std::vector<VkImageMemoryBarrier2> images_start{};
    std::vector<VkImageMemoryBarrier2> images_end{};
  } barriers_;
};

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_RENDERER_FX_PP_RT_RAY_TRACING_FX_H_