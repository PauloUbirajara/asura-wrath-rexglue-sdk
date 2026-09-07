/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    Tom Clay, 2026 - Adapted for ReXGlue runtime
 */

#include <rex/assert.h>
#include <rex/logging.h>
#include <rex/ui/vulkan/single_layout_descriptor_set_pool.h>

namespace rex {
namespace ui {
namespace vulkan {

SingleLayoutDescriptorSetPool::SingleLayoutDescriptorSetPool(
    const VulkanDevice* const vulkan_device, const uint32_t pool_set_count,
    const uint32_t set_layout_descriptor_counts_count,
    const VkDescriptorPoolSize* const set_layout_descriptor_counts,
    const VkDescriptorSetLayout set_layout)
    : vulkan_device_(vulkan_device), pool_set_count_(pool_set_count), set_layout_(set_layout) {
  assert_not_null(vulkan_device);
  assert_not_zero(pool_set_count);
  pool_descriptor_counts_.resize(set_layout_descriptor_counts_count);
  for (uint32_t i = 0; i < set_layout_descriptor_counts_count; ++i) {
    VkDescriptorPoolSize& pool_descriptor_type_count = pool_descriptor_counts_[i];
    const VkDescriptorPoolSize& set_layout_descriptor_type_count = set_layout_descriptor_counts[i];
    pool_descriptor_type_count.type = set_layout_descriptor_type_count.type;
    pool_descriptor_type_count.descriptorCount =
        set_layout_descriptor_type_count.descriptorCount * pool_set_count;
  }
}

SingleLayoutDescriptorSetPool::~SingleLayoutDescriptorSetPool() {
  const VulkanDevice::Functions& dfn = vulkan_device_->functions();
  const VkDevice device = vulkan_device_->device();
  if (current_pool_ != VK_NULL_HANDLE) {
    dfn.vkDestroyDescriptorPool(device, current_pool_, nullptr);
  }
  for (VkDescriptorPool pool : full_pools_) {
    dfn.vkDestroyDescriptorPool(device, pool, nullptr);
  }
}

size_t SingleLayoutDescriptorSetPool::Allocate() {
  if (!descriptor_sets_free_.empty()) {
    size_t free_index = descriptor_sets_free_.back();
    descriptor_sets_free_.pop_back();
    return free_index;
  }

  const VulkanDevice::Functions& dfn = vulkan_device_->functions();
  const VkDevice device = vulkan_device_->device();

  // Two iterations so if vkAllocateDescriptorSets fails even with a non-zero
  // current_pool_sets_remaining_, another attempt will be made in a new pool.
  for (uint32_t i = 0; i < 2; ++i) {
    if (current_pool_ != VK_NULL_HANDLE && !current_pool_sets_remaining_) {
      full_pools_.push_back(current_pool_);
      current_pool_ = VK_NULL_HANDLE;
    }
    if (current_pool_ == VK_NULL_HANDLE) {
      VkDescriptorPoolCreateInfo pool_create_info;
      pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
      pool_create_info.pNext = nullptr;
      pool_create_info.flags = 0;
      pool_create_info.maxSets = pool_set_count_;
      pool_create_info.poolSizeCount = uint32_t(pool_descriptor_counts_.size());
      pool_create_info.pPoolSizes = pool_descriptor_counts_.data();
      if (dfn.vkCreateDescriptorPool(device, &pool_create_info, nullptr, &current_pool_) !=
          VK_SUCCESS) {
        REXLOG_ERROR(
            "SingleLayoutDescriptorSetPool: Failed to create a descriptor pool");
        return SIZE_MAX;
      }
      std::vector<VkDescriptorSetLayout> layouts(pool_set_count_, set_layout_);
      std::vector<VkDescriptorSet> new_sets(pool_set_count_);
      VkDescriptorSetAllocateInfo descriptor_set_allocate_info;
      descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      descriptor_set_allocate_info.pNext = nullptr;
      descriptor_set_allocate_info.descriptorPool = current_pool_;
      descriptor_set_allocate_info.descriptorSetCount = pool_set_count_;
      descriptor_set_allocate_info.pSetLayouts = layouts.data();
      if (dfn.vkAllocateDescriptorSets(device, &descriptor_set_allocate_info, new_sets.data()) !=
          VK_SUCCESS) {
        REXLOG_ERROR("SingleLayoutDescriptorSetPool: Failed to bulk allocate descriptor sets");
        dfn.vkDestroyDescriptorPool(device, current_pool_, nullptr);
        current_pool_ = VK_NULL_HANDLE;
        return SIZE_MAX;
      }
      size_t base_index = descriptor_sets_.size();
      descriptor_sets_.insert(descriptor_sets_.end(), new_sets.begin(), new_sets.end());
      for (size_t k = pool_set_count_ - 1; k >= 1; --k) {
        descriptor_sets_free_.push_back(base_index + k);
      }
      full_pools_.push_back(current_pool_);
      current_pool_ = VK_NULL_HANDLE;
      current_pool_sets_remaining_ = 0;
      return base_index;
    }
  }

  return SIZE_MAX;
}

}  // namespace vulkan
}  // namespace ui
}  // namespace rex
