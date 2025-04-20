#include "framework/scene/resources.h"

extern "C" {
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
}

#include "framework/utils/utils.h"
#include "framework/backend/context.h"

/* -------------------------------------------------------------------------- */

/* ---- Helpers ---- */

namespace {

using PointerToStringMap_t = std::unordered_map<void const*, std::string>;

//-----------------------------------------------------------------------------

//
// Internal interleaved vertex structure used when bRestructureAttribs is set to true.
//
struct VertexInternal_t {
  vec3 position;
  vec3 normal;
  vec2 texcoord;

  static
  Geometry::AttributeInfoMap GetAttributeInfoMap() {
    return {
      {
        Geometry::AttributeType::Position,
        {
          .format = Geometry::AttributeFormat::RGB_F32,
          .offset = offsetof(VertexInternal_t, position),
          .stride = sizeof(VertexInternal_t),
        }
      },
      {
        Geometry::AttributeType::Normal,
        {
          .format = Geometry::AttributeFormat::RGB_F32,
          .offset = offsetof(VertexInternal_t, normal),
          .stride = sizeof(VertexInternal_t),
        }
      },
      {
        Geometry::AttributeType::Texcoord,
        {
          .format = Geometry::AttributeFormat::RG_F32,
          .offset = offsetof(VertexInternal_t, texcoord),
          .stride = sizeof(VertexInternal_t),
        }
      },
    };
  }

  static
  Geometry::AttributeOffsetMap GetAttributeOffsetMap(uint64_t buffer_offset) {
    return {
      { Geometry::AttributeType::Position,  buffer_offset },
      { Geometry::AttributeType::Normal,    buffer_offset },
      { Geometry::AttributeType::Texcoord,  buffer_offset },
    };
  }
};

/* Internal helper structure to post-process gltf data */
// struct MeshData {
//   std::vector<float> positions;
//   std::vector<float> texcoords;
//   std::vector<float> normals;
//   std::vector<float> tangents;
//   std::vector<uint16_t> joints;
//   std::vector<float> weights;
// };

//-----------------------------------------------------------------------------

std::string GetTextureRefID(cgltf_texture const& texture, std::string_view alt) {
  auto const& image = texture.image;
  std::string ref{image->name ? image->name : (image->uri ? image->uri : std::string(alt))};
  return ref;
}

Geometry::AttributeType GetAttributeType(cgltf_attribute const& attribute) {
  if (attribute.index != 0u) [[unlikely]] {
    LOGD("[GLTF] Unsupported multiple attribute of same type %s.", attribute.name);
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
      LOGD("[GLTF] Unsupported attribute type %s.", attribute.name);
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
          LOGD("[GLTF] Unsupported accessor vec4 format %d", accessor->component_type);
          return Geometry::AttributeFormat::kUnknown;
      }

    case cgltf_type_vec3:
      LOG_CHECK(accessor->component_type == cgltf_component_type_r_32f);
      return Geometry::AttributeFormat::RGB_F32;

    case cgltf_type_vec2:
      LOG_CHECK(accessor->component_type == cgltf_component_type_r_32f);
      return Geometry::AttributeFormat::RG_F32;

    default:
      LOGD("[GLTF] Unsupported accessor format.");
      return Geometry::AttributeFormat::kUnknown;
  }
}

Geometry::IndexFormat GetIndexFormat(cgltf_accessor const* accessor) {
  if (accessor->type == cgltf_type_scalar) [[likely]] {
    if (accessor->component_type == cgltf_component_type_r_32u) {
      return Geometry::IndexFormat::U32;
    }
    else if (accessor->component_type == cgltf_component_type_r_16u) {
      return Geometry::IndexFormat::U16;
    }
  }
  LOGD("[GLTF] Unsupported index format.");
  return Geometry::IndexFormat::kUnknown;
}

Geometry::Topology GetTopology(cgltf_primitive const& primitive) {
  switch (primitive.type) {
    case cgltf_primitive_type_triangles:
      return Geometry::Topology::TriangleList;

    case cgltf_primitive_type_triangle_strip:
      return Geometry::Topology::TriangleStrip;

    case cgltf_primitive_type_points:
      return Geometry::Topology::PointList;

    default:
      LOGD("[GLTF] Unsupported topology.");
      return Geometry::Topology::kUnknown;
  }
}

