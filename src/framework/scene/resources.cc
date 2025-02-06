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

std::string GetTextureRefID(cgltf_texture const& texture, std::string_view alt) {
  auto const& image = texture.image;
  std::string ref{image->name ? image->name : image->uri ? image->uri
                              : std::string(alt)}
                              ;
  return ref;
}

Geometry::AttributeType GetAttributeType(cgltf_attribute const& attribute) {
  if (attribute.index != 0u) {
    LOGE("[GLTF] Unsupported multiple attribute of same type %s", attribute.name);
    return Geometry::AttributeType::kUnknown;
  }

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

void ExtractTextures(std::string const& basename, std::unordered_map<void const*, std::string> const& texture_map, cgltf_data const* data, scene::host::Resources& R) {
  int const kDefaultNumChannels = 4;
  stbi_set_flip_vertically_on_load(false);

  for (cgltf_size i = 0; i < data->textures_count; ++i) {
    cgltf_texture const& texture = data->textures[i];

    if (auto img = texture.image; img) {
      std::string ref;

      if (auto it = texture_map.find(&texture); it != texture_map.cend()) {
        ref = it->second;
      } else {
        ref = GetTextureRefID(texture, std::string(basename) + "::Texture_" + std::to_string(i));
      }

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

void ExtractMaterials(
  std::string const& basename,
  std::unordered_map<void const*, std::string> &texture_map,
  std::unordered_map<void const*, std::string> &material_map,
  cgltf_data const* data,
  scene::host::Resources& R
)
{
  auto textureRef = [&texture_map](cgltf_texture const& texture, std::string_view suffix) {
    auto ref = GetTextureRefID(texture, suffix);
    texture_map[&texture] = ref;
    return ref;
  };

  for (cgltf_size i = 0; i < data->materials_count; ++i) {
    cgltf_material const& mat = data->materials[i];

    auto const material_name = mat.name ? mat.name
                             : basename + std::string("::Material_" + std::to_string(i));
    material_map[&mat] = material_name;

    LOG_CHECK(R.materials.find(material_name) == R.materials.end());

    auto material = std::make_shared<scene::host::Material>();
    material->name = material_name;

    std::string const texture_prefix = basename + "::Texture_";

    // [wip] PBR MetallicRoughness.
    if (mat.has_pbr_metallic_roughness) {
      auto const& pbr_mr = mat.pbr_metallic_roughness;
      std::copy(
        std::cbegin(pbr_mr.base_color_factor),
        std::cend(pbr_mr.base_color_factor),
        lina::ptr(material->baseColor)
      );
      if (const cgltf_texture* tex = pbr_mr.base_color_texture.texture; tex) {
        auto ref = textureRef(*tex, texture_prefix + "Albedo");
        if (auto it = R.images.find(ref); it != R.images.end()) {
          material->albedoTexture = it->second;
        }
      }
      if (const cgltf_texture* tex = pbr_mr.metallic_roughness_texture.texture; tex) {
        auto ref = textureRef(*tex, texture_prefix + "MetallicRough");
        if (auto it = R.images.find(ref); it != R.images.end()) {
          material->ormTexture = it->second;
        }
      }
    } else {
      LOGW("[GLTF] Material %s has unsupported material type.", material_name.c_str());
      continue;
    }

    // Normal texture.
    if (const cgltf_texture* tex = mat.normal_texture.texture; tex) {
      auto ref = textureRef(*tex, texture_prefix + "Normal");
      if (auto it = R.images.find(ref); it != R.images.end()) {
        material->normalTexture = it->second;
      }
    }

    R.materials[material_name] = std::move(material);
  }
}

void ExtractMeshes(std::string const& basename, std::unordered_map<void const*, std::string> const& material_map, cgltf_data const* data, scene::host::Resources& R) {
  // Preprocess meshes nodes.
  std::vector<uint32_t> meshNodeIndices;
  for (cgltf_size i = 0; i < data->nodes_count; ++i) {
    cgltf_node const& node = data->nodes[i];
    if (node.mesh) {
      // mesh_count += node.mesh->primitives_count;
      meshNodeIndices.push_back(i);
      if (node.has_mesh_gpu_instancing) {
        LOGW("[GLTF] GPU instancing not supported.");
      }
    }
  }
  R.meshes.reserve(meshNodeIndices.size());

  // Parse each mesh nodes (for primitives & skeleton).
  for (auto i : meshNodeIndices) {
    cgltf_node const& node = data->nodes[i];
    // cgltf_mesh const& mesh = *(node.mesh);

    auto mesh = std::make_shared<scene::host::Mesh>();
    mesh->primitives.reserve(node.mesh->primitives_count);

    // Root transform.
    cgltf_node_transform_world(&node, lina::ptr(mesh->world_matrix));

    // Primitives.
    for (cgltf_size j = 0; j < node.mesh->primitives_count; ++j) {
      cgltf_primitive const& prim = node.mesh->primitives[j];

      LOGI("-------\n << PRIMITIVE %03u", j);

      // [Check] Has Attributes.
      if (prim.attributes_count <= 0u) {
        LOGW("[GLTF] A primitive was missing attributes.");
        continue;
      }
      // [Check] Compressed.
      if (prim.has_draco_mesh_compression) {
        LOGW("[GLTF] Draco mesh compression is not supported.");
        continue;
      }
      // [Check] Non triangles primitives.
      // if (prim.type != cgltf_primitive_type_triangles) {
      //   LOGW("[GLTF] Non TRIANGLES primitives are not supported.");
      //   continue;
      // }
      // [Check] Morph targets.
      if (prim.targets_count > 0) {
        LOGW("[GLTF] Morph targets are not supported.");
      }

      auto primitive = std::make_shared<scene::host::Primitive>();

      //--------------------------------------------------

      // (Primitive buffer are copied "as is" but it might be better to force
      //  them to our own internal layout)

      /* Retrieve attributes parameters. */
      std::map<cgltf_buffer*, cgltf_buffer*> buffers{};
      std::map<cgltf_buffer_view*, cgltf_buffer_view*> bufferviews{};
      std::map<cgltf_accessor const*, cgltf_accessor const*> accessors{};
      std::map<cgltf_accessor const*, uint32_t> accessor_buffer_offsets{};

      uint32_t vertex_count = 0;
      bool isSparse = false;
      for (cgltf_size k = 0; k < prim.attributes_count; ++k) {
        cgltf_attribute const& attribute = prim.attributes[k];
        cgltf_accessor const* accessor = attribute.data;
        if (accessor->is_sparse) {
          isSparse = true;
          break;
        }
        accessors[accessor] = accessor;

        auto buffer_view = accessor->buffer_view;
        bufferviews[buffer_view] = bufferviews[buffer_view];

        auto buffer = buffer_view->buffer;
        buffers[buffer] = buffer;

        vertex_count = accessor->count;

        if (auto type = GetAttributeType(attribute); type != Geometry::AttributeType::kUnknown) {
          LOGD("%s", attribute.name);
          LOGD("offset %lu %lu", buffer_view->offset, accessor->offset);
          LOGD("stride %lu %lu", buffer_view->stride, accessor->stride);
          LOGD("size %lu \t count %lu", buffer_view->size, accessor->count);
          LOGD(" ");

          uint32_t bufferOffset;
          uint32_t accessorOffset(0);
          if ((accessor->offset == 0) || (accessor->offset >= 2048)) {
            bufferOffset = primitive->add_vertices_data(
              reinterpret_cast<uint8_t*>(buffer_view->buffer->data) + buffer_view->offset + accessor->offset,
              buffer_view->size
            );
            accessor_buffer_offsets[accessor] = bufferOffset;
            LOGW("[GLTF] added %u vertices, from an offset of %lu bytes, for a buffer of %lu bytes.", vertex_count, buffer_view->offset, buffer_view->size);
          } else {
            bufferOffset = accessor_buffer_offsets[accessor];
            accessorOffset = static_cast<uint32_t>(accessor->offset);
          }
          LOGD(">> %lu %u\n", accessor->offset, bufferOffset);

          primitive->add_attribute(type, {
            .format = GetAttributeFormat(accessor),
            .offset = accessorOffset,
            .stride = static_cast<uint32_t>(accessor->stride),
            .bufferOffset = bufferOffset, //
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

      LOGD("accessors.size : %lu", accessors.size());
      LOGD("bufferviews.size : %lu", bufferviews.size());

      // Issue when hte model use multiple buffers offset
      // for (auto key : bufferviews)
      // {
      //   auto buffer_view = key.first;
      //   primitive->add_vertices_data(reinterpret_cast<uint8_t*>(buffer_view->buffer->data) + buffer_view->offset, buffer_view->size); // XXXX
      //   LOGW("[GLTF] added %u vertices, from an offset of %lu bytes, for a buffer of %lu bytes.", vertex_count, buffer_view->offset, buffer_view->size);
      // }
      primitive->set_vertex_info(GetTopology(prim), vertex_count);
      //--------------------------------------------------

      if (prim.indices) {
        cgltf_accessor const* accessor = prim.indices;
        cgltf_buffer_view const* buffer_view = accessor->buffer_view;
        cgltf_buffer const* buffer = buffer_view->buffer;
        if (auto index_format = GetIndexFormat(accessor); index_format != Geometry::IndexFormat::kUnknown) {
          primitive->add_indices_data(index_format, accessor->count, reinterpret_cast<uint8_t*>(buffer->data) + buffer_view->offset, buffer_view->size);
          LOGD("[GLTF] added %lu indices, from an offset of %lu bytes, for a buffer of %lu bytes.", accessor->count, buffer_view->offset, buffer_view->size);
        }
      } else {
        LOGD("[GLFW] no indices.");
      }
      LOGD(" ");

      // Assign material when availables.
      if (prim.material) {
        if (auto it = material_map.find(prim.material); it != material_map.cend()) {
          primitive->material = R.materials[it->second];
        }
      }

      mesh->primitives.push_back(std::move(primitive));
    }

    // Skeleton.
    if (cgltf_skin const* skin = node.skin; skin) {
      auto const skinName = skin->name ? skin->name
                                       : std::string(basename + "::Skeleton_" + std::to_string(i));
      auto skel_it = R.skeletons.find(skinName);

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
                                                    : std::string(basename + "::Joint_" + std::to_string(jointIndex));
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
          auto const inverse_world_matrix{linalg::inverse(mesh->world_matrix)};
          skeleton->transformInverseBindMatrices(inverse_world_matrix);
        }

        R.skeletons[skinName] = skeleton;
      }

      mesh->skeleton = skeleton;
    }

    R.meshes.push_back(mesh);
    // R.meshes.insert(
    //   R.meshes.begin(),
    //   std::make_move_iterator(primitives.begin()),
    //   std::make_move_iterator(primitives.end())
    // );
    // primitives.clear();
  }
}

void ExtractAnimations(std::string const& basename, cgltf_data const* data, scene::host::Resources& R) {
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
                            : basename + std::string("::Skeleton_" + std::to_string(i));
    for (size_t j = 0; j < skin->joints_count; ++j) {
      cgltf_node* joint_node = skin->joints[j];
      for (size_t k = 0; k < data->animations_count; ++k) {
        cgltf_animation const& animation = data->animations[k];
        auto const clipName = animation.name ? animation.name
                            : basename + std::string("::Animation_" + std::to_string(k));
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
                               : basename + std::string("##Animation_" + std::to_string(i));
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

  auto basename = utils::ExtractBasename(filename);

  std::unordered_map<void const*, std::string> texture_map;
  std::unordered_map<void const*, std::string> material_map;
  ExtractMaterials(basename, texture_map, material_map, data, *this);
  ExtractTextures(basename, texture_map, data, *this);
  ExtractMeshes(basename, material_map, data, *this);
  ExtractAnimations(basename, data, *this);

  std::cout << "Image count : " << images.size() << std::endl;
  std::cout << "Material count : " << materials.size() << std::endl;
  std::cout << "Skeleton count : " << skeletons.size() << std::endl;
  std::cout << "Animation count : " << animations.size() << std::endl;
  std::cout << "Mesh count : " << meshes.size() << std::endl;

  cgltf_free(data);

  return true;
}

}  // namespace host
}  // namespace scene

/* -------------------------------------------------------------------------- */
