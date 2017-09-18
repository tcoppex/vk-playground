#ifndef ENGINE_DEVICE_BUFFER_MANAGER_H_
#define ENGINE_DEVICE_BUFFER_MANAGER_H_

#include <vector>
#include "engine/core.h"

class DeviceBufferManager {
 public:
  /**/
  void create(uint32_t *id);

  /**/
  void destroy(uint32_t id);

  /**/
  VkResult allocate(uint32_t id,
                    VkBufferUsageFlags usages,
                    uint32_t bytesize,
                    VkMemoryPropertyFlags memory_properties);

  VkResult allocate(uint32_t id,
                    VkBufferUsageFlags usages,
                    uint32_t bytesize,
                    void const* host_data,
                    VkMemoryPropertyFlags memory_properties);

  /**/
  VkResult upload(uint32_t id,
                  uint32_t offset,
                  uint32_t bytesize,
                  void const* host_data);

  /* Return the buffer descriptor */
  inline
  VkDescriptorBufferInfo const& descriptor(uint32_t id) const {
    return descriptors_[id];
  }


 private:
  DeviceBufferManager(VkDevice const& device,
                      VkPhysicalDeviceMemoryProperties const& gpu_memory) :
    device_(device),
    gpu_memory_(gpu_memory)
  {}

  ~DeviceBufferManager();

  inline
  size_t size() const {
    return buffers_.empty() ? 0uL : buffers_.size();
  }

  inline
  void resize(size_t size) {
    buffers_.resize(size, VK_NULL_HANDLE);
    mem_requirements_.resize(size);
    descriptors_.resize(size);
    memories_.resize(size, VK_NULL_HANDLE);
    used_.resize(size, false);
  }

  VkDevice const& device_;
  VkPhysicalDeviceMemoryProperties const& gpu_memory_; //

  std::vector<VkBuffer> buffers_;
  std::vector<VkMemoryRequirements> mem_requirements_;
  std::vector<VkDescriptorBufferInfo> descriptors_;
  std::vector<VkDeviceMemory> memories_;                   //< [ used an external device memory handler instead ]
  std::vector<bool> used_;

 private:
  friend class DeviceContext; //

  DISALLOW_COPY_AND_ASSIGN(DeviceBufferManager);
};

#endif // ENGINE_DEVICE_BUFFER_MANAGER_H_
