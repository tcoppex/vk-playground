#ifndef VKFRAMEWORK_RENDERER_FX_MATERIAL_IMPL_PBR_METALLIC_ROUGHNESS_H_
#define VKFRAMEWORK_RENDERER_FX_MATERIAL_IMPL_PBR_METALLIC_ROUGHNESS_H_

#include "framework/renderer/fx/material/material_fx.h"

/* -------------------------------------------------------------------------- */

namespace fx::material {

namespace pbr_metallic_roughness_shader_interop {
#include "framework/shaders/material/pbr_metallic_roughness/interop.h"
}

using PBRMetallicRoughnessMaterial = pbr_metallic_roughness_shader_interop::Material;

// ----------------------------------------------------------------------------

class PBRMetallicRoughnessFx final : public TMaterialFx<PBRMetallicRoughnessMaterial> {
 public:
   static constexpr uint32_t kMaxNumTextures = 128u; //

 public:
  void setup() final {
    TMaterialFx<PBRMetallicRoughnessMaterial>::setup();

    auto const& skybox = renderer_ptr_->skybox();
    auto const& ibl_sampler = skybox.sampler(); // ClampToEdge Linear MipMap
    
    context_ptr_->update_descriptor_set(descriptor_set_, {
      // Image-Based Lighting inputs.
      {
        .binding = pbr_metallic_roughness_shader_interop::kDescriptorSetBinding_IBL_Prefiltered,
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .images = {
          {
            .sampler = ibl_sampler,
            .imageView = skybox.prefiltered_specular_map().view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          }
        },
      },
      {
        .binding = pbr_metallic_roughness_shader_interop::kDescriptorSetBinding_IBL_Irradiance,
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .images = {
          {
            .sampler = ibl_sampler,
            .imageView = skybox.irradiance_map().view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          }
        },
      },
      {
        .binding = pbr_metallic_roughness_shader_interop::kDescriptorSetBinding_IBL_SpecularBRDF,
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .images = {
          {
            .sampler = ibl_sampler,
            .imageView = skybox.specular_brdf_lut().view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          }
        },
      },

      // MaterialFx instance materials SSBO.
      {
        .binding = pbr_metallic_roughness_shader_interop::kDescriptorSetBinding_MaterialSSBO,
        .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .buffers = { { material_storage_buffer_.buffer } },
      },
    });
  }

  void release() final {
    TMaterialFx<PBRMetallicRoughnessMaterial>::release();
  }

 public:
  uint32_t getFrameUniformBufferBinding() const final {
    return pbr_metallic_roughness_shader_interop::kDescriptorSetBinding_FrameUBO;
  }

  uint32_t getTransformsStorageBufferBinding() const final {
    return pbr_metallic_roughness_shader_interop::kDescriptorSetBinding_TransformSSBO;
  }

  uint32_t getTextureAtlasBinding() const final {
    return pbr_metallic_roughness_shader_interop::kDescriptorSetBinding_TextureAtlas;
  }

  void setTransformIndex(uint32_t index) final {
    push_constant_.transform_index = index;
  }

  void setMaterialIndex(uint32_t index) final {
    push_constant_.material_index = index;
  }

  void setInstanceIndex(uint32_t index) final {
    push_constant_.instance_index = index;
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
        .binding = getFrameUniformBufferBinding(),
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1u,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
                    | VK_SHADER_STAGE_FRAGMENT_BIT
      },
      // ----------------------------------------------
      {
        .binding = getTransformsStorageBufferBinding(),
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1u,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
                    | VK_SHADER_STAGE_FRAGMENT_BIT,
        .bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
                      | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT
                      | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
      },
      // ----------------------------------------------
      {
        .binding = getTextureAtlasBinding(),
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = kMaxNumTextures, //
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
                    | VK_SHADER_STAGE_FRAGMENT_BIT,
        .bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
                      | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
      },
      // ----------------------------------------------
      {
        .binding = pbr_metallic_roughness_shader_interop::kDescriptorSetBinding_IBL_Prefiltered,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1u,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .bindingFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
      },
      {
        .binding = pbr_metallic_roughness_shader_interop::kDescriptorSetBinding_IBL_Irradiance,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1u,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .bindingFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
      },
      {
        .binding = pbr_metallic_roughness_shader_interop::kDescriptorSetBinding_IBL_SpecularBRDF,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1u,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .bindingFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
      },
      // ----------------------------------------------
      {
        .binding = pbr_metallic_roughness_shader_interop::kDescriptorSetBinding_MaterialSSBO,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1u,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
                    | VK_SHADER_STAGE_FRAGMENT_BIT,
      },
    };
  }

  std::vector<VkPushConstantRange> getPushConstantRanges() const final {
    return {
      {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
                    | VK_SHADER_STAGE_FRAGMENT_BIT
                    ,
        .size = sizeof(push_constant_),
      }
    };
  }

  void pushConstant(GenericCommandEncoder const &cmd) final {
    // -------------------------
    if (renderer_ptr_->skybox().is_valid()) {
      push_constant_.dynamic_states |= pbr_metallic_roughness_shader_interop::kIrradianceBit;
    } else {
      push_constant_.dynamic_states &= ~pbr_metallic_roughness_shader_interop::kIrradianceBit;
    }
    // -------------------------
    cmd.push_constant(push_constant_, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  }

 private:
  ShaderMaterial convertMaterialProxy(scene::MaterialProxy const& proxy) const final {
    return {
      .emissive_factor = proxy.emissive_factor,
      .emissive_texture_id = proxy.bindings.emissive,
      .diffuse_factor = proxy.pbr_mr.basecolor_factor,
      .diffuse_texture_id = proxy.bindings.basecolor,
      .orm_texture_id = proxy.bindings.roughness_metallic,
      .metallic_factor = proxy.pbr_mr.metallic_factor,
      .roughness_factor = proxy.pbr_mr.roughness_factor,
      .normal_texture_id = proxy.bindings.normal,
      .occlusion_texture_id = proxy.bindings.occlusion,
      .alpha_cutoff = proxy.alpha_cutoff,
      .double_sided = proxy.double_sided,
    };
  }

 private:
  pbr_metallic_roughness_shader_interop::PushConstant push_constant_{};
};

}

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_RENDERER_FX_MATERIAL_IMPL_PBR_METALLIC_ROUGHNESS_H_
