#include "framework/renderer/raytracing_scene.h"

/* -------------------------------------------------------------------------- */

void RaytracingScene::init(Context const& ctx) {
  context_ptr_ = &ctx;
}

// ----------------------------------------------------------------------------

void RaytracingScene::build(
  scene::ResourceBuffer<scene::Mesh> const& meshes,
  backend::Buffer const& vertex_buffer,
  backend::Buffer const& index_buffer
) {
  vertex_address_ = vertex_buffer.address;
  index_address_ = index_buffer.address;

  blas_.reserve(meshes.size()); //
  tlas_.instances.reserve(meshes.size());

  // Build a BLAS for each submeshes.
  for (auto const& mesh : meshes) {
    mat3x4 transform{
      lina::to_mat3x4(mesh->world_matrix())
    };

    for (auto const& submesh : mesh->submeshes) {
      build_blas(submesh);

      // (simply instanciate the blas we just built)
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

// ----------------------------------------------------------------------------

void RaytracingScene::build_blas(scene::Mesh::SubMesh const& submesh) {
  auto const& allocator = context_ptr_->allocator();

  // A - Setup the BLAS Geometry info.

  scene::Mesh const& mesh{ *submesh.parent };
  DrawDescriptor const& desc{ submesh.draw_descriptor };

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
    context_ptr_->get_device(),
    VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
    &blas.build_geometry_info,
    &primitiveCount,
    &blas.build_sizes_info
  );
  blas.buffer = allocator.create_buffer(
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
    context_ptr_->get_device(), &asInfo, nullptr, &blas.handle
  ));

  // C - Build the BLAS.

  // ------------------------
  // ------------------------
  scratch_buffer_ = allocator.create_buffer(
    blas.build_sizes_info.accelerationStructureSize,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
  );
  blas.build_geometry_info.dstAccelerationStructure = blas.handle;
  blas.build_geometry_info.scratchData.deviceAddress = scratch_buffer_.address;

  auto cmd = context_ptr_->create_transient_command_encoder(Context::TargetQueue::Transfer);
  {
    std::vector<VkAccelerationStructureBuildRangeInfoKHR *> build_range_infos{
      &blas.build_range_info
    };
    vkCmdBuildAccelerationStructuresKHR(
      cmd.get_handle(),
      1,
      &blas.build_geometry_info,
      build_range_infos.data()
    );

    VkMemoryBarrier2 barrier{
      .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
      .srcStageMask  = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
      .srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
      .dstStageMask  = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
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
  // ------------------------
  // ------------------------

  VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
    .accelerationStructure = blas.handle
  };
  blas.buffer.address = vkGetAccelerationStructureDeviceAddressKHR(
    context_ptr_->get_device(),
    &addrInfo
  );

  blas_.push_back(blas);
}

// ----------------------------------------------------------------------------

void RaytracingScene::build_tlas() {
#if 0
  VkDeviceAddress instAddr = get_buffer_address(dev, tlas_.instances.vk);

  VkAccelerationStructureGeometryInstancesDataKHR instData{
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
    .arrayOfPointers = VK_FALSE,
    .data = { .deviceAddress = instAddr }
  };

  VkAccelerationStructureGeometryKHR geom{
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
    .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
    .geometry = { .instances = instData }
  };

  VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type  = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
    .flags = tlas_.flags,
    .mode  = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
    .geometryCount = 1,
    .pGeometries   = &geom
  };

  uint32_t primCount = tlas_.count; // = instances.size()
  VkAccelerationStructureBuildSizesInfoKHR sizes{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
  vkGetAccelerationStructureBuildSizesKHR(dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primCount, &sizes);

  tlas_.as.buffer = allocator_ptr_->create_buffer({
    .size  = sizes.accelerationStructureSize,
    .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    .memory = MemoryUsage::DeviceLocal
  });
  tlas_.as.size = sizes.accelerationStructureSize;

  VkAccelerationStructureCreateInfoKHR asInfo{
    .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
    .buffer = tlas_.as.buffer.vk,
    .size   = sizes.accelerationStructureSize,
    .type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
  };
  CHECK_VK(vkCreateAccelerationStructureKHR(dev, &asInfo, nullptr, &tlas_.as.handle));
#endif

#if 0
  reserve_or_resize_scratch(sizes.buildScratchSize);

  buildInfo.dstAccelerationStructure = tlas_.as.handle;
  buildInfo.scratchData.deviceAddress = get_buffer_address(dev, scratch_.vk);

  VkAccelerationStructureBuildRangeInfoKHR range{ .primitiveCount = primCount };
  const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;

  vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);

  // barrière post-build vers SHADER_READ pour usage en RT
  VkMemoryBarrier2 barrier{
    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
    .srcStageMask  = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
    .srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
    .dstStageMask  = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
    .dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR
  };
  VkDependencyInfo dep{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &barrier };
  vkCmdPipelineBarrier2(cmd, &dep);

  tlas_.as.deviceAddress = get_as_address(dev, tlas_.as.handle);
#endif
}

// ----------------------------------------------------------------------------

void RaytracingScene::release() {
  auto const& allocator = context_ptr_->allocator();

  vkDestroyAccelerationStructureKHR(context_ptr_->get_device(), tlas_.handle, nullptr);
  allocator.destroy_buffer(tlas_.buffer);
  for (auto &blas : blas_) {
    vkDestroyAccelerationStructureKHR(context_ptr_->get_device(), blas.handle, nullptr);
    allocator.destroy_buffer(blas.buffer);
  }
}

/* -------------------------------------------------------------------------- */