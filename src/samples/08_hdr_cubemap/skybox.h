#ifndef HELLO_VK_SKYBOX_H
#define HELLO_VK_SKYBOX_H

#include "framework/common.h"

#include "framework/backend/command_encoder.h"
#include "framework/renderer/pipeline.h"
#include "framework/scene/mesh.h"

#include "envmap.h" //

namespace shader_interop::skybox {
#include "shaders/skybox_interop.h" //
}

class Context;
class Renderer;
class Camera;

/* -------------------------------------------------------------------------- */

class Skybox {
 public:
  static constexpr uint32_t kBRDFLutResolution{ 512u };

 public:
  Skybox() = default;

  void init(Context const& context, Renderer const& renderer);

  void release(Context const& context, Renderer const& renderer);

  bool setup(std::string_view hdr_filename); //

  void render(RenderPassEncoder& pass, Camera const& camera);

  Envmap const& get_envmap() const {
    return envmap_;
  }

  backend::Image const& get_irradiance_cubemap() const {
    return envmap_.get_image(Envmap::ImageType::Irradiance);
  }

 private:
  void compute_brdf_lut();

 private:
  Envmap envmap_{};

  backend::Image brdf_lut_{}; //
  VkSampler sampler_{}; //

  scene::Mesh cube_{};
  backend::Buffer vertex_buffer_{};
  backend::Buffer index_buffer_{};

  VkDescriptorSetLayout descriptor_set_layout_{};
  VkDescriptorSet descriptor_set_{};
  shader_interop::skybox::PushConstant push_constant_{}; // (rename namespace)

  VkPipelineLayout pipeline_layout_{};
  Pipeline graphics_pipeline_{};
};

/* -------------------------------------------------------------------------- */

#endif // HELLO_VK_SKYBOX_H
