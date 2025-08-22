#ifndef FRAMEWORK_FX_SCENE_PBR_METALLIC_ROUGHNESS_H_
#define FRAMEWORK_FX_SCENE_PBR_METALLIC_ROUGHNESS_H_

#include "framework/fx/_experimental/material_fx.h"

/* -------------------------------------------------------------------------- */

// [WIP]
// Currently not a PBR, just a placeholder for the default material.

namespace fx::scene {

namespace pbr_metallic_roughness::shader_interop {
#include "framework/shaders/scene/pbr_metallic_roughness/interop.h" //
}

using PBRMetallicRoughnessMaterial = pbr_metallic_roughness::shader_interop::Material;

// ----------------------------------------------------------------------------

class PBRMetallicRoughnessFx : public TMaterialFx<PBRMetallicRoughnessMaterial> {
 public:
   static constexpr uint32_t kMaxNumTextures = 128u; //

 public:
  void setup(VkExtent2D const dimension = {}) override {
    FragmentFx::setup(dimension);

    uniform_buffer_ = allocator_->create_buffer(
      sizeof(host_data_),
        VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT
      | VK_BUFFER_USAGE_TRANSFER_DST_BIT
    );

    // ---------------------------
    material_storage_buffer_ = allocator_->create_buffer(
      1024u * sizeof(pbr_metallic_roughness::shader_interop::Material), // xxx
        VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT
      | VK_BUFFER_USAGE_TRANSFER_DST_BIT
    );
    // ---------------------------

    context_ptr_->update_descriptor_set(descriptor_set_, {
      {
        .binding = pbr_metallic_roughness::shader_interop::kDescriptorSetBinding_UniformBuffer,
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .buffers = { { uniform_buffer_.buffer } },
      },
      // {
      //   .binding = pbr_metallic_roughness::shader_interop::kDescriptorSetBinding_MaterialStorageBuffer,
      //   .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      //   .buffers = { { material_storage_buffer_.buffer } },
      // },
      {
        .binding = pbr_metallic_roughness::shader_interop::kDescriptorSetBinding_IrradianceEnvMap,
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .images = {
          {
            .sampler = renderer_ptr_->get_default_sampler(), //
            .imageView = renderer_ptr_->skybox().irradiance_cubemap().view, //
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          }
        },
      },
    });
  }

  void release() override {
    allocator_->destroy_buffer(uniform_buffer_);
    allocator_->destroy_buffer(material_storage_buffer_);
    FragmentFx::release();
  }


 public:
  // ------------------------------------
  uint32_t getDescriptorSetTextureAtlasBinding() const final {
    return pbr_metallic_roughness::shader_interop::kDescriptorSetBinding_Sampler;
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
    push_constant_.model.worldMatrix = world_matrix;
  }

  void setMaterialIndex(uint32_t material_index) final {
    push_constant_.model.material_index = material_index;
  }

  void setInstanceIndex(uint32_t instance_index) final {
    push_constant_.model.instance_index = instance_index;
  }
  // ------------------------------------

  // ------------------------------------
  void setMaterial(::scene::MaterialRef const& material_ref) override {
    auto pbr = material(material_ref.material_index);

    push_constant_.model.material_index = material_ref.material_index;
    push_constant_.model.diffuse_texture_index = pbr.diffuse_texture_id;

    push_constant_.model.use_irradiance =  true //pbr.use_irradiance
                                        && renderer_ptr_->skybox().is_valid()
                                        ;
  }
  // ------------------------------------

 private:
  std::string getShaderName() const final {
    return FRAMEWORK_COMPILED_SHADERS_DIR "scene/pbr_metallic_roughness/scene.frag.glsl";
  }

  std::string getVertexShaderName() const final {
    return FRAMEWORK_COMPILED_SHADERS_DIR "scene/pbr_metallic_roughness/scene.vert.glsl";
  }

  std::vector<DescriptorSetLayoutParams> getDescriptorSetLayoutParams() const override {
    return {
      {
        .binding = pbr_metallic_roughness::shader_interop::kDescriptorSetBinding_UniformBuffer,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1u,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
                      | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT
                      | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
      },
      {
        .binding = pbr_metallic_roughness::shader_interop::kDescriptorSetBinding_Sampler,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = kMaxNumTextures,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
      },
      {
        .binding = pbr_metallic_roughness::shader_interop::kDescriptorSetBinding_IrradianceEnvMap,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1u,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
      },
      {
        .binding = pbr_metallic_roughness::shader_interop::kDescriptorSetBinding_MaterialStorageBuffer,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1u,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
                      | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT
                      | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
      },
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
    cmd.push_constant(push_constant_, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  }

  GraphicsPipelineDescriptor_t getGraphicsPipelineDescriptor(std::vector<backend::ShaderModule> const& shaders) const override {
    return {
      .dynamicStates = {
        VK_DYNAMIC_STATE_VERTEX_INPUT_EXT,
        VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
      },
      .vertex = {
        .module = shaders[0u].module,
      },
      .fragment = {
        .module = shaders[1u].module,
        .targets = {
          { .format = renderer_ptr_->get_color_attachment(0).format },
        },
      },
      .depthStencil = {
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
      },
      .primitive = {
        .cullMode = VK_CULL_MODE_BACK_BIT,
      }
    };
  }

 private:
  // -----
  // Application data (this could be shared).
  pbr_metallic_roughness::shader_interop::UniformData host_data_{};
  backend::Buffer uniform_buffer_{};
  // -----

  // Fx shared material data (move to MaterialFx ?).
  backend::Buffer material_storage_buffer_{};

  // Material instance data (along with the descriptor set).
  pbr_metallic_roughness::shader_interop::PushConstant push_constant_{}; //
};

}

/* -------------------------------------------------------------------------- */

#endif // FRAMEWORK_FX_SCENE_PBR_METALLIC_ROUGHNESS_H_
