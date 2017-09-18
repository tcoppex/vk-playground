#ifndef ENGINE_DEVICE_UTILS_H_
#define ENGINE_DEVICE_UTILS_H_

#include <cassert>
#include <vector>
#include "engine/core.h"

/* -------------------------------------------------------------------------- */

/**
 * @brief The VertexBindingAttrib_t struct is used to define a vertex buffer
 * binding and attributes properties.
 */
struct VertexBindingAttrib_t {
  std::vector<VkVertexInputBindingDescription> bindings;
  std::vector<VkVertexInputAttributeDescription> attribs;

  void add_binding(uint32_t binding, uint32_t stride, VkVertexInputRate inputRate) {
    VkVertexInputBindingDescription b;
    b.binding = binding;
    b.stride = stride;
    b.inputRate = inputRate;
    bindings.push_back(b);
  }

  void add_attrib(uint32_t binding, uint32_t location, VkFormat format, uint32_t offset) {
    VkVertexInputAttributeDescription a;
    a.binding = binding;
    a.location = location;
    a.format = format;
    a.offset = offset;
    attribs.push_back(a);
  }
};

/* -------------------------------------------------------------------------- */

/**
 * @brief The DrawParameter_t struct holds vertex buffer parameters for
 * rendering.
 */
struct DrawParameter_t {
  DrawParameter_t() :
    num_vertices(0u)
  {}

  VkDescriptorBufferInfo desc;
  uint32_t num_vertices;

  void draw(const VkCommandBuffer &cmd) const {
    vkCmdBindVertexBuffers(cmd, 0, 1, &desc.buffer, &desc.offset);
    vkCmdDraw(cmd, num_vertices, 1, 0, 0);
  }
};

/* -------------------------------------------------------------------------- */

/**
 * @brief The DSLBinding struct allow a user to define custom Descriptor
 * Set Layout binding for the rendering context.
 */
struct DSLBinding_t {
  void add_binding(uint32_t binding,
                   VkDescriptorType type,
                   VkShaderStageFlags stages,
                   uint32_t count = 1u)
  {
    VkDescriptorSetLayoutBinding layout_binding;
    layout_binding.binding = binding;
    layout_binding.descriptorType = type;
    layout_binding.descriptorCount = count;
    layout_binding.stageFlags = stages;
    layout_binding.pImmutableSamplers = nullptr;

    bindings.push_back(layout_binding);
    desc_infos.push_back(VkDescriptorBufferInfo());
  }

  void set_desc_buffer_info(uint32_t id, VkDescriptorBufferInfo desc_info) {
    assert(id < desc_infos.size());
    desc_infos[id] = desc_info;
  }

  std::vector<VkDescriptorSetLayoutBinding> bindings;
  std::vector<VkDescriptorBufferInfo> desc_infos;
};

/* -------------------------------------------------------------------------- */

struct DescriptorSet_t {
  VkDescriptorPool pool;
  std::vector<VkDescriptorSet> sets;

  VkResult create_from_layout(VkDevice const& device,
                              VkDescriptorSetLayoutBinding const* bindings,
                              uint32_t num_binding,
                              VkDescriptorSetLayout const* layouts,
                              uint32_t num_layout) {
    /* Create the Descriptor Pool */
    std::vector<VkDescriptorPoolSize> pool_sizes(num_binding);
    for (unsigned int i=0u; i < pool_sizes.size(); ++i) {
      pool_sizes[i].type = bindings[i].descriptorType;
      pool_sizes[i].descriptorCount = bindings[i].descriptorCount;
    }

    VkDescriptorPoolCreateInfo pool_info;
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.pNext = nullptr;
    pool_info.flags = 0;
    pool_info.maxSets = 1u;
    pool_info.poolSizeCount = pool_sizes.size();
    pool_info.pPoolSizes = pool_sizes.data();
    CHECK_VK_RET( vkCreateDescriptorPool(device, &pool_info, nullptr, &pool) );

    /* Create the Descriptor Sets */
    VkDescriptorSetAllocateInfo alloc_info;
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.pNext = nullptr;
    alloc_info.descriptorPool = pool;
    alloc_info.descriptorSetCount = num_layout;
    alloc_info.pSetLayouts = layouts;

    sets.resize(num_layout);
    CHECK_VK_RET( vkAllocateDescriptorSets(device, &alloc_info, sets.data()) );

    return VK_SUCCESS;
  }

  void update_set(VkDevice const& device, const DSLBinding_t &dsl) {
    assert(!sets.empty());

    std::vector<VkWriteDescriptorSet> write_desc(dsl.bindings.size());
    for (unsigned int i=0u; i < write_desc.size(); ++i) {
      auto &b = dsl.bindings[i];
      write_desc[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write_desc[i].pNext = nullptr;
      write_desc[i].dstSet = sets[0];
      write_desc[i].descriptorCount = b.descriptorCount;
      write_desc[i].descriptorType = b.descriptorType ;
      write_desc[i].pBufferInfo = &dsl.desc_infos[i];
      write_desc[i].dstArrayElement = 0;
      write_desc[i].dstBinding = b.binding;
    }
    vkUpdateDescriptorSets(device, write_desc.size(), write_desc.data(), 0, nullptr);
  }

  void destroy(VkDevice const& device) {
    vkDestroyDescriptorPool(device, pool, nullptr);
  }
};

/* -------------------------------------------------------------------------- */

#endif  // ENGINE_DEVICE_UTILS_H_
