#ifndef HELLOVK_FRAMEWORK_RENDERER_POST_FX_COMPUTE_H
#define HELLOVK_FRAMEWORK_RENDERER_POST_FX_COMPUTE_H

#include "framework/fx/_experimental/generic_fx.h"

/* -------------------------------------------------------------------------- */

class ComputeFx : public GenericFx {
 public:
  static constexpr uint32_t kDefaultStorageImageBinding{ 0u };
  static constexpr uint32_t kDefaultStorageBufferBinding{ 1u };

 public:
  ComputeFx() = default;

  virtual ~ComputeFx() {}

  virtual void resize(VkExtent2D const dimension) = 0;


 public:
  void release() override;

  void execute(CommandEncoder& cmd) override;

  void setImageInputs(std::vector<backend::Image> const& inputs) override;

  void setBufferInputs(std::vector<backend::Buffer> const& inputs) override;

  backend::Image const& getImageOutput(uint32_t index = 0u) const override {
    return images_.at(index);
  }

  std::vector<backend::Image> const& getImageOutputs() const override {
    return images_;
  }

  backend::Buffer const& getBufferOutput(uint32_t index = 0u) const override {
    return buffers_.at(index);
  }

  std::vector<backend::Buffer> const& getBufferOutputs() const override {
    return buffers_;
  }

 protected:
  virtual std::string getShaderName() const = 0;

  virtual void releaseImagesAndBuffers();

  std::vector<DescriptorSetLayoutParams> getDescriptorSetLayoutParams() const override {
    return {
      {
        .binding = kDefaultStorageImageBinding,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .descriptorCount = 4u, //
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
      },
      {
        .binding = kDefaultStorageBufferBinding,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 4u, //
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
      },
    };
  }

  void createPipeline() override;

 protected:
  std::vector<backend::Image> images_{};
  std::vector<backend::Buffer> buffers_{};
  VkExtent2D dimension_{}; //
};

/* -------------------------------------------------------------------------- */

#endif // HELLOVK_FRAMEWORK_RENDERER_POST_FX_COMPUTE_H
