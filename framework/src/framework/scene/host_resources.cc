#include "framework/scene/host_resources.h"

#include <iostream>
#include "framework/scene/private/gltf_loader.h"

/* -------------------------------------------------------------------------- */

namespace scene {

void HostResources::setup() {
  // Create default 1x1 textures for optionnal bindings.
  //  -> should it be left to each MaterialFx?
  {
    constexpr uint32_t kDefaultResourceSize = 32u;
    host_images.reserve(kDefaultResourceSize);
    textures.reserve(kDefaultResourceSize);

    auto push_default_texture{
      [&textures = this->textures, &host_images = this->host_images]
      (std::array<uint8_t, 4> const& c) -> uint32_t {
        uint32_t const texture_id = textures.size();
        textures.emplace_back( host_images.size() );
        host_images.emplace_back( c[0], c[1], c[2], c[3] );
        return texture_id;
      }
    };

    auto &bindings = default_texture_binding_;
    bindings.basecolor          = push_default_texture({255, 255, 255, 255});
    bindings.normal             = push_default_texture({128, 128, 255, 255});
    bindings.roughness_metallic = push_default_texture({  0, 255,   0,   0});
    bindings.occlusion          = push_default_texture({255, 255, 255, 255});
    bindings.emissive           = push_default_texture({  0,   0,   0,   0});
  }
}

// ----------------------------------------------------------------------------

bool HostResources::load_file(std::string_view filename) {
  std::string const basename{ utils::ExtractBasename(filename) };

  cgltf_options options{};
  cgltf_result result{};
  cgltf_data* data{};

  utils::FileReader file{};
  if (!file.read(filename)) {
    LOGE("GLTF: failed to read the file.");
    return false;
  }

  if (result = cgltf_parse(&options, file.buffer.data(), file.buffer.size(), &data); cgltf_result_success != result) {
    LOGE("GLTF: failed to parse file \"%s\" %d.\n", basename.c_str(), result);
    return false;
  }

  if (result = cgltf_load_buffers(&options, data, filename.data()); cgltf_result_success != result) {
    LOGE("GLTF: failed to load buffers in \"%s\" %d.\n", basename.c_str(), result);
    cgltf_free(data);
    return false;
  }

  /* Extract data */
  {
    using namespace internal::gltf_loader;

    // Reserve data.
    samplers.reserve(data->samplers_count + samplers.size());
    host_images.reserve(data->images_count + host_images.size());
    textures.reserve(data->textures_count + textures.size());
    material_proxies.reserve(data->materials_count + material_proxies.size());
    material_refs.reserve(data->materials_count + material_refs.size());
    skeletons.reserve(data->skins_count + skeletons.size());
    meshes.reserve(data->meshes_count + meshes.size());
    transforms.reserve(data->meshes_count + transforms.size());
    animations_map.reserve(data->animations_count);

    if constexpr (kUseAsyncLoad)
    {
      /* --- Async tasks version --- */

      auto run_task         = utils::RunTaskGeneric<void>;
      auto run_task_ret     = utils::RunTaskGeneric<PointerToIndexMap_t>;
      auto run_task_sampler = utils::RunTaskGeneric<PointerToSamplerMap_t>;

      auto taskSamplers = run_task_sampler([data, &samplers = this->samplers] {
        return ExtractSamplers(data, samplers);
      });

      auto taskSkeletons = run_task_ret([data, &skeletons = this->skeletons] {
        return ExtractSkeletons(data, skeletons);
      });

      // [real bottleneck, internally images are loaded asynchronously and must be waited for at the end]
      auto taskImageData = run_task_ret([data, &host_images = this->host_images] {
        return ExtractImages(data, host_images);
      });

      auto taskTextures = run_task_ret(
        [&taskImageData, &taskSamplers, data, &textures = this->textures] {
        auto images_indices = taskImageData.get();
        auto samplers_lut = taskSamplers.get();
        return ExtractTextures(data, images_indices, samplers_lut, textures);
      });

      auto taskMaterials = run_task_ret([
        &taskTextures,
        data,
        &material_proxies = this->material_proxies,
        &material_refs = this->material_refs,
        &default_binding = this->default_texture_binding_
      ] {
        auto textures_indices = taskTextures.get();
        return ExtractMaterials(
          data, textures_indices,
          material_proxies,
          material_refs,
          default_binding
        );
      });

      auto skeletons_indices = taskSkeletons.get();

      auto taskAnimations = run_task([data, &skeletons_indices, &skeletons = this->skeletons] {
        // ExtractAnimations(data, basename, skeletons_indices, skeletons, animations_map);
      });

      auto taskMeshes = run_task([
        &taskMaterials,
        data,
        &material_refs = this->material_refs,
        &skeletons_indices,
        &skeletons = this->skeletons,
        &meshes = this->meshes,
        &transforms = this->transforms
      ] {
        auto materials_indices = taskMaterials.get();
        ExtractMeshes(
          data,
          materials_indices, material_refs,
          skeletons_indices, skeletons,
          meshes, transforms,
          kRestructureAttribs,
          kForce32BitsIndexing
        );
      });

      taskAnimations.get();
      taskMeshes.get();
    }
    else
    {
      /* --- Serialized version --- */

      auto samplers_lut       = ExtractSamplers(data, samplers);
      auto skeletons_indices  = ExtractSkeletons(data, skeletons);
      auto images_indices     = ExtractImages(data, host_images);
      auto textures_indices   = ExtractTextures(
        data, images_indices, samplers_lut, textures
      );
      auto materials_indices  = ExtractMaterials(
        data,
        textures_indices,
        material_proxies, material_refs,
        default_texture_binding_
      );
      ExtractMeshes(
        data,
        materials_indices, material_refs,
        skeletons_indices, skeletons,
        meshes, transforms,
        kRestructureAttribs,
        kForce32BitsIndexing
      );
    }

    /* Wait for the host images to finish loading before using them. */
    for (auto & host_image : host_images) {
      host_image.getLoadAsyncResult();
    }
  }

  /* Be sure to have finished loading all images before freeing gltf data */
  cgltf_free(data);

  reset_internal_descriptors();

#ifndef NDEBUG
  // This will also display the extra data procedurally created.
  std::cout << basename << " loaded." << std::endl;
  std::cout << "┌────────────┬───── " << std::endl;
  std::cout << "│ Images     │ " << host_images.size() << std::endl;
  std::cout << "│ Textures   │ " << textures.size() << std::endl;
  std::cout << "│ Materials  │ " << material_proxies.size() << std::endl;
  std::cout << "│ Skeletons  │ " << skeletons.size() << std::endl;
  std::cout << "│ Animations │ " << animations_map.size() << std::endl;
  std::cout << "│ Meshes     │ " << meshes.size() << std::endl;
  std::cerr << "└────────────┴─────" << std::endl;

  // uint32_t const kMegabyte{ 1024u * 1024u };
  // LOGI("> vertex buffer size %f Mb", vertex_buffer_size / static_cast<float>(kMegabyte));
  // LOGI("> index buffer size %f Mb ", index_buffer_size / static_cast<float>(kMegabyte));
  // LOGI("> total image size %f Mb ", total_image_size / static_cast<float>(kMegabyte));
#endif

  return true;
}

// ----------------------------------------------------------------------------

void HostResources::reset_internal_descriptors() {
  /* Calculate the offsets to indivual mesh data inside the shared vertices
   * and indices buffers. */
  uint32_t transform_index = 0u;
  vertex_buffer_size = 0u;
  index_buffer_size = 0u;

  for (auto const& mesh : meshes) {
    // ---------
    mesh->transform_index = transform_index++; //
    mesh->set_resources_ptr(this); //
    mesh->set_buffer_info({
      .vertex_offset = vertex_buffer_size,
      .index_offset = index_buffer_size,
    });
    // ---------
    vertex_buffer_size += mesh->get_vertices().size();
    index_buffer_size += mesh->get_indices().size();
  }

  for (auto const& host_image : host_images) {
    total_image_size += host_image.getBytesize();
  }
}

}  // namespace scene

/* -------------------------------------------------------------------------- */
