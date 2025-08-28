#ifndef HELLOVK_FRAMEWORK_FX_MATERIAL_FX_H_
#define HELLOVK_FRAMEWORK_FX_MATERIAL_FX_H_

#include "framework/common.h"
#include "framework/renderer/renderer.h"
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

  virtual void init(Context const& context, Renderer const& renderer) {
    context_ptr_ = &context;
    renderer_ptr_ = &renderer;
    allocator_ = context.get_resource_allocator();
  }

  virtual void setup(VkExtent2D const dimension = {}) {
    LOG_CHECK(nullptr != context_ptr_);
    createPipelineLayout();
    createPipeline();
    descriptor_set_ = renderer_ptr_->create_descriptor_set(descriptor_set_layout_); //
  }

  virtual void release() {
    if (pipeline_layout_ != VK_NULL_HANDLE) {
      renderer_ptr_->destroy_pipeline(pipeline_);
      renderer_ptr_->destroy_pipeline_layout(pipeline_layout_); //
      renderer_ptr_->destroy_descriptor_set_layout(descriptor_set_layout_);
      pipeline_layout_ = VK_NULL_HANDLE;
    }
  }

  virtual void prepareDrawState(RenderPassEncoder const& pass) {
    pass.bind_pipeline(pipeline_);
    pass.bind_descriptor_set(descriptor_set_, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    pass.set_viewport_scissor(renderer_ptr_->get_surface_size()); //
  }

  virtual void pushConstant(GenericCommandEncoder const& cmd) = 0;

 protected:
  virtual std::string getVertexShaderName() const = 0;

  virtual std::string getShaderName() const = 0;

  virtual std::vector<VkPushConstantRange> getPushConstantRanges() const {
    return {};
  }

  virtual std::vector<DescriptorSetLayoutParams> getDescriptorSetLayoutParams() const {
    return {}; //
  }

  virtual void createPipelineLayout() {
    descriptor_set_layout_ = renderer_ptr_->create_descriptor_set_layout(
      getDescriptorSetLayoutParams()
    );
    pipeline_layout_ = renderer_ptr_->create_pipeline_layout({
      .setLayouts = { descriptor_set_layout_ },
      .pushConstantRanges = getPushConstantRanges()
    });
  }

  virtual GraphicsPipelineDescriptor_t getGraphicsPipelineDescriptor(std::vector<backend::ShaderModule> const& shaders) const {
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
          {
            .format = renderer_ptr_->get_color_attachment(0).format,
            // .blend = {
            //   .enable = VK_TRUE,
            //   .color = {
            //     .operation = VK_BLEND_OP_ADD,
            //     .srcFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            //     .dstFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            //   },
            //   .alpha =  {
            //     .operation = VK_BLEND_OP_ADD,
            //     .srcFactor = VK_BLEND_FACTOR_ONE,
            //     .dstFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            //   },
            // }
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
  }

  virtual void createPipeline() {
    auto shaders{context_ptr_->create_shader_modules({
      getVertexShaderName(),
      getShaderName()
    })};
    pipeline_ = renderer_ptr_->create_graphics_pipeline(
      pipeline_layout_,
      getGraphicsPipelineDescriptor(shaders)
    );
    context_ptr_->release_shader_modules(shaders);
  }

 public:
  // -- frame-wide resource descriptor --

  void updateDescriptorSetTextureAtlasEntry(DescriptorSetWriteEntry const& entry) const {
    context_ptr_->update_descriptor_set(descriptor_set_, { entry });
  }

  void updateDescriptorSetFrameUBO(backend::Buffer const& buf) const {
    context_ptr_->update_descriptor_set(descriptor_set_, {{
      .binding = getFrameUniformBufferBinding(),
      .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .buffers = { { buf.buffer } },
    }});
  }

  void updateDescriptorSetTransformsSSBO(backend::Buffer const& buf) const {
    context_ptr_->update_descriptor_set(descriptor_set_, {{
      .binding = getTransformsStorageBufferBinding(),
      .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .buffers = { { buf.buffer } },
    }});
  }

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

  virtual CreatedMaterial createMaterial() = 0;

  virtual void pushMaterialStorageBuffer() const = 0;

  // virtual void setMaterial(::scene::MaterialRef const& material_ref) = 0; //
  // virtual void buildMaterialUI(::scene::MaterialRef const& material_ref) {}

 private:
  // (we might probably discard them altogether)
  // ------------------------------------
  void draw(RenderPassEncoder const& pass) override {} //
  void execute(CommandEncoder& cmd) override {} //
  // ------------------------------------

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

 protected:
  backend::Buffer material_storage_buffer_{};
};

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
  void setup(VkExtent2D const dimension) override {
    MaterialFx::setup(dimension);
    setupMaterialStorageBuffer();
  }

  void release() override {
    allocator_->destroy_buffer(material_storage_buffer_);
    MaterialFx::release();
  }

  CreatedMaterial createMaterial() override {
    auto& mat = materials_.emplace_back(defaultMaterial());
    scene::MaterialRef ref = {
      .material_type_index = MaterialTypeIndex(),
      .material_index = static_cast<uint32_t>(materials_.size() - 1u),
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

  // To handle the 3 kind of pipeline properly, as we don't have access to dynamic
  //  blending extension, it might be interesting to create a PipelineCache
  // and maybe store the AlphaMode on alongside materials..
  // scene::AlphaMode alpha_mode_; //
};

/* -------------------------------------------------------------------------- */

#endif // HELLOVK_FRAMEWORK_FX_MATERIAL_FX_H_