// void GetTextureFilter(int const filter, VkFilter &filter, VkSamplerMipmapMode &mipmapMode) {
//   switch (filter) {
//     case 9728:
//       filter = VK_FILTER_NEAREST;
//     break;

//     case 9729:
//       filter = VK_FILTER_LINEAR;
//     break;

//     case 9984:
//     case 9985:
//       filter = VK_FILTER_LINEAR;
//       mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
//     break;

//     case 9986:
//     case 9987:
//       filter = VK_FILTER_LINEAR;
//       mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
//     break;
//   };
// }

// void GetTextureWrap(int wrap, VkSamplerAddressMode &addressMode) {
//   switch (wrap) {
//     case 10497:
//       addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
//       break;

//     case 33071:
//       addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
//       break;

//     case 33648:
//       addressMode = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
//       break;
//   }
// }

// scene::Sampler GetSampler(cgltf_sampler const& sampler) {
//   scene::Sampler out;
//   GetTextureFilter(sampler.mag_filter, out.magFilter, out.mipmapMode);
//   GetTextureFilter(sampler.min_filter, out.minFilter, out.mipmapMode);
//   GetTextureWrap(sampler.wrap_s, out.addressModeU);
//   GetTextureWrap(sampler.wrap_t, out.addressModeV);
//   return out;
// }

} // namespace ""

// ----------------------------------------------------------------------------

/* ---- GLTF extractor ---- */

