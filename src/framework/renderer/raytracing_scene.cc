#include "framework/renderer/raytracing_scene.h"

/* -------------------------------------------------------------------------- */

void RayTracingScene::init(Context const& ctx) {
  context_ptr_ = &ctx;
}

// ----------------------------------------------------------------------------

void RayTracingScene::release() {
  auto const& allocator = context_ptr_->allocator();

  vkDestroyAccelerationStructureKHR(context_ptr_->device(), tlas_.handle, nullptr);
  allocator.destroy_buffer(tlas_.buffer);
  for (auto &blas : blas_) {
    vkDestroyAccelerationStructureKHR(context_ptr_->device(), blas.handle, nullptr);
    allocator.destroy_buffer(blas.buffer);
  }
}

// ----------------------------------------------------------------------------

void RayTracingScene::build(
  scene::ResourceBuffer<scene::Mesh> const& meshes,
  backend::Buffer const& vertex_buffer,
  backend::Buffer const& index_buffer
) {
  // Refuse the whole scene if no mesh has a proper index format.
  bool hasAnyValidIndexFormat = false;
  for (auto const& mesh : meshes) {
    for (auto const& submesh : mesh->submeshes) {
      auto const indexType = submesh.draw_descriptor.indexType;
      hasAnyValidIndexFormat |= (indexType == VK_INDEX_TYPE_UINT16)
                             || (indexType == VK_INDEX_TYPE_UINT32)
                             ;
    }
  }
  if (!hasAnyValidIndexFormat) {
    LOGW("Index type not supported by VkAccelerationStructureGeometryTrianglesDataKHR.");
    return;
  }

  vertex_address_ = vertex_buffer.address;
  index_address_ = index_buffer.address;

  blas_.reserve(meshes.size()); // heuristic
  tlas_.instances.reserve(meshes.size());

  // Build a BLAS for each submeshes.
  for (auto const& mesh : meshes) {
    mat3x4 transform{
      lina::to_mat3x4(mesh->world_matrix())
    };

    for (auto const& submesh : mesh->submeshes) {
      if (build_blas(submesh)) {
        // (simply instanciate the BLAS we just built)
        tlas_.instances.emplace_back(VkAccelerationStructureInstanceKHR{
          .transform = *static_cast<float*>(lina::ptr(transform)), //
          .instanceCustomIndex = submesh.material_ref->proxy_index, //
          .mask = 0xFF,
          .instanceShaderBindingTableRecordOffset = 0,
          .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR, //
          .accelerationStructureReference = blas_.back().buffer.address,
        });
      }
    }
  }

  build_tlas();
}

// ----------------------------------------------------------------------------

bool RayTracingScene::build_blas(scene::Mesh::SubMesh const& submesh) {
  DrawDescriptor const& desc{ submesh.draw_descriptor };

  if ((desc.indexType != VK_INDEX_TYPE_UINT16)
   && (desc.indexType != VK_INDEX_TYPE_UINT32)) {
    return false;
  }

  // A - Setup the BLAS Geometry info.

  scene::Mesh const& mesh{ *submesh.parent };
  bool const is_opaque{
    submesh.material_ref->states.alpha_mode != scene::MaterialStates::AlphaMode::Blend
  };

  VkAccelerationStructureGeometryTrianglesDataKHR const tri{
    .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
    .vertexFormat  = mesh.vk_format(Geometry::AttributeType::Position),
    .vertexData    = {
      .deviceAddress = vertex_address_ + desc.vertexOffset, //
    },
    .vertexStride  = mesh.get_stride(Geometry::AttributeType::Position),
    .maxVertex     = desc.vertexCount - 1,
    .indexType     = desc.indexType,
    .indexData     = {
      .deviceAddress = index_address_ + desc.indexOffset
    },
    .transformData = { .deviceAddress = 0 }
  };

  uint32_t const primitiveCount = desc.indexCount / 3u;

  backend::BLAS blas{
    .geometry = {
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
      .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
      .geometry = {
        .triangles = tri
      },
      .flags = (is_opaque ? VK_GEOMETRY_OPAQUE_BIT_KHR : VkGeometryFlagBitsKHR{}),
    },
    .build_range_info = {
      .primitiveCount  = primitiveCount,
      .primitiveOffset = 0,
      .firstVertex     = 0,
      .transformOffset = 0,
    },
  };

  // B - Create the acceleration structure buffer.

  blas.build_geometry_info = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type  = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    .flags = blas.flags,
    .mode  = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
    .geometryCount = 1,
    .pGeometries = &blas.geometry
  };

  blas.build_sizes_info = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
  };

  vkGetAccelerationStructureBuildSizesKHR(
    context_ptr_->device(),
    VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
    &blas.build_geometry_info,
    &primitiveCount,
    &blas.build_sizes_info
  );

  blas.buffer = context_ptr_->allocator().create_buffer(
    blas.build_sizes_info.accelerationStructureSize,
      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
    | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
  );

  VkAccelerationStructureCreateInfoKHR const asInfo{
    .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
    .buffer = blas.buffer.buffer,
    .size   = blas.build_sizes_info.accelerationStructureSize,
    .type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
  };
  CHECK_VK(vkCreateAccelerationStructureKHR(
    context_ptr_->device(), &asInfo, nullptr, &blas.handle
  ));

  // C - Build the BLAS.

  build_acceleration_structure(
    &blas,
    VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
    blas.build_range_info
  );

  blas_.push_back(blas);

  return true;
}

