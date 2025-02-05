#include "framework/scene/resources.h"
#include "framework/utils/utils.h"

extern "C" {
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

// #define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
}

/* -------------------------------------------------------------------------- */

namespace {

std::string GetTextureRefID(cgltf_texture const& texture, cgltf_size const unknown_id = -1) {
  auto const& image = texture.image;
  std::string ref{image->name ? image->name : image->uri ? image->uri
                              : "Texture::Unknown_" + std::to_string(unknown_id)};
  return ref;
}

Geometry::AttributeType GetAttributeType(cgltf_attribute const& attribute) {
  LOG_CHECK(attribute.index == 0u);

  switch (attribute.type) {
    case cgltf_attribute_type_position:
      return Geometry::AttributeType::Position;

    case cgltf_attribute_type_texcoord:
      return Geometry::AttributeType::Texcoord;

    case cgltf_attribute_type_normal:
      return Geometry::AttributeType::Normal;

    case cgltf_attribute_type_tangent:
      return Geometry::AttributeType::Tangent;

    case cgltf_attribute_type_joints:
      return Geometry::AttributeType::Joints;

    case cgltf_attribute_type_weights:
      return Geometry::AttributeType::Weights;

    default:
      LOGE("[GLTF] Unsupported attribute type %s", attribute.name);
      return Geometry::AttributeType::kUnknown;
  }
}

Geometry::AttributeFormat GetAttributeFormat(cgltf_accessor const* accessor) {
  switch (accessor->type) {
    case cgltf_type_vec4:
      switch (accessor->component_type) {
        case cgltf_component_type_r_32f:
          return Geometry::AttributeFormat::RGBA_F32;

        case cgltf_component_type_r_32u:
          return Geometry::AttributeFormat::RGBA_U32;

        case cgltf_component_type_r_16u:
          return Geometry::AttributeFormat::RGBA_U16;

        default:
          LOGE("[GLTF] Unsupported accessor vec4 format %d", accessor->component_type);
          return Geometry::AttributeFormat::kUnknown;
      }

    case cgltf_type_vec3:
      LOG_CHECK(accessor->component_type == cgltf_component_type_r_32f);
      return Geometry::AttributeFormat::RGB_F32;

    case cgltf_type_vec2:
      LOG_CHECK(accessor->component_type == cgltf_component_type_r_32f);
      return Geometry::AttributeFormat::RG_F32;

    default:
      LOGE("[GLTF] Unsupported accessor format");
      return Geometry::AttributeFormat::kUnknown;
  }
}

Geometry::IndexFormat GetIndexFormat(cgltf_accessor const* accessor) {
  if (accessor->type == cgltf_type_scalar) {
    if (accessor->component_type == cgltf_component_type_r_32u) {
      return Geometry::IndexFormat::U32;
    }
    else if (accessor->component_type == cgltf_component_type_r_16u) {
      return Geometry::IndexFormat::U16;
    }
  }
  LOGE("[GLTF] Unsupported index format");
  return Geometry::IndexFormat::kUnknown;
}

Geometry::Topology GetTopology(cgltf_primitive const& primitive) {
  switch (primitive.type) {
    case cgltf_primitive_type_points:
      return Geometry::Topology::PointList;

    case cgltf_primitive_type_triangles:
      return Geometry::Topology::TriangleList;

    case cgltf_primitive_type_triangle_strip:
      return Geometry::Topology::TriangleStrip;

    default:
      return Geometry::Topology::kUnknown;
  }
}

// ----------------------------------------------------------------------------

void ExtractTextures(cgltf_data const* data, scene::host::Resources& R) {
  int const kDefaultNumChannels = 4;
  stbi_set_flip_vertically_on_load(false);

  for (cgltf_size i = 0; i < data->textures_count; ++i) {
    cgltf_texture const& texture = data->textures[i];

    if (auto img = texture.image; img) {
      auto const ref = GetTextureRefID(texture, i);
      auto image = std::make_shared<scene::host::Image>();

      if (auto bufferView = img->buffer_view; bufferView) {
        stbi_uc const* bufferData = reinterpret_cast<stbi_uc const*>(bufferView->buffer->data) + bufferView->offset;
        int32_t const bufferSize = static_cast<int32_t>(bufferView->size);
        image->pixels.reset(stbi_load_from_memory(
          bufferData,
          bufferSize,
          &image->width,
          &image->height,
          &image->channels,
          kDefaultNumChannels
        ));
      } else {
        LOGW("Texture %s unsupported.", ref.c_str());
        continue;
      }

      // Reference into the scene structure.
      if (image->pixels != nullptr) {
        LOGI("[GLTF] Texture %s has been loaded.", ref.c_str());
        R.images[ref] = std::move(image);
      } else {
        LOGW("[GLTF] Texture %s failed to be loaded.", ref.c_str());
      }
    }
  }
}

void ExtractMaterials(std::unordered_map<void const*, std::string> &material_map, cgltf_data const* data, scene::host::Resources& R) {
  for (cgltf_size i = 0; i < data->materials_count; ++i) {
    cgltf_material const& material = data->materials[i];

    auto const materialDataName = material.name ? material.name
                                : std::string("Material::Unknown_" + std::to_string(i));
    material_map[&material] = materialDataName;

    // (should never happens)
    if (auto it = R.materials.find(materialDataName); it != R.materials.end()) {
      LOGW("[GLTF] material \"%s\" already exists.", materialDataName.c_str());
      continue;
    }

    auto materialData = std::make_shared<scene::host::Material>();
    materialData->name = materialDataName;

    // [wip] PBR MetallicRoughness.
    if (material.has_pbr_metallic_roughness) {
      auto const& pbr_mr = material.pbr_metallic_roughness;
      std::copy(
        std::cbegin(pbr_mr.base_color_factor),
        std::cend(pbr_mr.base_color_factor),
        lina::ptr(materialData->baseColor)
      );
      if (const cgltf_texture* tex = pbr_mr.base_color_texture.texture; tex) {
        auto ref = GetTextureRefID(*tex);
        if (auto it = R.images.find(ref); it != R.images.end()) {
          materialData->albedoTexture = it->second;
        }
      }
      if (const cgltf_texture* tex = pbr_mr.metallic_roughness_texture.texture; tex) {
        auto ref = GetTextureRefID(*tex);
        if (auto it = R.images.find(ref); it != R.images.end()) {
          materialData->ormTexture = it->second;
        }
      }
    } else {
      LOGW("[GLTF] Material %s has unsupported material type.", materialDataName.c_str());
      continue;
    }

    // Normal texture.
    if (const cgltf_texture* tex = material.normal_texture.texture; tex) {
      auto ref = GetTextureRefID(*tex);
      if (auto it = R.images.find(ref); it != R.images.end()) {
        materialData->normalTexture = it->second;
      }
    }

    R.materials[materialDataName] = std::move(materialData);
  }
}

void ExtractMeshes(std::unordered_map<void const*, std::string> const& material_map, cgltf_data const* data, scene::host::Resources& R) {
  /**
   * [DevNote]
   * All primitives / submeshes of a gltf Mesh are considered individuals mesh.
   **/

  // Preprocess meshes nodes.
  cgltf_size submeshesCount = 0;
  std::vector<uint32_t> meshNodeIndices;
  for (cgltf_size i = 0; i < data->nodes_count; ++i) {
    cgltf_node const& node = data->nodes[i];
    if (node.mesh) {
      submeshesCount += node.mesh->primitives_count;
      meshNodeIndices.push_back(i);
      if (node.has_mesh_gpu_instancing) {
        LOGW("[GLTF] GPU instancing not supported.");
      }
    }
  }
  R.meshes.reserve(submeshesCount);

  // Parse each mesh nodes (for submeshes & skeleton).
  for (auto i : meshNodeIndices) {
    cgltf_node const& node = data->nodes[i];
    cgltf_mesh const& mesh = *node.mesh;

    // Root transform.
    mat4f world_matrix;
    cgltf_node_transform_world(&node, lina::ptr(world_matrix));

    // Keep track of current mesh's primitives (/ submeshes).
    std::vector<std::shared_ptr<scene::host::Mesh>> submeshes;
    submeshes.reserve(mesh.primitives_count);

    // Primitives.
    for (cgltf_size j = 0; j < mesh.primitives_count; ++j) {
      cgltf_primitive const& primitive = mesh.primitives[j];

      auto meshData = std::make_shared<scene::host::Mesh>();

      // [Check] Has Attributes.
      if (primitive.attributes_count <= 0u) {
        LOGW("[GLTF] A primitive was missing attributes.");
        continue;
      }
      // [Check] Compressed.
      if (primitive.has_draco_mesh_compression) {
        LOGW("[GLTF] Draco mesh compression is not supported.");
        continue;
      }
      // [Check] Non triangles primitives.
      if (primitive.type != cgltf_primitive_type_triangles) {
        LOGW("[GLTF] Non TRIANGLES primitives are not supported.");
        continue;
      }
      // [Check] Morph targets.
      if (primitive.targets_count > 0) {
        LOGW("[GLTF] Morph targets are not supported.");
      }

      /* Retrieve attributes parameters. */
      std::map<cgltf_buffer*, cgltf_buffer*> buffers{};
      uint32_t vertex_count = 0;
      bool isSparse = false;
      for (cgltf_size k = 0; k < primitive.attributes_count; ++k) {
        cgltf_attribute const& attribute = primitive.attributes[k];
        cgltf_accessor const* accessor = attribute.data;
        if (accessor->is_sparse) {
          isSparse = true;
          break;
        }
        auto buffer = accessor->buffer_view->buffer;
        buffers[buffer] = buffer;

        vertex_count = accessor->count;

        if (auto type = GetAttributeType(attribute); type != Geometry::AttributeType::kUnknown) {
          meshData->add_attribute(type, {
            .format = GetAttributeFormat(accessor),
            .offset = static_cast<uint32_t>(accessor->offset),
            .stride = static_cast<uint32_t>(accessor->stride),
          });
        }
      }

      /* Check the attributes are neither sparse nor on different buffer. */
      if (isSparse) {
        LOGW("[GLTF] sparse attributes are not supported.");
        continue;
      }
      if (buffers.size() > 1) {
        LOGW("[GLTF] attributes on multiple buffers are not supported.");
        continue;
      }

      if (cgltf_buffer *buffer = buffers.begin()->first; buffer) {
        meshData->add_vertices_data(reinterpret_cast<uint8_t*>(buffer->data), buffer->size);
        meshData->set_vertex_info(GetTopology(primitive), vertex_count);
      }

      if (primitive.indices) {
        cgltf_accessor const* accessor = primitive.indices;
        cgltf_buffer const* buffer = accessor->buffer_view->buffer;
        if (auto index_format = GetIndexFormat(accessor); index_format != Geometry::IndexFormat::kUnknown) {
          meshData->add_indices_data(index_format, accessor->count, reinterpret_cast<uint8_t*>(buffer->data), buffer->size);
        }
      }

      // [TODO]
      // Batch transform attributes to the mesh root's node world coords.
      // meshData->transform_attribute(..);

      // Assign material when availables.
      if (primitive.material) {
        if (auto it = material_map.find(primitive.material); it != material_map.cend()) {
          meshData->material = R.materials[it->second];
        }
      }

      submeshes.push_back(std::move(meshData));
    }

    // Skeleton.
    if (cgltf_skin const* skin = node.skin; skin) {
      auto const skinName = skin->name ? skin->name
                                       : std::string("Skin::Unknown_" + std::to_string(i));
      auto skel_it = R.skeletons.find(skinName);

      // std::cerr << "* Skin Node name : " << node.name << std::endl;
      // std::cerr << "    -> Skin name : " << skinName << std::endl;

      std::shared_ptr<scene::host::Skeleton> skeleton;

      if (skel_it != R.skeletons.end()) {
        skeleton = skel_it->second;
      } else [[likely]] {
        cgltf_size const jointCount = skin->joints_count;

        skeleton = std::make_shared<scene::host::Skeleton>(jointCount);

        // LUT Map to find parent nodes index.
        std::unordered_map<cgltf_node const*, int32_t> joint_indices(jointCount);
        for (cgltf_size jointIndex = 0; jointIndex < jointCount; ++jointIndex) {
          cgltf_node const* joint = skin->joints[jointIndex];
          joint_indices[joint] = static_cast<int32_t>(jointIndex);
        }

        // Fill skeleton basic data (no matrices).
        for (cgltf_size jointIndex = 0; jointIndex < jointCount; ++jointIndex) {
          cgltf_node const* joint = skin->joints[jointIndex];

          std::string const jointName = joint->name ? joint->name
                                                    : std::string("Joint::Unknown_" + std::to_string(jointIndex));
          auto const& it = joint_indices.find(joint->parent);
          int32_t const parentIndex = (it != joint_indices.end()) ? it->second : -1;

          skeleton->names[jointIndex] = jointName;
          skeleton->parents[jointIndex] = parentIndex;
          skeleton->index_map[jointName] = static_cast<int32_t>(jointIndex);
        }

        // Calculates global inverse bind matrices.
        {
          // Retrieve local matrices.
          auto &matrices = skeleton->inverse_bind_matrices;
          auto const bufferSize = (sizeof(matrices[0]) / sizeof(*lina::ptr(matrices[0]))) * matrices.size();
          LOG_CHECK(bufferSize == 16 * jointCount);
          cgltf_accessor_unpack_floats(skin->inverse_bind_matrices, lina::ptr(matrices[0]), bufferSize);

          // Transform them to world space.
          auto const inverse_world_matrix{linalg::inverse(world_matrix)};
          skeleton->transformInverseBindMatrices(inverse_world_matrix);
        }

        R.skeletons[skinName] = skeleton;
      }

      // Give the skeleton reference to all submeshes.
      for (auto &meshData : submeshes) {
        meshData->skeleton = skeleton;
      }
    }

    R.meshes.insert(
      R.meshes.begin(),
      std::make_move_iterator(submeshes.begin()),
      std::make_move_iterator(submeshes.end())
    );
    submeshes.clear();
  }
}

void ExtractAnimations(cgltf_data const* data, scene::host::Resources& R) {
  if (!data || (data->animations_count == 0u)) {
    return;
  } else if (R.skeletons.empty()) {
    LOGE("[GLTF] animations without skeleton are not supported.");
    return;
  }

  // --------

  // LUT to find skeleton via animations name.
  std::unordered_map<std::string, std::string> animationName_to_SkeletonName;
  for (cgltf_size i = 0; i < data->skins_count; ++i) {
    cgltf_skin* skin = &data->skins[i];
    auto const skeletonName = skin->name ? skin->name
                            : std::string("Skin::Unknown_" + std::to_string(i));
    for (size_t j = 0; j < skin->joints_count; ++j) {
      cgltf_node* joint_node = skin->joints[j];
      for (size_t k = 0; k < data->animations_count; ++k) {
        cgltf_animation const& animation = data->animations[k];
        auto const clipName = animation.name ? animation.name
                            : std::string("Animation::Unknown_" + std::to_string(k));
        for (size_t l = 0; l < animation.channels_count; ++l) {
          cgltf_animation_channel* channel = &animation.channels[l];
          if (channel->target_node == joint_node) {
            animationName_to_SkeletonName[clipName] = skeletonName;
          }
        }
      }
    }
  }

  // --------

  std::vector<float> inputs, outputs;
  R.animations.reserve(data->animations_count);

  // Retrieve Animations.
  for (cgltf_size i = 0; i < data->animations_count; ++i) {
    cgltf_animation const& animation = data->animations[i];
    std::string const clipName = animation.name ? animation.name
                               : std::string("Animation::Unknown_" + std::to_string(i));
    LOG_CHECK(animation.samplers_count == animation.channels_count);

    // Find animation's skeleton.
    std::shared_ptr<scene::host::Skeleton> skeleton{nullptr};
    if (auto skelname_it = animationName_to_SkeletonName.find(clipName); skelname_it != animationName_to_SkeletonName.end()) {
      auto skelname = skelname_it->second;
      if (auto skel_it = R.skeletons.find(skelname); skel_it != R.skeletons.end()) {
        skeleton = skel_it->second;
      }
    }
    if (nullptr == skeleton) {
      if (auto it = R.skeletons.begin(); it != R.skeletons.end()) {
        LOGW("[GLTF] used the first available skeleton found.");
        skeleton = it->second;
      } else {
        LOGE("[GLTF] cannot find animation's skeleton.");
        continue;
      }
    }

    // Preprocess channels to detect max sample count, in case the exporter compressed them.
    bool bResamplingNeededCheck = false;
    cgltf_size sampleCount{animation.channels[0].sampler->output->count};
    for (cgltf_size channel_id = 0; channel_id < animation.channels_count; ++channel_id) {
      auto const& channel = animation.channels[channel_id];
      auto const* sampler = channel.sampler;
      auto const* input = sampler->input;

      // We only support scalar sampling.
      LOG_CHECK(input->type == cgltf_type_scalar);

      // Check if the exporter have optimized animations by removing duplicate frames.
      bResamplingNeededCheck = (sampleCount > 1) && (sampleCount != sampler->output->count);

      inputs.resize(input->count);
      cgltf_accessor_unpack_floats(input, inputs.data(), inputs.size());
      sampleCount = std::max(sampleCount, sampler->output->count);
    }
    if (bResamplingNeededCheck) {
      LOGW("[GLTF] Some animations will be resampled.");
    }

    // Create the animation clip.
    auto clip = std::make_shared<scene::host::AnimationClip>();
    float const clipDuration = inputs.back();
    clip->setup(clipName, sampleCount, clipDuration, skeleton->jointCount());

    // Parse each channels (ie. transform per joint).
    cgltf_accessor const* last_input_accessor{nullptr};
    for (cgltf_size channel_id = 0; channel_id < animation.channels_count; ++channel_id) {
      auto const& channel{ animation.channels[channel_id] };
      auto const* sampler{ channel.sampler };
      auto const* input{ sampler->input };
      auto const* output{ sampler->output };

      // Find target joint's index.
      int32_t jointIndex = -1;
      if (auto jointName = channel.target_node->name; jointName != nullptr) {
        if (auto it = skeleton->index_map.find(jointName); it != skeleton->index_map.end()) {
          jointIndex = it->second;
        }
      }
      if (jointIndex < 0) {
        LOGE("[GLTF] cannot find joint index in LUT.");
        break;
      }

      // Inputs (eg. time).
      if (input != last_input_accessor) {
        inputs.resize(input->count);
        cgltf_accessor_unpack_floats(input, inputs.data(), inputs.size());
        last_input_accessor = input;
      }

      // Outputs (eg. rotation).
      cgltf_size const output_ncomp{cgltf_num_components(output->type)};
      outputs.resize(output_ncomp * output->count);
      cgltf_accessor_unpack_floats(output, outputs.data(), outputs.size());

      // Check if we need to resample outputs.
      bool const bNeedResampling{(sampleCount > 1) && (sampleCount != output->count)};
      cgltf_size frameStart, frameEnd;
      float lerpFactor;

      for (cgltf_size sid = 0; sid < sampleCount; ++sid) {
        auto &pose = clip->poses.at(sid);
        auto &joint = pose.joints.at(jointIndex);

        // Calculate resampling parameters when needed.
        if (bNeedResampling) [[unlikely]] {
          // [optimization]
          // Most of the time there would be only two frames, the first and the last,
          // which would be identical and would not require any interpolation
          // (just copying them to the whole range).

          cgltf_size const src_sampleCount{ output->count };
          float const dt = sid / static_cast<float>(sampleCount - 1);

          frameStart = static_cast<int>(floor(dt * src_sampleCount));
          frameEnd = static_cast<int>(ceil(dt * src_sampleCount));
          frameStart = std::min(frameStart, src_sampleCount - 1);
          frameEnd = std::min(frameEnd, src_sampleCount - 1);

          float const start_time = inputs.at(frameStart);
          float const diff_time = inputs.at(frameEnd) - start_time;
          float const dst_time = dt * clipDuration;
          lerpFactor = (diff_time > 0.0f) ? (dst_time - start_time) / diff_time : 0.0f;
        }

        switch(channel.target_path) {
          case cgltf_animation_path_type_translation:
          {
            vec3f const* v = reinterpret_cast<vec3f const*>(outputs.data());
            joint.translation = bNeedResampling ? linalg::lerp(v[frameStart], v[frameEnd], lerpFactor)
                                                : v[sid]
                                                ;
          }
          break;

          case cgltf_animation_path_type_rotation:
          {
            vec4f const* q = reinterpret_cast<vec4f const*>(outputs.data());
            joint.rotation = bNeedResampling ? linalg::qnlerp(q[frameStart], q[frameEnd], lerpFactor)
                                             : q[sid]
                                             ;
          }
          break;

          case cgltf_animation_path_type_scale:
          {
            vec3f const* v = reinterpret_cast<vec3f const*>(outputs.data());
            vec3f s = bNeedResampling ? linalg::lerp(v[frameStart], v[frameEnd], lerpFactor)
                                      : v[sid]
                                      ;

            float const kTolerance = 1.0e-5f;
            if (lina::almost_equal(s[0], s[1], kTolerance)
             && lina::almost_equal(s[0], s[2], kTolerance)
             && lina::almost_equal(s[1], s[2], kTolerance))
            {
              joint.scale = s[0];
            }
            else
            {
              LOGW("[GLTF] Non-uniform scale not supported for skinning.");
            }
          }
          break;

          case cgltf_animation_path_type_weights:
          default:
            LOGW("[GLTF] Unsupported animation type.");
          break;
        }
      }
    }

    skeleton->clips.push_back(clip);

    R.animations[clipName] = std::move(clip);
  }
}

}  // namespace ""

