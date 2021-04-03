#pragma once

#include "../libs/imgui/imgui.h"
#include "../libs/imgui/imgui_impl_glfw.h"
#include "../libs/imgui/imgui_impl_vulkan.h"

#include <vector>

#include "vulkan_base.h"

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
	void CreateCommandBuffers() override {}
};
