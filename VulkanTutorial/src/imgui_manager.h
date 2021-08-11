#pragma once

#include "../libs/imgui/imgui.h"
#include "../libs/imgui/imgui_impl_glfw.h"
#include "../libs/imgui/imgui_impl_vulkan.h"

#include <vector>

#include "helpers.h"
#include "vulkan_base.h"

#define IMGUI_ENABLED true

class ImGuiManager : public VulkanBase
{
public:
	void Initialize() override;
	void Reinitialize() override;
	void SubmitDrawCall(uint32_t imageIndex, Camera& camera) override;
	void Cleanup(bool recreateSwapchain) override;

	void CreateRenderPass() override;
	void CreateGraphicsPipeline() override {}
	void CreateDescriptorSetLayout() override;
	void CreateDescriptorSet() override {}
	void CreateDescriptorPool() override;
	void CreateCommandPool() override;
	void CreateFrameBuffers() override;
	void CreateCommandBuffers() override;

	void SetupImGui(GLFWwindow* window, size_t currentFrame);
	void Begin();
	void End();
	void SetActive(bool active) { m_active = active; }

	bool m_active = false;

	std::vector<VkCommandBuffer>& GetCommandBuffers() { return m_commandBuffers; }
	std::vector<VkCommandBuffer> m_commandBuffers;

	//VkCommandPool& GetCommandPool() { return m_commandPool.m_commandPool; }
	//CommandPool m_commandPool;
};
