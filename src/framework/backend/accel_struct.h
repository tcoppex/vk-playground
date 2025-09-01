#ifndef VKFRAMEWORK_BACKEND_ACCEL_STRUCT_H_
#define VKFRAMEWORK_BACKEND_ACCEL_STRUCT_H_

#include "framework/backend/types.h"
#include <vector>

/* -------------------------------------------------------------------------- */

namespace backend {

struct AccelerationStructure {
  VkAccelerationStructureKHR handle{VK_NULL_HANDLE};
  backend::Buffer buffer{};
  VkAccelerationStructureBuildGeometryInfoKHR build_geometry_info{};
  VkAccelerationStructureBuildSizesInfoKHR build_sizes_info{};
};

struct BLAS : AccelerationStructure {
  VkBuildAccelerationStructureFlagsKHR flags{
    VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
  | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR
  };
  VkAccelerationStructureGeometryKHR geometry{};
  VkAccelerationStructureBuildRangeInfoKHR build_range_info{};
};

struct TLAS : AccelerationStructure {
  std::vector<VkAccelerationStructureInstanceKHR> instances{};

  VkBuildAccelerationStructureFlagsKHR flags{
    VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
  // | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR
  };
};

}

/* -------------------------------------------------------------------------- */

#endif