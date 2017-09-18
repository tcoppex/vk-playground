#include "engine/device/buffer_manager.h"

#include <cassert>
#include <cstring>
#include "engine/device/context.h"

void DeviceBufferManager::create(uint32_t *id) {
  assert(nullptr != id);

  /* Search for an available buffer. */
  for (unsigned int i = 0u; i < size(); ++i) {
    if (!used_[i]) {
      *id = i;
      used_[i] = true;
      return;
    }
  }

  /* Otherwise resize vectors. */
  *id = size();
  resize(size() + 1u);
  used_[*id] = true;
}

void DeviceBufferManager::destroy(uint32_t id) {
  assert(id < size());
  assert(used_[id] == true);

  /* Free device memory */
  vkFreeMemory(device_, memories_[id], nullptr);

  /* Release buffer object */
  vkDestroyBuffer(device_, buffers_[id], nullptr);

  used_[id] = false;
}

VkResult DeviceBufferManager::allocate( uint32_t id,
                                        VkBufferUsageFlags usages,
                                        uint32_t bytesize,
                                        VkMemoryPropertyFlags memory_properties) {
  assert(id < size());
  assert(used_[id] == true);
  assert(buffers_[id] == VK_NULL_HANDLE);

  /* Create the buffer object */
  VkBufferCreateInfo buffer_info;
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.pNext = nullptr;
  buffer_info.usage = usages;
  buffer_info.size = bytesize;
  buffer_info.queueFamilyIndexCount = 0;
  buffer_info.pQueueFamilyIndices = nullptr;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  buffer_info.flags = 0;
  CHECK_VK_RET( vkCreateBuffer(device_, &buffer_info, nullptr, &buffers_[id]) );

  /* Retreive its memory requirements */
  vkGetBufferMemoryRequirements(device_, buffers_[id], &mem_requirements_[id]);

  //-------

  /* Allocate memory on the device that matches requiremnts */
  VkMemoryAllocateInfo alloc_info;
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.pNext = nullptr;
  alloc_info.allocationSize = mem_requirements_[id].size;
  alloc_info.memoryTypeIndex = GetMemoryTypeIndex(
    gpu_memory_, mem_requirements_[id].memoryTypeBits, memory_properties
  );
  assert(UINT32_MAX != alloc_info.memoryTypeIndex);
  CHECK_VK_RET( vkAllocateMemory(device_, &alloc_info, nullptr, &memories_[id]) );

  /* Bind memory to the buffer */
  CHECK_VK_RET( vkBindBufferMemory(device_, buffers_[id], memories_[id], 0) );

  //-------

  /* Description of the buffer */
  descriptors_[id].buffer = buffers_[id];
  descriptors_[id].offset = 0;
  descriptors_[id].range = bytesize;

  return VK_SUCCESS;
}

VkResult DeviceBufferManager::allocate( uint32_t id,
                                        VkBufferUsageFlags usages,
                                        uint32_t bytesize,
                                        void const* host_data,
                                        VkMemoryPropertyFlags memory_properties) {
  assert(nullptr != host_data);

  CHECK_VK_RET( allocate(id, usages, bytesize, memory_properties) );
  return upload(id, 0, bytesize, host_data);
}

VkResult DeviceBufferManager::upload( uint32_t id,
                                      uint32_t offset,
                                      uint32_t bytesize,
                                      void const* host_data) {
  assert(id < size());
  assert(host_data != nullptr);
  assert(offset+bytesize <= mem_requirements_[id].size);

  uint8_t *pData(nullptr);
  CHECK_VK( vkMapMemory(device_, memories_[id], offset, bytesize, 0, (void **)&pData) );
  memcpy(pData, host_data, bytesize);
  vkUnmapMemory(device_, memories_[id]);

  return VK_SUCCESS;
}

DeviceBufferManager::~DeviceBufferManager() {
  for (unsigned int i=0u; i<size(); ++i) {
    //assert(!used_[i]);
    if (used_[i]) {
      destroy(i);
    }
  }
}