/* -------------------------------------------------------------------------- */

namespace scene {
namespace host {

bool Resources::loadFromFile(std::string_view const& filename) {
  utils::FileReader file;

  if (!file.read(filename)) {
    return false;
  }

  cgltf_options options{};
  cgltf_result result{};
  cgltf_data* data{};

  result = cgltf_parse(&options, file.buffer.data(), file.buffer.size(), &data);
  if (result != cgltf_result_success) {
    LOGE("GLTF: failed to parse file \"%s\" %d.\n", filename.data(), result);
    return false;
  }

  result = cgltf_load_buffers(&options, data, filename.data());
  if (result != cgltf_result_success) {
    LOGE( "GLTF: failed to load buffers in \"%s\".\n", filename.data());
    cgltf_free(data);
    data = nullptr;
    return false;
  }

  std::unordered_map<void const*, std::string> material_map;

  ExtractTextures(data, *this);
  ExtractMaterials(material_map, data, *this);
  ExtractMeshes(material_map, data, *this);
  ExtractAnimations(data, *this);

  std::cout << "Image count : " << images.size() << std::endl;
  std::cout << "Material count : " << materials.size() << std::endl;
  std::cout << "Skeleton count : " << skeletons.size() << std::endl;
  std::cout << "Animation count : " << animations.size() << std::endl;

  cgltf_free(data);

  return true;
}

}  // namespace host
}  // namespace scene

/* -------------------------------------------------------------------------- */
