#ifndef VKFRAMEWORK_RENDERER_FX_MATERIAL_MATERIAL_FX_H_
#define VKFRAMEWORK_RENDERER_FX_MATERIAL_MATERIAL_FX_H_

#include "framework/common.h"
#include "framework/renderer/renderer.h"
#include "framework/backend/context.h" //

#include "framework/scene/material.h" //

/* -------------------------------------------------------------------------- */

class MaterialFx {
 public:
  struct CreatedMaterial {
    ::scene::MaterialRef ref;
    void* raw_ptr{};
  };

 public:
  MaterialFx() = default;
  virtual ~MaterialFx() {}

  virtual void init(Context const& context, Renderer const& renderer);

  virtual void setup() {
    createPipelineLayout();
    createDescriptorSets();
  }

  virtual void release() {
    if (pipeline_layout_ != VK_NULL_HANDLE) {
      for (auto [_, pipeline] : pipelines_) {
        renderer_ptr_->destroy_pipeline(pipeline);
      }
      renderer_ptr_->destroy_pipeline_layout(pipeline_layout_); //
      renderer_ptr_->destroy_descriptor_set_layout(descriptor_set_layout_);
      pipeline_layout_ = VK_NULL_HANDLE;
    }
  }

  virtual void createPipelines(std::vector<scene::MaterialStates> const& states);

  virtual void prepareDrawState(RenderPassEncoder const& pass, scene::MaterialStates const& states) {
    LOG_CHECK(pipelines_.contains(states));

    pass.set_viewport_scissor(renderer_ptr_->get_surface_size()); //

    pass.bind_pipeline(pipelines_[states]);
    pass.bind_descriptor_set(descriptor_set_, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  }

  virtual void pushConstant(GenericCommandEncoder const& cmd) = 0;

  /* Check if the MaterialFx has been setup. */
  bool valid() const {
    return pipeline_layout_ != VK_NULL_HANDLE; //
  }

 protected:
  virtual std::string getVertexShaderName() const = 0;

  virtual std::string getShaderName() const = 0;

  virtual backend::ShaderMap createShaderModules() const;

  virtual std::vector<DescriptorSetLayoutParams> getDescriptorSetLayoutParams() const {
    return {}; //
  }

  virtual std::vector<VkPushConstantRange> getPushConstantRanges() const {
    return {};
  }

  virtual void createPipelineLayout() {
    LOG_CHECK(renderer_ptr_);

    descriptor_set_layout_ = renderer_ptr_->create_descriptor_set_layout(
      getDescriptorSetLayoutParams()
    );

    pipeline_layout_ = renderer_ptr_->create_pipeline_layout({
      .setLayouts = { descriptor_set_layout_ },
      .pushConstantRanges = getPushConstantRanges()
    });
  }

  virtual void createDescriptorSets() {
    descriptor_set_ = renderer_ptr_->create_descriptor_set(descriptor_set_layout_); //
  }

 protected:
  virtual GraphicsPipelineDescriptor_t getGraphicsPipelineDescriptor(backend::ShaderMap const& shaders, scene::MaterialStates const& states) const {
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

 public:
  // -- frame-wide resource descriptor --

  void updateDescriptorSetTextureAtlasEntry(DescriptorSetWriteEntry const& entry) const;

  void updateDescriptorSetFrameUBO(backend::Buffer const& buf) const;

  void updateDescriptorSetTransformsSSBO(backend::Buffer const& buf) const;

  virtual uint32_t getFrameUniformBufferBinding() const = 0;

  virtual uint32_t getTransformsStorageBufferBinding() const = 0;

  virtual uint32_t getTextureAtlasBinding() const {
    return kInvalidIndexU32;
  }

  // -- mesh instance push constants --

  virtual void setTransformIndex(uint32_t index) = 0;
  virtual void setMaterialIndex(uint32_t index) = 0;
  virtual void setInstanceIndex(uint32_t index) = 0;

  // -- material utils --

  virtual CreatedMaterial createMaterial(scene::MaterialStates sates) = 0;

  virtual void pushMaterialStorageBuffer() const = 0;

 protected:
  Context const* context_ptr_{};
  Renderer const* renderer_ptr_{};
  std::shared_ptr<ResourceAllocator> allocator_{};

  // ----------------
  VkDescriptorSetLayout descriptor_set_layout_{};
  VkDescriptorSet descriptor_set_{}; //
  VkPipelineLayout pipeline_layout_{}; //
  // ----------------

  // Pipeline default_pipeline_{};
  std::map<scene::MaterialStates, Pipeline> pipelines_{};

  backend::Buffer material_storage_buffer_{};
};

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

template<typename MaterialT>
class TMaterialFx : public MaterialFx {
 public:
  using MaterialType = MaterialT;

  static constexpr uint32_t kDefaultMaterialCount{ 1024u }; //
  static constexpr bool kEditMode{ false };

  static
  std::type_index MaterialTypeIndex() {
    return std::type_index(typeid(MaterialType));
  }

 public:
  void setup() override {
    MaterialFx::setup();
    setupMaterialStorageBuffer();
  }

  void release() override {
    allocator_->destroy_buffer(material_storage_buffer_);
    MaterialFx::release();
  }

  CreatedMaterial createMaterial(scene::MaterialStates states) override {
    auto& mat = materials_.emplace_back(defaultMaterial());
    scene::MaterialRef ref = {
      .index = kInvalidIndexU32,
      .material_type_index = MaterialTypeIndex(),
      .material_index = static_cast<uint32_t>(materials_.size() - 1u),
      .states = states,
    };
    return { ref, &mat };
  }

  void pushMaterialStorageBuffer() const override {
    LOG_CHECK(materials_.size() < kDefaultMaterialCount);

    if (materials_.empty()) {
      return;
    }

    if constexpr (kEditMode) {
      allocator_->upload_host_to_device(
        materials_.data(),
        materials_.size() * sizeof(MaterialType),
        material_storage_buffer_
      );
    } else {
      context_ptr_->transfer_host_to_device(
        materials_.data(),
        materials_.size() * sizeof(MaterialType),
        material_storage_buffer_
      );
    }
  }

  MaterialType const& material(uint32_t index) const {
    return materials_[index];
  }

 private:
  virtual MaterialType defaultMaterial() const {
   return {};
  }

  void setupMaterialStorageBuffer() {
    size_t const buffersize = kDefaultMaterialCount * sizeof(MaterialType);
    materials_.reserve(kDefaultMaterialCount);

    if constexpr (kEditMode) {
      // Setup the SSBO for frequent host-device mapping (slower).
      material_storage_buffer_ = allocator_->create_buffer(
        buffersize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
      | VMA_ALLOCATION_CREATE_MAPPED_BIT
      );
    } else {
      // Setup the SSBO for rarer device-to-device transfer.
      material_storage_buffer_ = allocator_->create_buffer(
        buffersize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
      );
    }
  }

 protected:
  std::vector<MaterialType> materials_{};
};

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_RENDERER_FX_MATERIAL_MATERIAL_FX_H_
