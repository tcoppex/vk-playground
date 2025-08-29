#include "framework/fx/_experimental/post_fx_pipeline.h"
#include "framework/fx/_experimental/compute_fx.h"

/* -------------------------------------------------------------------------- */

void PostFxPipeline::setupDependencies() {
  assert(context_ptr_ != nullptr);
  assert(!effects_.empty());

  context_ptr_->wait_device_idle();

  for (size_t i = 0; i < effects_.size(); ++i) {
    auto& fx = effects_[i];
    auto const& dep = dependencies_[i];

    if (!dep.images.empty()) {
      std::vector<backend::Image> input_images;
      input_images.reserve(dep.images.size());
      for (auto const& [image_fx, index] : dep.images) {
        // LOGD("fx %ld, image %d %s", i, index, image_fx->name().c_str());
        input_images.push_back(image_fx->getImageOutput(index));
      }
      fx->setImageInputs(std::move(input_images));
    }

    if (!dep.buffers.empty()) {
      std::vector<backend::Buffer> input_buffers;
      input_buffers.reserve(dep.buffers.size());
      for (auto const& [buffer_fx, index] : dep.buffers) {
        // LOGD("fx %ld, buffer %d %s", i, index, buffer_fx->name().c_str());
        input_buffers.push_back(buffer_fx->getBufferOutput(index));
      }
      fx->setBufferInputs(std::move(input_buffers));
    }
  }
}

/* -------------------------------------------------------------------------- */
