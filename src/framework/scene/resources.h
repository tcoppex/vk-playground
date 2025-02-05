#ifndef HELLO_VK_FRAMEWORK_SCENE_RESOURCES_H
#define HELLO_VK_FRAMEWORK_SCENE_RESOURCES_H

#include <string_view>
#include <unordered_map>

#include "stb/stb_image.h"

#include "framework/common.h"
#include "framework/utils/geometry.h"

namespace scene {
namespace host {

/* -------------------------------------------------------------------------- */

struct Image {
  int width{};
  int height{};
  int channels{};

  std::unique_ptr<uint8_t, decltype(&stbi_image_free)> pixels{nullptr, stbi_image_free}; //

  Image() = default;

  void clear() {
    pixels.reset();
  }
};

struct Material {
  std::string name{};
  vec4f baseColor{vec4f(1.0f)};
  std::shared_ptr<Image> albedoTexture{};
  std::shared_ptr<Image> ormTexture{};
  std::shared_ptr<Image> normalTexture{};

  Material() = default;
};

// ----------------------------------------------------------------------------

template<typename T>
using JointBuffer_t = std::vector<T>;

struct Pose {
  struct Transform {
    vec4f rotation = lina::qidentity<float>();
    vec3f translation = vec3f(0.0f);
    float scale = 1.0f;
  };

  JointBuffer_t<Transform> joints{};
};

struct AnimationClip {
  std::string name{};
  float duration{};
  float framerate{};
  std::vector<Pose> poses{};

  AnimationClip() = default;

  void setup(std::string_view const& clip_name, size_t const sampleCount, float const clip_duration, size_t const jointCount) {
    name = std::string(clip_name);
    duration = clip_duration;
    framerate = static_cast<float>(sampleCount) / linalg::max(duration, lina::kTrueEpsilon);

    poses.resize(sampleCount);
    for (auto& pose : poses) {
      pose.joints.resize(jointCount);
    }
  }
};

struct Skeleton {
  JointBuffer_t<std::string> names{};
  JointBuffer_t<int32_t> parents{};
  JointBuffer_t<mat4f> inverse_bind_matrices{};
  JointBuffer_t<mat4f> global_bind_matrices{};

  std::unordered_map<std::string, int32_t> index_map{};
  std::vector<std::shared_ptr<AnimationClip>> clips{};

  Skeleton() = default;

  Skeleton(size_t const jointCount) {
    resize(jointCount);
  }

  size_t jointCount() const {
    return global_bind_matrices.size();
  }

  void resize(size_t const jointCount) {
    names.resize(jointCount);
    parents.resize(jointCount);
    inverse_bind_matrices.resize(jointCount);
    global_bind_matrices.resize(jointCount);
    index_map.clear();
  }

  void transformInverseBindMatrices(mat4f const& inv_world) {
    for (auto &inverse_bind_matrix : inverse_bind_matrices) {
      inverse_bind_matrix = linalg::mul(inverse_bind_matrix, inv_world);
    }
  }
};

// ----------------------------------------------------------------------------

struct Mesh : Geometry {
  std::shared_ptr<Material> material;
  std::shared_ptr<Skeleton> skeleton;
};

// ----------------------------------------------------------------------------

struct Resources {
  template<typename T> using ResourceMap = std::unordered_map<std::string, std::shared_ptr<T>>;

  ResourceMap<Image> images{};
  ResourceMap<Material> materials{};
  ResourceMap<AnimationClip> animations{};
  ResourceMap<Skeleton> skeletons{};

  std::vector<std::shared_ptr<Mesh>> meshes; //

  Resources() = default;

  Resources(std::string_view const& filename) {
    loadFromFile(filename);
  }

  bool loadFromFile(std::string_view const& filename);
};

/* -------------------------------------------------------------------------- */

}  // namespace host
}  // namespace scene

#endif // HELLO_VK_FRAMEWORK_SCENE_RESOURCES_H
