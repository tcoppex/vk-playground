#ifndef FRAMEWORK_FX_SCENE_PBR_METALLIC_ROUGHNESS_H_
#define FRAMEWORK_FX_SCENE_PBR_METALLIC_ROUGHNESS_H_

#include "framework/fx/_experimental/material_fx.h"

/* -------------------------------------------------------------------------- */

namespace fx::scene {

namespace pbr_metallic_roughness_shader_interop {
#include "framework/shaders/scene/pbr_metallic_roughness/interop.h"
}

using PBRMetallicRoughnessMaterial = pbr_metallic_roughness_shader_interop::Material;

// ----------------------------------------------------------------------------

class PBRMetallicRoughnessFx final : public TMaterialFx<PBRMetallicRoughnessMaterial> {
 public:
   static constexpr uint32_t kMaxNumTextures = 128u; //

 public:
  void setup(VkExtent2D const dimension = {}) final {
    TMaterialFx<PBRMetallicRoughnessMaterial>::setup(dimension);

    uniform_buffer_ = allocator_->create_buffer(
      sizeof(host_data_),
        VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT
      | VK_BUFFER_USAGE_TRANSFER_DST_BIT
    );

    auto const& skybox = renderer_ptr_->skybox();
    auto const& default_sampler = renderer_ptr_->get_default_sampler();

    context_ptr_->update_descriptor_set(descriptor_set_, {
      {
        .binding = pbr_metallic_roughness_shader_interop::kDescriptorSetBinding_UniformBuffer,
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .buffers = { { uniform_buffer_.buffer } },
      },
      {
        .binding = pbr_metallic_roughness_shader_interop::kDescriptorSetBinding_EnvMap_Prefiltered,
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .images = {
          {
            .sampler = default_sampler,
            .imageView = skybox.prefiltered_specular_map().view, //
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          }
        },
      },
      {
        .binding = pbr_metallic_roughness_shader_interop::kDescriptorSetBinding_EnvMap_Irradiance,
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .images = {
          {
            .sampler = default_sampler,
            .imageView = skybox.irradiance_map().view, //
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          }
        },
      },
      {
        .binding = pbr_metallic_roughness_shader_interop::kDescriptorSetBinding_SpecularBRDF,
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .images = {
          {
            .sampler = default_sampler,
            .imageView = skybox.specular_brdf_lut().view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          }
        },
      },
      // ---------------------------
      {
        .binding = pbr_metallic_roughness_shader_interop::kDescriptorSetBinding_MaterialStorageBuffer,
        .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .buffers = { { material_storage_buffer_.buffer } },
      },
      // ---------------------------
    });
  }

  void release() final {
    allocator_->destroy_buffer(uniform_buffer_);
    TMaterialFx<PBRMetallicRoughnessMaterial>::release();
  }

 public:
  // ------------------------------------
  uint32_t getDescriptorSetTextureAtlasBinding() const final {
    return pbr_metallic_roughness_shader_interop::kDescriptorSetBinding_TextureAtlas;
  }

  void pushUniforms() final {
    context_ptr_->transfer_host_to_device(
      &host_data_, sizeof(host_data_), uniform_buffer_
    );
  }

  void setProjectionMatrix(mat4 const& projection_matrix) final {
    host_data_.scene.projectionMatrix = projection_matrix;
  }

  void setCameraPosition(vec3 const& camera_position) final {
    push_constant_.cameraPosition = camera_position;
  }

  void setViewMatrix(mat4 const& view_matrix) final {
    push_constant_.viewMatrix = view_matrix;
  }

  void setWorldMatrix(mat4 const& world_matrix) final {
    push_constant_.worldMatrix = world_matrix;
  }
  // ------------------------------------

  void setMaterialIndex(uint32_t material_index) final {
    push_constant_.material_index = material_index;
  }

  void setInstanceIndex(uint32_t instance_index) final {
    push_constant_.instance_index = instance_index;
  }

 private:
  std::string getShaderName() const final {
    return FRAMEWORK_COMPILED_SHADERS_DIR "scene/pbr_metallic_roughness/scene.frag.glsl";
  }

  std::string getVertexShaderName() const final {
    return FRAMEWORK_COMPILED_SHADERS_DIR "scene/pbr_metallic_roughness/scene.vert.glsl";
  }

  std::vector<DescriptorSetLayoutParams> getDescriptorSetLayoutParams() const final {
    return {
      {
        .binding = pbr_metallic_roughness_shader_interop::kDescriptorSetBinding_UniformBuffer,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1u,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
                      | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT
                      | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
      },
      {
        .binding = pbr_metallic_roughness_shader_interop::kDescriptorSetBinding_TextureAtlas,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = kMaxNumTextures,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
      },
      {
        .binding = pbr_metallic_roughness_shader_interop::kDescriptorSetBinding_EnvMap_Prefiltered,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1u,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
      },
      {
        .binding = pbr_metallic_roughness_shader_interop::kDescriptorSetBinding_EnvMap_Irradiance,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1u,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
      },
      {
        .binding = pbr_metallic_roughness_shader_interop::kDescriptorSetBinding_SpecularBRDF,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1u,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
      },
      // -----------------------------------
      {
        .binding = pbr_metallic_roughness_shader_interop::kDescriptorSetBinding_MaterialStorageBuffer,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1u,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
                      | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT
                      | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
      },
      // -----------------------------------
    };
  }

  std::vector<VkPushConstantRange> getPushConstantRanges() const final {
    return {
      {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(push_constant_),
      }
    };
  }

  void updatePushConstant(GenericCommandEncoder const &cmd) final {
    push_constant_.enable_irradiance = renderer_ptr_->skybox().is_valid(); //
    cmd.push_constant(push_constant_, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  }

 private:
  pbr_metallic_roughness_shader_interop::PushConstant push_constant_{};

  // -----
  // Application data (should probably be shared).
  pbr_metallic_roughness_shader_interop::UniformData host_data_{};
  backend::Buffer uniform_buffer_{};
  // -----
};

}

/* -------------------------------------------------------------------------- */

#endif // FRAMEWORK_FX_SCENE_PBR_METALLIC_ROUGHNESS_H_