// ----------------------------------------------------------------------------

void RayTracingScene::build_tlas() {
  auto const& allocator = context_ptr_->allocator();

  if (blas_.empty()) {
    return;
  }

#if 0
  backend::Buffer instances_buffer = context_ptr_->create_buffer_and_upload(
    tlas_.instances,
      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
    | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
  );
#else
  size_t const instances_bytesize{
    tlas_.instances.size() * sizeof(tlas_.instances[0])
  };
  backend::Buffer instances_buffer = allocator.create_buffer(
    instances_bytesize,
      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
    | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
    , VMA_MEMORY_USAGE_CPU_TO_GPU
  );
  allocator.upload_host_to_device(
    tlas_.instances.data(), instances_bytesize,
    instances_buffer
  );
#endif

  VkAccelerationStructureGeometryKHR const acceleration_structure_geometry{
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
    .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
    .geometry = {
      .instances = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .arrayOfPointers = VK_FALSE,
        .data = { .deviceAddress = instances_buffer.address }
      }
    },
  };

  tlas_.build_geometry_info = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type  = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
    .flags = tlas_.flags,
    .mode  = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
    .geometryCount = 1,
    .pGeometries = &acceleration_structure_geometry
  };

  tlas_.build_sizes_info = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
  };

  uint32_t const primitiveCount{ static_cast<uint32_t>(tlas_.instances.size()) };
  vkGetAccelerationStructureBuildSizesKHR(
    context_ptr_->device(),
    VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
    &tlas_.build_geometry_info,
    &primitiveCount,
    &tlas_.build_sizes_info
  );

  tlas_.buffer = allocator.create_buffer(
    tlas_.build_sizes_info.accelerationStructureSize,
    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
    VMA_MEMORY_USAGE_GPU_ONLY
  );

  VkAccelerationStructureCreateInfoKHR asInfo{
    .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
    .buffer = tlas_.buffer.buffer,
    .size   = tlas_.build_sizes_info.accelerationStructureSize,
    .type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
  };
  CHECK_VK(vkCreateAccelerationStructureKHR(
    context_ptr_->device(), &asInfo, nullptr, &tlas_.handle
  ));

  build_acceleration_structure(
    &tlas_,
    VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
    { .primitiveCount = primitiveCount }
  );

  allocator.destroy_buffer(instances_buffer);
}

// ----------------------------------------------------------------------------

void RayTracingScene::build_acceleration_structure(
  backend::AccelerationStructure* as,
  VkPipelineStageFlags2 dstStageMask,
  VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo
) {
  auto const& allocator = context_ptr_->allocator();

  scratch_buffer_ = allocator.create_buffer(
    as->build_sizes_info.buildScratchSize,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    VMA_MEMORY_USAGE_AUTO,
    VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT
  );
  as->build_geometry_info.dstAccelerationStructure = as->handle;
  as->build_geometry_info.scratchData.deviceAddress = scratch_buffer_.address;

  auto cmd = context_ptr_->create_transient_command_encoder(Context::TargetQueue::Transfer);
  {
    std::vector<VkAccelerationStructureBuildRangeInfoKHR *> build_range_infos{
      &buildRangeInfo
    };
    vkCmdBuildAccelerationStructuresKHR(
      cmd.get_handle(),
      1,
      &as->build_geometry_info,
      build_range_infos.data()
    );

    VkMemoryBarrier2 barrier{
      .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
      .srcStageMask  = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
      .srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
      .dstStageMask  = dstStageMask,
      .dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR
    };
    VkDependencyInfo depInfo{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .memoryBarrierCount = 1,
      .pMemoryBarriers = &barrier
    };
    vkCmdPipelineBarrier2(cmd.get_handle(), &depInfo);
  }
  context_ptr_->finish_transient_command_encoder(cmd);

  allocator.destroy_buffer(scratch_buffer_); //

  VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
    .accelerationStructure = as->handle
  };
  as->buffer.address = vkGetAccelerationStructureDeviceAddressKHR(
    context_ptr_->device(),
    &addrInfo
  );
}

/* -------------------------------------------------------------------------- */