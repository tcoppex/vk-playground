#ifndef HELLOVK_FRAMEWORK_FX_MATERIAL_FX_H_
#define HELLOVK_FRAMEWORK_FX_MATERIAL_FX_H_

#include "framework/fx/_experimental/fragment_fx.h"

#include "framework/scene/material.h" //

/* -------------------------------------------------------------------------- */

///
/// [wip]
/// Specialized FragmentFx with custom material.
///
/// Ideally should derive from GenericFx rather than FragmentFx, but this would
/// requires to rethink the post processing pipeline. Maybe in the future.
///

// ----------------------------------------------------------------------------

class MaterialFx : public FragmentFx {
 public:
  using CreateMaterialTuple = std::tuple<::scene::MaterialRef, void*>;

 public:
  void setup(VkExtent2D const dimension = {}) override {
    FragmentFx::setup(dimension);
  }

  // (we need those as public, so we expose them with new names..)
  // ------------------------------------
  void prepareDrawState(RenderPassEncoder const& pass) {
    setupRenderPass(pass);
  }

  void pushConstant(GenericCommandEncoder const& cmd) {
    updatePushConstant(cmd);
  }
  // ------------------------------------

  virtual CreateMaterialTuple createMaterial() = 0;

 public:
  virtual void setProjectionMatrix(mat4 const& projection_matrix) = 0;
  virtual void setViewMatrix(mat4 const& view_matrix) = 0;
  virtual void setCameraPosition(vec3 const& camera_position) = 0;

  virtual void updateCameraData(Camera const& camera) {
    setProjectionMatrix(camera.proj());
    setViewMatrix(camera.view());
    setCameraPosition(camera.position());
  }

  // ------------------------------------
  // (application-wide)
  virtual uint32_t getDescriptorSetTextureAtlasBinding() const = 0;

  void updateDescriptorSetTextureAtlasEntry(DescriptorSetWriteEntry const& entry) const {
    context_ptr_->update_descriptor_set(descriptor_set_, { entry });
  }

  // (uniform buffer, probably to be move to application)
  virtual void pushUniforms() = 0;

  // (per model instance push constants)
  virtual void setWorldMatrix(mat4 const& world_matrix) = 0; //
  virtual void setMaterialIndex(uint32_t material_index) = 0;
  virtual void setInstanceIndex(uint32_t instance_index) = 0;
  // ------------------------------------

  virtual void setMaterial(::scene::MaterialRef const& material_ref) = 0; //

 protected:
  // (we might probably discard them altogether)
  // ------------------------------------
  void draw(RenderPassEncoder const& pass) override {} //
  void execute(CommandEncoder& cmd) override {} //
  // ------------------------------------

 // protected:
 //  uint32_t material_fx_index_{ kInvalidIndexU32 }; //
};

// ----------------------------------------------------------------------------

template<typename TMaterial>
class TMaterialFx : public MaterialFx {
 public:
  using Material = TMaterial;

  static
  std::type_index MaterialTypeIndex() {
    return std::type_index(typeid(TMaterial));
  }

 protected:
  CreateMaterialTuple createMaterial() override {
    auto& mat = materials_.emplace_back();
    scene::MaterialRef ref = {
      .material_type_index = MaterialTypeIndex(),
      .material_index = static_cast<uint32_t>(materials_.size() - 1u),
    };
    return { ref, &mat };
  }

  TMaterial const& material(uint32_t index) const {
    return materials_[index];
  }

 protected:
  std::vector<TMaterial> materials_{};
};

/* -------------------------------------------------------------------------- */

#endif // HELLOVK_FRAMEWORK_FX_MATERIAL_FX_H_
