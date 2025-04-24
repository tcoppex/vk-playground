#ifndef HELLOVK_FRAMEWORK_RENDERER_POST_FX_FRAGMENT_H
#define HELLOVK_FRAMEWORK_RENDERER_POST_FX_FRAGMENT_H

#include "framework/fx/_experimental/generic_fx.h"

#include "framework/renderer/_experimental/render_target.h"

/* -------------------------------------------------------------------------- */

class FragmentFx : public virtual GenericFx {
 public:
  static constexpr uint32_t kDefaultCombinedImageSamplerBinding{ 0u };
  static constexpr uint32_t kDefaultStorageBufferBinding{ 1u };

  static constexpr uint32_t kDefaultCombinedImageSamplerDescriptorCount{ 8u };
  static constexpr uint32_t kDefaultStorageBufferDescriptorCount{ 4u };

 protected:
  virtual std::string getVertexShaderName() const = 0;

  virtual GraphicsPipelineDescriptor_t getGraphicsPipelineDescriptor(std::vector<backend::ShaderModule> const& shaders) const = 0;

  virtual void draw(RenderPassEncoder const& pass) = 0;

  std::vector<DescriptorSetLayoutParams> getDescriptorSetLayoutParams() const override {
    return {
      {
        .binding = kDefaultCombinedImageSamplerBinding,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = kDefaultCombinedImageSamplerDescriptorCount,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
      },
      {
        .binding = kDefaultStorageBufferBinding,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = kDefaultStorageBufferDescriptorCount,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
      },
    };
  }

};

// ----------------------------------------------------------------------------

class RenderTargetFx : public PostGenericFx
                     , public FragmentFx {
 public:
  static std::string GetMapScreenVertexShaderName();

 public:
  void resize(VkExtent2D const dimension) override;

  void release() override;

  void setImageInputs(std::vector<backend::Image> const& inputs) override;

  void setBufferInputs(std::vector<backend::Buffer> const& inputs) override;

  void execute(CommandEncoder& cmd) override;

  backend::Image const& getImageOutput(uint32_t index = 0u) const override;

  virtual std::vector<backend::Image> const& getImageOutputs() const override;

  backend::Buffer const& getBufferOutput(uint32_t index = 0u) const override {
    return unused_buffers_[index];
  }

  std::vector<backend::Buffer> const& getBufferOutputs() const override {
    return unused_buffers_;
  }

 protected:
  std::string getVertexShaderName() const override {
    return GetMapScreenVertexShaderName();
  }

  GraphicsPipelineDescriptor_t getGraphicsPipelineDescriptor(std::vector<backend::ShaderModule> const& shaders) const override {
     return {
      .vertex = {
        .module = shaders[0u].module,
      },
      .fragment = {
        .module = shaders[1u].module,
        .targets = {
          { .format = render_target_->get_color_attachment().format },
        }
      },
      .primitive = {
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .cullMode = VK_CULL_MODE_BACK_BIT,
      },
    };
  }

  void draw(RenderPassEncoder const& pass) override {
    pass.draw(3u);
  }

  void createPipeline() override;

  virtual void createRenderTarget(VkExtent2D const dimension);

 protected:
  std::shared_ptr<RenderTarget> render_target_{};
  std::vector<backend::Buffer> unused_buffers_{};
};

/* -------------------------------------------------------------------------- */

#endif // HELLOVK_FRAMEWORK_RENDERER_POST_FX_FRAGMENT_H
