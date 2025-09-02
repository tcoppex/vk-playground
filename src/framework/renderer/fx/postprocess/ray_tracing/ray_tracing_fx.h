#ifndef VKFRAMEWORK_RENDERER_FX_RAY_TRACING_FX_H_
#define VKFRAMEWORK_RENDERER_FX_RAY_TRACING_FX_H_

#include "framework/renderer/fx/postprocess/generic_fx.h"

/* -------------------------------------------------------------------------- */

class RayTracingFx : public virtual GenericFx {
 public:
  virtual void release() {
    allocator_ptr_->destroy_buffer(sbt_storage_);
    GenericFx::release();
  }

  virtual void setTLAS(backend::TLAS const& tlas);

 protected:
  std::string getShaderName() const final { return ""; } //

  DescriptorSetLayoutParamsBuffer getDescriptorSetLayoutParams() const override {
    return {
      {
        .binding = 0, //
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        .descriptorCount = 1u,
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
                    | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
      },
    };
  }

  void createPipeline() final {
    auto shaders_map = createShaderModules();
    auto pipeline_desc = pipelineDescriptor(shaders_map);
    pipeline_ = renderer_ptr_->create_raytracing_pipeline(
      pipeline_layout_,
      pipeline_desc
    );
    for (auto const& [_, shaders] : shaders_map) {
      for (auto const& shader : shaders) {
        context_ptr_->release_shader_module(shader);
      }
    }

    buildShaderBindingTable(pipeline_desc);
  }

  void setImageInputs(std::vector<backend::Image> const& inputs) final {
  }
  void setBufferInputs(std::vector<backend::Buffer> const& inputs) final {
  }

  void execute(CommandEncoder& cmd) final {
    // (buffer & images barriers in)
    // cmd.bind_pipeline(pipeline_);
    // // cmd.bind_descriptor_set(descriptor_set_, pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT);
    // pushConstant(cmd);
    // vkCmdTraceRaysKHR(
    //   cmd.get_handle(),
    //   &region_.raygen,
    //   &region_.miss,
    //   &region_.hit,
    //   &region_.callable,
    //   32, 32, 1 //width, height, 1
    // );
    // (buffer & images barriers out)
  }

 protected:
  virtual backend::ShadersMap createShaderModules() const = 0;

  virtual RayTracingPipelineDescriptor_t pipelineDescriptor(backend::ShadersMap const& shaders_map) = 0;

  void buildShaderBindingTable(RayTracingPipelineDescriptor_t const& desc) {
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };
    VkPhysicalDeviceProperties2 props2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    props2.pNext = &rtProps;
    vkGetPhysicalDeviceProperties2(context_ptr_->physical_device(), &props2);
    uint32_t const handleSize         = rtProps.shaderGroupHandleSize;
    uint32_t const handleAlignment    = rtProps.shaderGroupHandleAlignment;
    uint32_t const handleSizeAligned  = utils::AlignTo(handleSize, handleAlignment);
    uint32_t const baseAlignment      = rtProps.shaderGroupBaseAlignment;
    uint32_t const numGroups          = desc.shaderGroups.size();

    std::vector<std::byte> shader_handles(numGroups * handleSize);
    CHECK_VK(vkGetRayTracingShaderGroupHandlesKHR(
      context_ptr_->device(),
      pipeline_.get_handle(),
      0,
      numGroups,
      shader_handles.size(),
      shader_handles.data()
    ));

    uint32_t const numRayGen  = static_cast<uint32_t>(desc.raygens.size());
    uint32_t const numMiss    = static_cast<uint32_t>(desc.misses.size());
    uint32_t const numHit     = static_cast<uint32_t>(desc.anyHits.size()
                                                    + desc.closestHits.size()
                                                    + desc.intersections.size());
    uint32_t const numCallable = static_cast<uint32_t>(desc.callables.size());

    size_t const offsetRayGen   = 0;
    size_t const sizeRayGen     = numRayGen * handleSizeAligned;
    size_t const offsetMiss     = utils::AlignTo(offsetRayGen + sizeRayGen, baseAlignment);
    size_t const sizeMiss       = numMiss * handleSizeAligned;
    size_t const offsetHit      = utils::AlignTo(offsetMiss + sizeMiss, baseAlignment);
    size_t const sizeHit        = numHit * handleSizeAligned;
    size_t const offsetCallable = utils::AlignTo(offsetHit + sizeHit, baseAlignment);
    size_t const sizeCallable   = numCallable * handleSizeAligned;

    size_t const sbt_buffersize = offsetCallable + sizeCallable;

    sbt_storage_ = allocator_ptr_->create_buffer(
      sbt_buffersize,
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR
      | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
      | VK_BUFFER_USAGE_TRANSFER_DST_BIT
      | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
      ,
      VMA_MEMORY_USAGE_CPU_TO_GPU
    );

    backend::Buffer staging_buffer = allocator_ptr_->create_staging_buffer(sbt_buffersize);

    // Map staging and fill regions with shader handles
    {
      void* mapped;
      allocator_ptr_->map_memory(staging_buffer, &mapped);

      uint8_t* pData = reinterpret_cast<uint8_t*>(mapped);

      auto copyHandles = [&](uint32_t firstGroup, uint32_t count, size_t dstOffset) {
        for (uint32_t i = 0; i < count; i++) {
          std::byte* src = shader_handles.data() + (firstGroup + i) * handleSize;
          std::byte* dst = reinterpret_cast<std::byte*>(pData + dstOffset + i * handleSizeAligned);
          std::memcpy(dst, src, handleSize);
        }
      };

      uint32_t groupOffset = 0;
      copyHandles(groupOffset, numRayGen, offsetRayGen);
      groupOffset += numRayGen;

      copyHandles(groupOffset, numMiss, offsetMiss);
      groupOffset += numMiss;

      copyHandles(groupOffset, numHit, offsetHit);
      groupOffset += numHit;

      copyHandles(groupOffset, numCallable, offsetCallable);

      allocator_ptr_->unmap_memory(staging_buffer);
    }

    context_ptr_->copy_buffer(
      staging_buffer, sbt_storage_, sbt_buffersize
    );
    context_ptr_->wait_device_idle();

    auto getRegion = [&](size_t offset, size_t size) -> VkStridedDeviceAddressRegionKHR {
      VkBufferDeviceAddressInfo addrInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = sbt_storage_.buffer,
      };

      VkDeviceAddress baseAddress = vkGetBufferDeviceAddressKHR(
        context_ptr_->device(), &addrInfo
      );
      return {
        .deviceAddress = baseAddress + offset,
        .stride = handleSizeAligned,
        .size = size,
      };
    };

    region_.raygen   = getRegion(offsetRayGen, sizeRayGen);
    region_.miss     = getRegion(offsetMiss, sizeMiss);
    region_.hit      = getRegion(offsetHit, sizeHit);
    region_.callable = getRegion(offsetCallable, sizeCallable);
  }

 protected:
  backend::Buffer sbt_storage_{};
  RayTracingAddressRegion region_{};
};

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_RENDERER_FX_RAY_TRACING_FX_H_