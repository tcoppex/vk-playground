
#include "framework/fx/_experimental/compute_fx.h"
#include "framework/renderer/_experimental/render_target.h"

#include "framework/backend/command_encoder.h"

/* -------------------------------------------------------------------------- */

void ComputeFx::release() {
  releaseImagesAndBuffers();
  GenericFx::release();
}

// ----------------------------------------------------------------------------

void ComputeFx::setImageInputs(std::vector<backend::Image> const& inputs) {
  DescriptorSetWriteEntry write_entry{
    .binding = kDefaultStorageImageBinding,
    .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
  };
  for (auto const& input : inputs) {
    write_entry.images.push_back({
      .imageView = input.view,
      .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    });
  }
  renderer_ptr_->update_descriptor_set(descriptor_set_, { write_entry });
}

// ----------------------------------------------------------------------------

void ComputeFx::setBufferInputs(std::vector<backend::Buffer> const& inputs) {
  DescriptorSetWriteEntry write_entry{
    .binding = kDefaultStorageBufferBinding,
    .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
  };
  for (auto const& input : inputs) {
    write_entry.buffers.push_back({
      .buffer = input.buffer,
      .offset = 0,
      .range = VK_WHOLE_SIZE,
    });
  }
  renderer_ptr_->update_descriptor_set(descriptor_set_, { write_entry });
}

// ----------------------------------------------------------------------------

void ComputeFx::execute(CommandEncoder& cmd) {
  if (!isEnabled()) {
    return;
  }

  cmd.bind_pipeline(pipeline_);
  cmd.bind_descriptor_set(descriptor_set_, pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT);
  updatePushConstant(cmd);

  cmd.dispatch<32u, 32u>(
    static_cast<uint32_t>(dimension_.width),
    static_cast<uint32_t>(dimension_.height)
  );

  std::vector<VkImageMemoryBarrier2> image_barriers(
    images_.size(),
    VkImageMemoryBarrier2{
      .oldLayout = VK_IMAGE_LAYOUT_GENERAL, //
      .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, //
      .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } //
    }
  );
  for (size_t i = 0u; i < images_.size(); ++i) {
    image_barriers[i].image = images_[i].image;
  }

  std::vector<VkBufferMemoryBarrier2> buffer_barriers(
    buffers_.size(),
    VkBufferMemoryBarrier2{
      .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT
                    | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
                    ,
      .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
    }
  );
  for (size_t i = 0u; i < buffers_.size(); ++i) {
    buffer_barriers[i].buffer = buffers_[i].buffer;
  }

  if (!image_barriers.empty()) {
    cmd.pipeline_image_barriers(image_barriers);
  }
  if (!buffer_barriers.empty()) {
    cmd.pipeline_buffer_barriers(buffer_barriers);
  }
}

/* -------------------------------------------------------------------------- */

void ComputeFx::releaseImagesAndBuffers() {
  for (auto &image : images_) {
    allocator_->destroy_image(&image);
  }
  for (auto &buffer : buffers_) {
    allocator_->destroy_buffer(buffer);
  }
}

// ----------------------------------------------------------------------------

void ComputeFx::createPipeline() {
  auto cs_shader{context_ptr_->create_shader_module( getShaderName() )};
  pipeline_ = renderer_ptr_->create_compute_pipeline(pipeline_layout_, cs_shader);
  context_ptr_->release_shader_modules({ cs_shader });
}

/* -------------------------------------------------------------------------- */
