#include "framework/renderer/fx/material/material_fx.h"
#include "framework/backend/context.h"

/* -------------------------------------------------------------------------- */

void MaterialFx::init(Context const& context, Renderer const& renderer) {
  context_ptr_ = &context;
  renderer_ptr_ = &renderer;
  allocator_ptr_ = context.allocator_ptr();
}

// ----------------------------------------------------------------------------

void MaterialFx::createPipelines(std::vector<scene::MaterialStates> const& states) {
  auto shaders = createShaderModules();

  // ------------------------------
  // Retrieve specific descriptors.
  std::vector<GraphicsPipelineDescriptor_t> descs{};
  descs.reserve(states.size());
  for (auto const& s : states) {
    descs.push_back( getGraphicsPipelineDescriptor(shaders, s) );
  }

  // Batch create the pipelines.
  std::vector<Pipeline> pipelines{};
  pipelines.reserve(states.size());
  renderer_ptr_->create_graphics_pipelines(pipeline_layout_, descs, &pipelines);

  // Store them into the pipeline map.
  for (size_t i = 0; i < states.size(); ++i) {
    pipelines_[states[i]] = pipelines[i];
  }
  // ------------------------------

  for (auto const& [_, shader] : shaders) {
    context_ptr_->release_shader_module(shader);
  }
}

// ----------------------------------------------------------------------------

backend::ShaderMap MaterialFx::createShaderModules() const {
  return {
    { backend::ShaderStage::Vertex, context_ptr_->create_shader_module(getVertexShaderName()) },
    { backend::ShaderStage::Fragment, context_ptr_->create_shader_module(getShaderName()) },
  };
}

// ----------------------------------------------------------------------------

void MaterialFx::updateDescriptorSetTextureAtlasEntry(DescriptorSetWriteEntry const& entry) const {
  context_ptr_->update_descriptor_set(descriptor_set_, { entry });
}

// ----------------------------------------------------------------------------

void MaterialFx::updateDescriptorSetFrameUBO(backend::Buffer const& buf) const {
  context_ptr_->update_descriptor_set(descriptor_set_, {{
    .binding = getFrameUniformBufferBinding(),
    .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .buffers = { { buf.buffer } },
  }});
}

// ----------------------------------------------------------------------------

void MaterialFx::updateDescriptorSetTransformsSSBO(backend::Buffer const& buf) const {
  context_ptr_->update_descriptor_set(descriptor_set_, {{
    .binding = getTransformsStorageBufferBinding(),
    .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    .buffers = { { buf.buffer } },
  }});
}

/* -------------------------------------------------------------------------- */
