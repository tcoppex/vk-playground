#include "framework/renderer/fx/material/material_fx.h"
#include "framework/backend/context.h"

/* -------------------------------------------------------------------------- */

void MaterialFx::init(Renderer const& renderer) {
  context_ptr_ = &renderer.context(); //
  renderer_ptr_ = &renderer;
  allocator_ptr_ = context_ptr_->allocator_ptr();
}

// ----------------------------------------------------------------------------

void MaterialFx::release() {
  if (pipeline_layout_ != VK_NULL_HANDLE) {
    for (auto [_, pipeline] : pipelines_) {
      renderer_ptr_->destroy_pipeline(pipeline);
    }
    renderer_ptr_->destroy_pipeline_layout(pipeline_layout_); //
    renderer_ptr_->destroy_descriptor_set_layout(descriptor_set_layout_);
    pipeline_layout_ = VK_NULL_HANDLE;
  }
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

void MaterialFx::prepareDrawState(
  RenderPassEncoder const& pass,
  scene::MaterialStates const& states
) {
  LOG_CHECK(pipelines_.contains(states));

  pass.bind_pipeline(pipelines_[states]);
  pass.bind_descriptor_set(
    descriptor_set_,
    pipeline_layout_,
    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
  );
}

// ----------------------------------------------------------------------------

void MaterialFx::createPipelineLayout() {
  LOG_CHECK(renderer_ptr_);

  descriptor_set_layout_ = renderer_ptr_->create_descriptor_set_layout(
    getDescriptorSetLayoutParams()
  );

  pipeline_layout_ = renderer_ptr_->create_pipeline_layout({
    .setLayouts = { descriptor_set_layout_ },
    .pushConstantRanges = getPushConstantRanges()
  });
}

// ----------------------------------------------------------------------------

GraphicsPipelineDescriptor_t MaterialFx::getGraphicsPipelineDescriptor(
  backend::ShaderMap const& shaders,
  scene::MaterialStates const& states
) const {
  LOG_CHECK(shaders.contains(backend::ShaderStage::Vertex));
  LOG_CHECK(shaders.contains(backend::ShaderStage::Fragment));

  GraphicsPipelineDescriptor_t desc{
    .dynamicStates = {
      VK_DYNAMIC_STATE_VERTEX_INPUT_EXT,
      VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
    },
    .vertex = {
      .module = shaders.at(backend::ShaderStage::Vertex).module,
    },
    .fragment = {
      .module = shaders.at(backend::ShaderStage::Fragment).module,
      .specializationConstants = {
        { 0u, VK_FALSE },  // kUseAlphaCutoff
      },
      .targets = {
        {
          .format = renderer_ptr_->get_color_attachment(0).format,
        },
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
  if (states.alpha_mode == scene::MaterialStates::AlphaMode::Mask) {
    desc.fragment.specializationConstants[0] = { 0u, VK_TRUE };
  }
  if (states.alpha_mode == scene::MaterialStates::AlphaMode::Blend) {
    desc.fragment.targets[0].blend = {
      .enable = VK_TRUE,
      .color = {
        .operation = VK_BLEND_OP_ADD,
        .srcFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      },
      .alpha =  {
        .operation = VK_BLEND_OP_ADD,
        .srcFactor = VK_BLEND_FACTOR_ONE,
        .dstFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      },
    };
  }
  return desc;
}

// ----------------------------------------------------------------------------

backend::ShaderMap MaterialFx::createShaderModules() const {
  return {
    {
      backend::ShaderStage::Vertex,
      context_ptr_->create_shader_module(getVertexShaderName())
    },
    {
      backend::ShaderStage::Fragment,
      context_ptr_->create_shader_module(getShaderName())
    },
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
