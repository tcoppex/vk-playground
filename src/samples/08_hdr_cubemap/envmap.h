#ifndef HELLOVK_FRAMEWORK_RENDERER_ENVMAP_H
#define HELLOVK_FRAMEWORK_RENDERER_ENVMAP_H

#include "framework/common.h"

#include "framework/backend/command_encoder.h"
#include "framework/renderer/pipeline.h"

namespace shader_interop::envmap {
#include "shaders/envmap_interop.h" //
}

class Context;
class Renderer;

/* -------------------------------------------------------------------------- */

class Envmap {
 public:
  static constexpr uint32_t kDiffuseResolution{ 1024u };
  static constexpr uint32_t kIrradianceResolution{ 256u };
  static constexpr uint32_t kSpecularResolution{ 256u };
  static constexpr uint32_t kSpecularSampleCount{ 64u };

  static const uint32_t kSpecularLevelCount{
    static_cast<uint32_t>(std::log2(kSpecularResolution))
  };

 public:
  enum class ImageType {
    Diffuse,
    Irradiance,
    Specular,

    kCount
  };

  enum class ComputeStage {
    TransformSpherical,
    IrradianceSHCoeff,
    ReduceSHCoeff,
    IrradianceTransfer,
    Irradiance,
    Specular,

    kCount,
  };

 public:
  Envmap() = default;

  void init(Context const& context, Renderer const& renderer);

  void release();

  bool setup(std::string_view filename);

  backend::Image const& get_image(ImageType const type) const {
    return images_[type];
  }

  backend::Buffer const& get_irradiance_matrices_buffer() const {
    return irradiance_matrices_buffer_;
  }

 private:
  bool load_diffuse_envmap(std::string_view filename);

  void compute_irradiance_sh_coeff();

  void compute_irradiance();

  void compute_specular();

 private:
  Context const* context_{};
  Renderer const* renderer_{};

  std::shared_ptr<ResourceAllocator> allocator_{};

  VkDescriptorSetLayout descriptor_set_layout_{};
  VkDescriptorSet descriptor_set_{};
  shader_interop::envmap::PushConstant push_constant_{};

  VkPipelineLayout pipeline_layout_{};
  EnumArray<Pipeline, ComputeStage> compute_pipelines_{};

  VkSampler sampler_{}; //
  EnumArray<backend::Image, ImageType> images_{};

  backend::Buffer irradiance_matrices_buffer_{};
};

/* -------------------------------------------------------------------------- */

#endif // HELLOVK_FRAMEWORK_RENDERER_ENVMAP_H
