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
	void SubmitDrawCall(uint32_t imageIndex) override;
	void Cleanup() override;

	void CreateRenderPass() override;
	void CreateDescriptorSetLayout() override;
	void CreateDescriptorPool() override;
	void CreateCommandPool() override;
};
