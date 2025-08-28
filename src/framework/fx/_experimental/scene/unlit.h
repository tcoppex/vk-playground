#ifndef FRAMEWORK_FX_SCENE_UNLIT_H_
#define FRAMEWORK_FX_SCENE_UNLIT_H_

#include "framework/fx/_experimental/material_fx.h"

/* -------------------------------------------------------------------------- */

namespace fx::scene {

namespace unlit_shader_interop {
#include "framework/shaders/scene/unlit/interop.h"
}

// ----------------------------------------------------------------------------

class UnlitMaterialFx final : public TMaterialFx<unlit_shader_interop::Material> {
 public:
    void setup(VkExtent2D const dimension = {}) final {
    TMaterialFx<unlit_shader_interop::Material>::setup(dimension);

    context_ptr_->update_descriptor_set(descriptor_set_, {
      {
        .binding = unlit_shader_interop::kDescriptorSetBinding_MaterialSSBO,
        .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .buffers = { { material_storage_buffer_.buffer } },
      },
    });
  }

 public:
  uint32_t getFrameUniformBufferBinding() const final {
    return unlit_shader_interop::kDescriptorSetBinding_FrameUBO;
  }

  uint32_t getTransformsStorageBufferBinding() const final {
    return unlit_shader_interop::kDescriptorSetBinding_TransformSSBO;
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
    return FRAMEWORK_COMPILED_SHADERS_DIR "scene/unlit/scene.frag.glsl";
  }

  std::string getVertexShaderName() const final {
    return FRAMEWORK_COMPILED_SHADERS_DIR "scene/unlit/scene.vert.glsl";
  }

  std::vector<DescriptorSetLayoutParams> getDescriptorSetLayoutParams() const final {
    return {
      {
        .binding = getFrameUniformBufferBinding(),
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1u,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
                    | VK_SHADER_STAGE_FRAGMENT_BIT
                    ,
        .bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
                      | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT
                      | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
      },
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
      {
        .binding = unlit_shader_interop::kDescriptorSetBinding_MaterialSSBO,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1u,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
                    | VK_SHADER_STAGE_FRAGMENT_BIT,
        .bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
                      | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT
                      | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
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
    cmd.push_constant(push_constant_, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  }

 private:
  unlit_shader_interop::PushConstant push_constant_{};
};

}

/* -------------------------------------------------------------------------- */

#endif // FRAMEWORK_FX_SCENE_UNLIT_H_
