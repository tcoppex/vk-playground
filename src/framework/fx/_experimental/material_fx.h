#ifndef HELLOVK_FRAMEWORK_FX_MATERIAL_FX_H_
#define HELLOVK_FRAMEWORK_FX_MATERIAL_FX_H_

#include "framework/fx/_experimental/fragment_fx.h"

#include "framework/scene/material.h" //

/* -------------------------------------------------------------------------- */

///
/// Specialized FragmentFx with custom material.
///
/// Ideally should derive from GenericFx rather than FragmentFx, but this would
/// requires to rethink the post processing pipeline. Maybe in the future.
///

// ----------------------------------------------------------------------------

class MaterialFx : public FragmentFx {
 public:
  struct CreatedMaterial {
    ::scene::MaterialRef ref;
    void* raw_ptr{};
  };

 public:
  void setup(VkExtent2D const dimension = {}) override {
    FragmentFx::setup(dimension);
  }

  void prepareDrawState(RenderPassEncoder const& pass) override {
    FragmentFx::prepareDrawState(pass);
  }

  void pushConstant(GenericCommandEncoder const& cmd) override {
    FragmentFx::pushConstant(cmd);
  }

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
    auto& mat = materials_.emplace_back();
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
  void setupMaterialStorageBuffer() {
    size_t buffersize = kDefaultMaterialCount * sizeof(MaterialType);

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

#endif // HELLOVK_FRAMEWORK_FX_MATERIAL_FX_H_