namespace {

void ExtractTextures(std::string const& basename, PointerToStringMap_t & texture_map, cgltf_data const* data, scene::Resources& R) {
  int constexpr kDefaultNumChannels{ 4 }; //

  stbi_set_flip_vertically_on_load(false);

  for (cgltf_size i = 0; i < data->textures_count; ++i) {
    cgltf_texture const& texture = data->textures[i];

    if (auto img = texture.image; img) {
      std::string ref;

      if (auto it = texture_map.find(&texture); it != texture_map.cend()) {
        ref = it->second;
      } else {
        ref = GetTextureRefID(texture, std::string(basename) + "::Texture_" + std::to_string(i));
        texture_map[&texture] = ref;
      }

      if (auto it = R.images.find(ref); it != R.images.end()) {
        continue;
      }

      auto image = std::make_shared<scene::Image>();

      if (auto bufferView = img->buffer_view; bufferView) {
        stbi_uc const* bufferData = reinterpret_cast<stbi_uc const*>(bufferView->buffer->data) + bufferView->offset;
        int32_t const bufferSize = static_cast<int32_t>(bufferView->size);


        auto pixel_data = stbi_load_from_memory(
          bufferData,
          bufferSize,
          &image->width,
          &image->height,
          &image->channels,
          kDefaultNumChannels
        );


        if (pixel_data) {
          image->pixels.reset(pixel_data);
        } else {
          LOGW("Fail to load texture %s.", ref.c_str());
          continue;
        }

        R.total_image_size += 4u * image->width * image->height; //

        // LOGD("**** %lu  %u  %u*%u", bufferView->size, 4u * image->width * image->height, image->width, image->height);
      } else {
        LOGW("Texture %s unsupported.", ref.c_str());
        continue;
      }

      // Reference into the scene structure.
      if (image->pixels != nullptr) {
        LOGD("[GLTF] Texture \"%s\" has been loaded.", ref.c_str());
        R.images[ref] = std::move(image);
      } else {
        LOGE("[GLTF] Texture \"%s\" failed to be loaded.", ref.c_str());
      }
    }
  }
}

void ExtractMaterials(
  std::string const& basename,
  PointerToStringMap_t &texture_map,
  PointerToStringMap_t &material_map,
  cgltf_data const* data,
  scene::Resources& R)
{
  auto textureRef = [&texture_map](cgltf_texture const& texture, std::string_view suffix) {
    auto ref = GetTextureRefID(texture, suffix);
    if (auto it = texture_map.find(&texture); it != texture_map.end()) {
      return it->second;
    }
    texture_map[&texture] = ref;
    return ref;
  };

  for (cgltf_size i = 0; i < data->materials_count; ++i) {
    cgltf_material const& mat = data->materials[i];

    auto const material_name = mat.name ? mat.name
                                        : std::string(basename + "::Material_" + std::to_string(i));
    material_map[&mat] = material_name;

    // if (R.materials.find(material_name) == R.materials.end()) continue;
    // LOG_CHECK(R.materials.find(material_name) == R.materials.end());

    std::shared_ptr<scene::Material> material;
    if (auto it = R.materials.find(material_name); it != R.materials.end()) {
      material = it->second;
    } else {
      material = std::make_shared<scene::Material>();
      material->name = material_name;
    }

    std::string const texture_prefix{ basename + "::Texture_" };

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
        } else {
          LOGD("Fails to find albedo texture '%s'", ref.c_str());
        }
      }
      if (const cgltf_texture* tex = pbr_mr.metallic_roughness_texture.texture; tex) {
        auto ref = textureRef(*tex, texture_prefix + "MetallicRough");
        if (auto it = R.images.find(ref); it != R.images.end()) {
          material->ormTexture = it->second;
        } else {
          LOGD("Fails to find metallic rough texture '%s'", ref.c_str());
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

  uint32_t matid = 0u;
  for (auto& [key, material] : R.materials) {
    material->index = matid++;
  }
}

void ExtractMeshes(
  std::string const& basename,
  PointerToStringMap_t const& material_map,
  cgltf_data const* data,
  scene::Resources& R,
  bool const bRestructureAttribs)
{
  /* Preprocess meshes nodes. */
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

  // Utility function.
  auto isAccessorOffsetFlat{[](cgltf_accessor const* acc) -> bool {
    size_t const kAccessorOffsetLimit = 2048;
    return (acc->offset == 0) || (acc->offset >= kAccessorOffsetLimit);
  }};

  /**
   * Each Mesh hold its geometry,
   * each primitive consist of a material and offset in the mesh geometry.
   * We assume every primitives of a Mesh have the same topology / attributes
   ***/

  // Parse each mesh nodes (for primitives & skeleton).
  for (auto i : meshNodeIndices) {
    cgltf_node const& node = data->nodes[i];

    uint32_t total_vertex_count{0u};
    std::vector<uint32_t> valid_prim_indices{};

    // A. Preprocess primitives.
    for (cgltf_size j = 0; j < node.mesh->primitives_count; ++j) {
      cgltf_primitive const& prim = node.mesh->primitives[j];

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
      if (prim.type != cgltf_primitive_type_triangles) {
        LOGW("[GLTF] The mesh contains non TRIANGLES primitives.");
        // continue;
      }
      // [Check] Morph targets.
      if (prim.targets_count > 0) {
        LOGW("[GLTF] Morph targets are not supported.");
      }
      // [Check] Sparse attributes.
      bool is_sparse = false;
      for (cgltf_size k = 0; k < prim.attributes_count; ++k) {
        cgltf_attribute const& attribute = prim.attributes[k];
        cgltf_accessor const* accessor = attribute.data;
        if (accessor->is_sparse) {
          LOGW("[GLTF] Sparse attributes are not supported.");
          is_sparse = true;
          break;
        }
      }
      if (is_sparse) {
        continue;
      }

      total_vertex_count += prim.attributes[0].data->count;
      valid_prim_indices.push_back(j);
    }

    if (valid_prim_indices.empty()) {
      LOGW("[GLTF] A Mesh was bypassed due to unsupported features.");
      continue;
    }

    // B. Create a new mesh.
    auto mesh = std::make_shared<scene::Mesh>();
    {
      cgltf_node_transform_world(&node, lina::ptr(mesh->world_matrix));
      mesh->submeshes.resize(valid_prim_indices.size(), {.parent = mesh.get()});
    }

    // -----------------

    // C. Retrieve its vertex attributes and indices.
    //
    //      The first branch force the attributes into a specific interleaved format
    //        allowing the engine to always assume the same layout.
    //
    //      The second take the attributes as is.
    //

    if (bRestructureAttribs) [[likely]] {

      mesh->set_attributes(VertexInternal_t::GetAttributeInfoMap());
      mesh->set_topology(Geometry::Topology::TriangleList); //

      // Hold the interleaved attributes of the mesh in the same interleaved buffer.
      std::vector<VertexInternal_t> vertices{};

      // Offset to the primitive attributes inside the mesh buffer.
      uint64_t attribs_buffer_offset{0};

      /* Parse the primitives. */
      for (size_t prim_index = 0u; prim_index < valid_prim_indices.size(); ++prim_index) {
        uint32_t const valid_prim_index{ valid_prim_indices[prim_index] };
        cgltf_primitive const& prim{ node.mesh->primitives[valid_prim_index] };
        cgltf_size const vertex_count{ prim.attributes[0].data->count };

        Geometry::Primitive primitive{};

        // Attributes.
        {
          vertices.resize(vertex_count);
          for (cgltf_size k = 0; k < prim.attributes_count; ++k) {
            cgltf_attribute const& attrib = prim.attributes[k];
            cgltf_accessor const* accessor = attrib.data;
            assert(accessor->count == vertex_count);

            // ---------------------------------------------------
            // Positions.
            if (attrib.type == cgltf_attribute_type_position) {
              LOG_CHECK(accessor->type == cgltf_type_vec3);
              // LOGD( "> loading positions." );
              for (cgltf_size vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
                auto& vertex = vertices[vertex_index];
                cgltf_accessor_read_float( accessor, vertex_index, lina::ptr(vertex.position), 3);
              }
            }
            // Normals.
            else if (attrib.type == cgltf_attribute_type_normal) {
              LOG_CHECK(accessor->type == cgltf_type_vec3);
              // LOGD( "> loading normals." );
              for (cgltf_size vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
                auto& vertex = vertices[vertex_index];
                cgltf_accessor_read_float( accessor, vertex_index, lina::ptr(vertex.normal), 3);
              }
            }
            // Tangents
            else if (attrib.type == cgltf_attribute_type_tangent) {
              // LOG_CHECK(accessor->type == cgltf_type_vec4);
              // LOGD( "> loading tangents." );

              // for (cgltf_size vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
              //   auto& vertex = vertices[vertex_index];
              //   cgltf_accessor_read_float( accessor, vertex_index, lina::ptr(vertex.tangent), 4);
              //   vec3 t3 = lina::to_vec3(tangent);
              //        t3 = vec3(linalg::mul(world_matrix, vec4(t3, 0.0f));
              //   vertex.tangent = vec4(t3, vertex.tangent.w);
              // }
            }
            // Texcoords.
            else if (attrib.type == cgltf_attribute_type_texcoord) {
              LOG_CHECK(accessor->type == cgltf_type_vec2);
              if (attrib.index > 0) {
                LOGW( "MultiTexturing is not supported yet." );
                continue;
              }
              // LOGD( "> loading texture coordinates." );
              for (cgltf_size vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
                auto& vertex = vertices[vertex_index];
                LOG_CHECK(0 != cgltf_accessor_read_float( accessor, vertex_index, lina::ptr(vertex.texcoord), 2));
              }
            }
            // Joints.
            else if (attrib.type == cgltf_attribute_type_joints) {
              // LOG_CHECK(accessor->type == cgltf_type_vec4);
              // LOGD( "> loading joint indices." );

              // for (cgltf_size vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
              //   LOG_CHECK(0 != cgltf_accessor_read_uint( accessor, vertex_index, lina::ptr(joints), 4));
              //   raw.joints.push_back( joints );
              // }
            }
            // Weights.
            else if (attrib.type == cgltf_attribute_type_weights) {
              // LOGD( "> loading joint weights." );
              // LOG_CHECK(accessor->type == cgltf_type_vec4);
              // raw.weights.reserve(vertex_count);

              // vec4 weights;
              // for (cgltf_size vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
              //   LOG_CHECK(0 != cgltf_accessor_read_float( accessor, vertex_index, lina::ptr(weights), 4));
              //   // weights = vec4(0.0, 0.0, 0.0, 0.0);
              //   raw.weights.push_back( weights );
              // }
            }
            // ---------------------------------------------------
          }

          /* Add the primitive interleaved attributes to the mesh, and retrieve its internal offset. */
          attribs_buffer_offset = mesh->add_vertices_data(
            reinterpret_cast<std::byte const*>(vertices.data()),
            vertices.size() * sizeof(vertices[0u])
          );
          primitive.vertexCount = static_cast<uint32_t>(vertex_count);
          primitive.bufferOffsets = VertexInternal_t::GetAttributeOffsetMap(attribs_buffer_offset);
        }

        // Indices.
        if (prim.indices) {
          cgltf_accessor const* accessor = prim.indices;
          cgltf_buffer_view const* buffer_view = accessor->buffer_view;
          cgltf_buffer const* buffer = buffer_view->buffer;

          if (auto index_format = GetIndexFormat(accessor); index_format != Geometry::IndexFormat::kUnknown)
          {
            // [the same index format should be shared by the whole mesh.]
            mesh->set_index_format(index_format);

            primitive.indexCount = accessor->count;
            primitive.indexOffset = mesh->add_indices_data(
              reinterpret_cast<std::byte const*>(buffer->data) + buffer_view->offset + accessor->offset,
              buffer_view->size
            );
          }
        }

        // Material.
        if (prim.material) {
          if (auto it = material_map.find(prim.material); it != material_map.cend()) {
            auto const& material_name = it->second;
            mesh->submeshes[prim_index].material = R.materials[material_name];
          } else {
            LOGD("A submesh material has not been found.");
          }
        } else {
          LOGD("> Primitive loaded without material.");
        }

        mesh->add_primitive(primitive);
      }

    } else {

      /* Setup the shared attributes from the first primitive.
       *
       * This suppose all mesh primitives share the same layout.
       */
      {
        cgltf_primitive const& prim{ node.mesh->primitives[valid_prim_indices[0u]] };

        mesh->set_topology(GetTopology(prim));

        for (cgltf_size j = 0; j < prim.attributes_count; ++j) {
          cgltf_attribute const& attribute = prim.attributes[j];
          cgltf_accessor const* accessor = attribute.data;

          if (auto type = GetAttributeType(attribute); type != Geometry::AttributeType::kUnknown) {
            size_t const accessorOffset{ isAccessorOffsetFlat(accessor) ? 0u : accessor->offset };
            mesh->add_attribute(type, {
              .format = GetAttributeFormat(accessor),
              .offset = static_cast<uint32_t>(accessorOffset),
              .stride = static_cast<uint32_t>(accessor->stride),
            });
          }
        }
      }

      /* Parse the primitives. */
      for (uint32_t j = 0u; j < valid_prim_indices.size(); ++j) {
        cgltf_primitive const& prim{ node.mesh->primitives[valid_prim_indices[j]] };
        LOG_CHECK(GetTopology(prim) == mesh->get_topology());

        Geometry::Primitive primitive{};

        /* Retrieve primitive attributes offsets, when relevant. */
        std::map<cgltf_accessor const*, uint64_t> accessor_buffer_offsets{};

        // Attributes.
        for (cgltf_size k = 0; k < prim.attributes_count; ++k) {
          cgltf_attribute const& attribute = prim.attributes[k];

          if (auto type = GetAttributeType(attribute); type != Geometry::AttributeType::kUnknown) {
            auto accessor = attribute.data;
            auto buffer_view = accessor->buffer_view;

            // (overwritten, but shared by all attributes as it's non-sparsed)
            primitive.vertexCount = accessor->count;

            uint64_t bufferOffset{};
            if (isAccessorOffsetFlat(accessor)) {
              bufferOffset = mesh->add_vertices_data(
                reinterpret_cast<std::byte const*>(buffer_view->buffer->data) + buffer_view->offset + accessor->offset,
                buffer_view->size
              );
              accessor_buffer_offsets[accessor] = bufferOffset;
            } else {
              bufferOffset = accessor_buffer_offsets[accessor];
            }
            primitive.bufferOffsets[type] = bufferOffset;
          }
        }

        // Indices.
        if (prim.indices) {
          cgltf_accessor const* accessor = prim.indices;
          cgltf_buffer_view const* buffer_view = accessor->buffer_view;
          cgltf_buffer const* buffer = buffer_view->buffer;

          if (auto index_format = GetIndexFormat(accessor); index_format != Geometry::IndexFormat::kUnknown) {
            mesh->set_index_format(index_format);

            primitive.indexCount = accessor->count;
            primitive.indexOffset = mesh->add_indices_data(
              reinterpret_cast<std::byte const*>(buffer->data) + buffer_view->offset + accessor->offset, //
              buffer_view->size
            );

            // LOGD("[GLTF] added %lu indices, for a buffer of %lu bytes, to an offset of %lu bytes.\n",
            //   accessor->count, /*buffer_view->offset, accessor->offset,*/ buffer_view->size, indicesOffset
            // );
          }
        }

        // Assign material when availables.
        if (prim.material) {
          if (auto it = material_map.find(prim.material); it != material_map.cend()) {
            mesh->submeshes[j].material = R.materials[it->second];
            // LOGD(" *** submeshes has a material named '%s'", it->second.c_str());
          } else {
            LOGD("material not found !");
          }
        } else {
          LOGD("no material for this prim ('parrently) !");
        }

        mesh->add_primitive(primitive);
      }
    }

    // -----------------

    // D. (optionnal) Retrieve the mesh skeleton.
    if (cgltf_skin const* skin = node.skin; skin) {
      auto const skinName = skin->name ? skin->name
                                       : std::string(basename + "::Skeleton_" + std::to_string(i));
      auto skel_it = R.skeletons.find(skinName);

      std::shared_ptr<scene::Skeleton> skeleton;

      if (skel_it != R.skeletons.end()) {
        skeleton = skel_it->second;
      } else [[likely]] {
        cgltf_size const jointCount = skin->joints_count;

        skeleton = std::make_shared<scene::Skeleton>(jointCount);

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
          LOG_CHECK(bufferSize == (16 * jointCount));
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
  }
}

void ExtractAnimations(std::string const& basename, cgltf_data const* data, scene::Resources& R) {
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
    std::shared_ptr<scene::Skeleton> skeleton{nullptr};
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
    auto clip = std::make_shared<scene::AnimationClip>();
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
        auto &pose = clip->poses[sid];
        auto &joint = pose.joints[jointIndex];

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

void Resources::release(std::shared_ptr<ResourceAllocator> allocator) {
  for (auto& tex : textures) {
    allocator->destroy_image(&tex);
  }

  allocator->destroy_buffer(index_buffer);
  allocator->destroy_buffer(vertex_buffer);
}

// ----------------------------------------------------------------------------

bool Resources::load_from_file(std::string_view const& filename, bool bRestructureAttribs) {
  std::string const basename{ utils::ExtractBasename(filename) };

  utils::FileReader file;
  if (!file.read(filename)) {
    return false;
  }

  cgltf_options options{};
  cgltf_result result{};
  cgltf_data* data{};

  if (result = cgltf_parse(&options, file.buffer.data(), file.buffer.size(), &data); cgltf_result_success != result) {
    LOGE("GLTF: failed to parse file \"%s\" %d.\n", basename.c_str(), result);
    return false;
  }

  result = cgltf_load_buffers(&options, data, filename.data());
  bool const bSuccess{ cgltf_result_success == result };

  if (bSuccess) [[likely]] {
    PointerToStringMap_t texture_map{};
    PointerToStringMap_t material_map{};

    // (hack) prepass to give proper name to texture when they've got none.
    // ExtractMaterials(basename, texture_map, material_map, data, *this);

    ExtractTextures(basename, texture_map, data, *this);
    ExtractMaterials(basename, texture_map, material_map, data, *this);
    ExtractMeshes(basename, material_map, data, *this, bRestructureAttribs);
    ExtractAnimations(basename, data, *this);

#ifndef NDEBUG
    // std::cout << "Image count : " << images.size() << std::endl;
    // std::cout << "Material count : " << materials.size() << std::endl;
    // std::cout << "Skeleton count : " << skeletons.size() << std::endl;
    // std::cout << "Animation count : " << animations.size() << std::endl;
    // std::cout << "Mesh count : " << meshes.size() << std::endl;
    // std::cerr << " ----------------- gltf loaded ----------------- " << std::endl;
#endif
  } else {
    LOGE("GLTF: failed to load buffers in \"%s\" %d.\n", basename.c_str(), result);
  }
  cgltf_free(data);

  return bSuccess;
}

// ----------------------------------------------------------------------------

void Resources::initialize_submesh_descriptors(Mesh::AttributeLocationMap const& attribute_to_location) {
  /* Calculate the offsets to indivual mesh data inside the shared vertices and indices buffers. */
  vertex_buffer_size = 0u;
  index_buffer_size = 0u;
  for (auto& mesh : meshes) {
    uint32_t const vertex_size = mesh->get_vertices().size();
    uint32_t const index_size = mesh->get_indices().size();
    mesh->set_device_buffer_info({
      .vertex_offset = vertex_buffer_size,
      .vertex_size = vertex_size,
      .index_offset = index_buffer_size,
      .index_size = index_size,
    });
    vertex_buffer_size += vertex_size;
    index_buffer_size += index_size;
  }

  /* Bind mesh attributes to pipeline locations. */
  for (auto& mesh : meshes) {
    mesh->initialize_submesh_descriptors(attribute_to_location);
  }

#ifndef NDEBUG
  // uint32_t const kMegabyte{ 1024u * 1024u };
  // LOGI("> vertex buffer size %f Mb", vertex_buffer_size / static_cast<float>(kMegabyte));
  // LOGI("> index buffer size %f Mb ", index_buffer_size / static_cast<float>(kMegabyte));
  // LOGI("> total image size %f Mb ", total_image_size / static_cast<float>(kMegabyte));
#endif
}

// ----------------------------------------------------------------------------

void Resources::upload_to_device(Context const& context) {
  auto allocator = context.get_resource_allocator();

  /* Transfer Textures */
  if (total_image_size > 0) {
    /* Create a staging buffer. */
    backend::Buffer staging_buffer{
      allocator->create_staging_buffer( total_image_size ) // XXX
    };

    textures.reserve(images.size()); //

    std::vector<VkBufferImageCopy> copies;
    copies.reserve(images.size());

    // ----------------

    uint32_t texture_index = 0u;
    uint64_t staging_offset = 0u;
    for (auto& [key, img] : images) {
      VkExtent3D const extent{
        .width = static_cast<uint32_t>(img->width),
        .height = static_cast<uint32_t>(img->height),
        .depth = 1u,
      };

      auto tex{context.create_image_2d(
        extent.width, extent.height, 1u, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT
      )};
      textures.push_back(tex);
      img->texture_index = textures.size() - 1u;

      /* Upload image to staging buffer */
      uint32_t const img_bytesize{
        static_cast<uint32_t>(4u * extent.width * extent.height) //
      };
      allocator->write_buffer( staging_buffer, staging_offset,  img->pixels.get(), 0u, img_bytesize);

      copies.push_back({
        .bufferOffset = staging_offset,
        .imageSubresource = {
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .layerCount = 1u,
        },
        .imageExtent = extent,
      });

      staging_offset += img_bytesize;
      ++texture_index;
    }

    // ----------------

    auto cmd{ context.create_transient_command_encoder(Context::TargetQueue::Transfer) };
    {
      VkImageLayout const transfer_layout{ VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL };

      cmd.transition_images_layout(textures, VK_IMAGE_LAYOUT_UNDEFINED, transfer_layout);
      for (uint32_t tex_id = 0u; tex_id < textures.size(); ++tex_id) {
        vkCmdCopyBufferToImage(cmd.get_handle(), staging_buffer.buffer, textures[tex_id].image, transfer_layout, 1u, &copies[tex_id]);
      }
      cmd.transition_images_layout(textures, transfer_layout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    context.finish_transient_command_encoder(cmd);
  }

  /* Transfer Buffers */
  {
    vertex_buffer = allocator->create_buffer(
      vertex_buffer_size,
      VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR,
      VMA_MEMORY_USAGE_GPU_ONLY
    );

    index_buffer = allocator->create_buffer(
      index_buffer_size,
      VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR,
      VMA_MEMORY_USAGE_GPU_ONLY
    );

    /* Copy host mesh data to staging buffer */
    backend::Buffer mesh_staging_buffer{
      allocator->create_staging_buffer(vertex_buffer_size + index_buffer_size)
    };
    {
      size_t vertex_offset{0u};
      size_t index_offset{vertex_buffer_size};
      for (auto const& mesh : meshes) {
        auto const& vertices = mesh->get_vertices();
        allocator->write_buffer(mesh_staging_buffer, vertex_offset, vertices.data(), 0u, vertices.size());
        vertex_offset += vertices.size();

        auto const& indices = mesh->get_indices();
        allocator->write_buffer(mesh_staging_buffer, index_offset, indices.data(), 0u, indices.size());
        index_offset += indices.size();

        /* clear host data once uploaded */
        mesh->clear_indices_and_vertices();
      }
    }

    /* Copy device data from staging buffers to their respective buffers. */
    auto cmd = context.create_transient_command_encoder(Context::TargetQueue::Transfer);
    {
      cmd.copy_buffer(mesh_staging_buffer, 0u, vertex_buffer, 0u, vertex_buffer_size);
      cmd.copy_buffer(mesh_staging_buffer, vertex_buffer_size, index_buffer, 0u, index_buffer_size);
    }
    context.finish_transient_command_encoder(cmd);
  }
}

}  // namespace scene

/* -------------------------------------------------------------------------- */
