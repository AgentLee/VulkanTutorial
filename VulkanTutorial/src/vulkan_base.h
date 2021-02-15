#pragma once

#include "vulkan_manager.h"
#include "vulkan_helper.h"

#include <vector>

class VulkanBase
{
public:
	VulkanBase() = default;
	VulkanBase(VulkanManager& vulkanManager) : m_vkManager(vulkanManager) {}
	~VulkanBase() = default;
	
	virtual void Initialize() = 0;
	virtual void SubmitDrawCall(uint32_t imageIndex) = 0;
	virtual void Cleanup() = 0;

	virtual void CreateRenderPass() = 0;
	virtual void CreateDescriptorSetLayout() = 0;
	virtual void CreateDescriptorPool() = 0;
	virtual void CreateCommandPool() = 0;

	VkDescriptorPool& GetDescriptorPool() { return m_descriptorPool; }
	std::vector<VkDescriptorSet>& GetDescriptorSets() { return  m_descriptorSets; }
	VkRenderPass& GetRenderPass() { return m_renderPass; }
	VkCommandPool& GetCommandPool() { return m_commandPool; }
	std::vector<VkCommandBuffer>& GetCommandBuffers() { return m_commandBuffers; }
	std::vector<VkFramebuffer>& GetFrameBuffers() { return m_frameBuffers; }

	// This should be global singleton?
	VulkanManager m_vkManager;

	VkDescriptorPool m_descriptorPool;
	std::vector<VkDescriptorSet> m_descriptorSets;
	VkRenderPass m_renderPass;
	VkCommandPool m_commandPool;
	std::vector<VkCommandBuffer> m_commandBuffers;
	std::vector<VkFramebuffer> m_frameBuffers;
};