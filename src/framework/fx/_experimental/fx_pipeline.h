#ifndef HELLO_VK_FRAMEWORK_FX_POSTPROCESS_PIPELINE_H_
#define HELLO_VK_FRAMEWORK_FX_POSTPROCESS_PIPELINE_H_

#include "framework/common.h"
#include "framework/fx/_experimental/generic_fx.h"
#include "framework/fx/_experimental/compute_fx.h"

/* -------------------------------------------------------------------------- */

struct FxDependencies {
  template<typename T>
  requires DerivedFrom<T, FxInterface>
  struct FxDep {
    std::shared_ptr<T> fx{};
    uint32_t index{};
  };
  std::vector<FxDep<FxInterface>> images{};
  std::vector<FxDep<FxInterface>> buffers{};
};

// ----------------------------------------------------------------------------

class FxPipeline : public FxInterface {
 public:
  FxPipeline() = default;

  virtual ~FxPipeline() {}

  virtual void reset() {
    effects_.clear();
    dependencies_.clear();
  }

  template<typename T>
  requires DerivedFrom<T, FxInterface>
  std::shared_ptr<T> add(FxDependencies const& dependencies = {}) {
    auto fx = std::make_shared<T>();
    effects_.push_back(fx);
    dependencies_.push_back(dependencies);
    return fx;
  }

  template<typename T>
  requires DerivedFrom<T, FxInterface>
  std::shared_ptr<T> get(uint32_t index) {
    return std::static_pointer_cast<T>(effects_.at(index));
  }

  virtual void setupDependencies();

 public:
  void init(Context const& context, Renderer const& renderer) override {
    assert(!effects_.empty());
    FxInterface::init(context, renderer);
    for (auto fx : effects_) {
      fx->init(context, renderer);
    }
  }

  void setup(VkExtent2D const dimension) override {
    for (auto fx : effects_) {
      fx->setup(dimension);
    }
    setupDependencies();
  }

  void resize(VkExtent2D const dimension) override {
    for (auto fx : effects_) {
      fx->resize(dimension);
    }
  }

  void release() override {
    for (auto it = effects_.rbegin(); it != effects_.rend(); ++it) {
     (*it)->release();
    }
    reset();
  }

  void execute(CommandEncoder& cmd) override {
    for (auto fx : effects_) {
      fx->execute(cmd);
    }
  }

  void setImageInputs(std::vector<backend::Image> const& inputs) override {
    assert(!effects_.empty());
    effects_.front()->setImageInputs(inputs);
  }

  void setBufferInputs(std::vector<backend::Buffer> const& inputs) override {
    assert(!effects_.empty());
    effects_.front()->setBufferInputs(inputs);
  }

  backend::Image const& getImageOutput(uint32_t index = 0u) const override {
    assert(!effects_.empty());
    return effects_.back()->getImageOutput(index);
  }

  std::vector<backend::Image> const& getImageOutputs() const override {
    assert(!effects_.empty());
    return effects_.back()->getImageOutputs();
  }

  backend::Buffer const& getBufferOutput(uint32_t index) const override {
    return effects_.back()->getBufferOutput(index);
  }

  std::vector<backend::Buffer> const& getBufferOutputs() const override {
    return effects_.back()->getBufferOutputs();
  }

  void setupUI() override {
    for (auto fx : effects_) {
      fx->setupUI();
    }
  }

  std::string name() const override {
    return "FxPipeline::NoName";
  }

 protected:
  static
  FxDependencies GetOutputDependencies(std::shared_ptr<FxPipeline> fx) {
    FxDependencies dep = fx->getDefaultOutputDependencies();
    for (auto& img : dep.images) {
      img.fx = fx;
    }
    for (auto& buf : dep.buffers) {
      buf.fx = fx;
    }
    return dep;
  }

  // Format of the pipeline output, usually just an image.
  virtual FxDependencies getDefaultOutputDependencies() const {
    return { .images = { {.index = 0u} } };
  }

 protected:
  std::vector<std::shared_ptr<FxInterface>> effects_{};
  std::vector<FxDependencies> dependencies_{};
};

// ----------------------------------------------------------------------------

/**
 * A Templated fx pipeline where the provided parameter is the entry point
 * effect, automatically added to the pipeline.
 */
template<typename E>
class TFxPipeline : public FxPipeline {
 public:
  TFxPipeline()
    : FxPipeline()
  {
    reset();
  }

  void reset() override {
    FxPipeline::reset();
    add<E>();
    setEntryDependencies(entry_dependencies_);
  }

  virtual std::shared_ptr<E> getEntryFx() {
    return get<E>(0u);
  }

  void setEntryDependencies(FxDependencies const& dependencies) {
    dependencies_[0u] = dependencies;
    entry_dependencies_ = dependencies;
  }

  template<typename T>
  requires DerivedFrom<T, FxInterface>
  std::shared_ptr<T> add(FxDependencies const& dependencies = {}) {
    auto fx = FxPipeline::add<T>(dependencies);
    if constexpr (DerivedFrom<T, FxPipeline>) {
      fx->setEntryDependencies(dependencies);
    }
    return fx;
  }

  template<typename T>
  requires DerivedFrom<T, FxInterface>
  std::shared_ptr<T> add(std::shared_ptr<FxPipeline> pipeline) {
    return add<T>(GetOutputDependencies(pipeline));
  }

 public:
  FxDependencies entry_dependencies_{};
};

// ----------------------------------------------------------------------------

// Blank Fx used to pass data to a specialized pipeline
class PassDataNoFx final : public FxInterface {
 public:
  void setup(VkExtent2D const dimension) final {}
  void resize(VkExtent2D const dimension) final {}
  void execute(CommandEncoder& cmd) final {}
  void setupUI() final {}

  void release() final {
    images_.clear();
    buffers_.clear();
  }

  void setImageInputs(std::vector<backend::Image> const& inputs) final {
    images_ = inputs;
  }

  void setBufferInputs(std::vector<backend::Buffer> const& inputs) final {
    buffers_ = inputs;
  }

  backend::Image const& getImageOutput(uint32_t index) const final {
    return images_.at(index);
  }

  std::vector<backend::Image> const& getImageOutputs() const final {
    return images_;
  }

  backend::Buffer const& getBufferOutput(uint32_t index) const final {
    return buffers_.at(index);
  }

  std::vector<backend::Buffer> const& getBufferOutputs() const final {
    return buffers_;
  }

  std::string name() const override {
    return "PassDataNoFX";
  }

 private:
  std::vector<backend::Image> images_{};
  std::vector<backend::Buffer> buffers_{};
};

/* -------------------------------------------------------------------------- */

#endif // HELLO_VK_FRAMEWORK_FX_POSTPROCESS_PIPELINE_H_