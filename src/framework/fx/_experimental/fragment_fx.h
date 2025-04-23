#ifndef HELLOVK_FRAMEWORK_RENDERER_POST_FX_FRAGMENT_H
#define HELLOVK_FRAMEWORK_RENDERER_POST_FX_FRAGMENT_H

#include "framework/fx/_experimental/generic_fx.h"

#include "framework/renderer/_experimental/render_target.h"

/* -------------------------------------------------------------------------- */

/*
  [WIP]

  Currently the default FragmentFx render into a screenmap triangle for
  post processing effects, but we should use it as interface and create
  two sub classes:
    * ScreenFx, to map image to the screen
    * SceneFx, to render scene data.
*/

class FragmentFx : public GenericFx {
 public:
  static constexpr uint32_t kDefaultCombinedImageSamplerBinding{ 0u };
  static constexpr uint32_t kDefaultStorageBufferBinding{ 1u };

  static constexpr uint32_t kDefaultCombinedImageSamplerDescriptorCount{ 8u };
  static constexpr uint32_t kDefaultStorageBufferDescriptorCount{ 4u };

 public:
  static std::string GetMapScreenVertexShaderName();

 public:
  FragmentFx() = default;

  virtual ~FragmentFx() {}

  void setup(VkExtent2D const dimension) override;

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
  virtual std::string getVertexShaderName() const {
    return GetMapScreenVertexShaderName();
  }

  virtual std::string getShaderName() const = 0;

  virtual void createRenderTarget(VkExtent2D const dimension);

  virtual GraphicsPipelineDescriptor_t getGraphicsPipelineDescriptor(std::vector<backend::ShaderModule> const& shaders) const {
     return {
      .vertex = {
        .module = shaders[0u].module
      },
      .fragment = {
        .module = shaders[1u].module,
        .targets = {
          { .format = render_target_->get_color_attachment().format },
        }
      },
      .primitive = {
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .cullMode = VK_CULL_MODE_NONE,
      },
    };
  }

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

  void createPipeline() override;

  virtual void draw(RenderPassEncoder const& pass) {
    pass.draw(3u);
  }

 protected:
  std::shared_ptr<RenderTarget> render_target_{};
  std::vector<backend::Buffer> unused_buffers_{};

  bool is_setup_{};
};

/* -------------------------------------------------------------------------- */

#endif // HELLOVK_FRAMEWORK_RENDERER_POST_FX_FRAGMENT_H
