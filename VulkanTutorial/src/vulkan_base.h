#pragma once

#include "vulkan_manager.h"
#include "vulkan_helper.h"

#include <vector>

class VulkanBase
{
public:
	VulkanBase() = default;
	~VulkanBase() = default;
	
	virtual void Initialize() = 0;
	virtual void Reinitialize() = 0;
	virtual void SubmitDrawCall(uint32_t imageIndex) = 0;
	virtual void Cleanup(bool recreateSwapchain) = 0;

	virtual void CreateRenderPass() = 0;
	virtual void CreateGraphicsPipeline() = 0;
	virtual void CreateDescriptorSet() = 0;
	virtual void CreateDescriptorSetLayout() = 0;
	virtual void CreateDescriptorPool() = 0;
	virtual void CreateCommandPool() = 0;
	virtual void CreateCommandBuffers() = 0;
	virtual void CreateFrameBuffers() = 0;

	// Getters
	VkDescriptorPool& GetDescriptorPool() { return m_descriptorPool; }
	VkDescriptorSetLayout& GetDescriptorSetLayout() { return m_descriptorSetLayout; }
	std::vector<VkDescriptorSet>& GetDescriptorSets() { return  m_descriptorSets; }
	VkRenderPass& GetRenderPass() { return m_renderPass; }
	VkCommandPool& GetCommandPool() { return m_commandPool; }
	std::vector<VkCommandBuffer>& GetCommandBuffers() { return m_commandBuffers; }
	std::vector<VkFramebuffer>& GetFrameBuffers() { return m_frameBuffers; }

	// Manage the memory being used to store/allocate buffers.
	// Write all the ops you want into the command buffer then
	// tell Vulkan which commands to execute.
	VkDescriptorPool m_descriptorPool;
	VkDescriptorSetLayout m_descriptorSetLayout;
	std::vector<VkDescriptorSet> m_descriptorSets;

	VkPipelineLayout m_pipelineLayout;
	VkPipeline m_graphicsPipeline;
	
	// This should be outside in Renderer
	VkRenderPass m_renderPass;
	VkCommandPool m_commandPool;
	std::vector<VkCommandBuffer> m_commandBuffers;
	std::vector<VkFramebuffer> m_frameBuffers;
};