#pragma once

#include "../libs/imgui/imgui.h"
#include "../libs/imgui/imgui_impl_glfw.h"
#include "../libs/imgui/imgui_impl_vulkan.h"

#include <vector>

class ImGuiManager
{
public:
	ImGuiManager(GLFWwindow* window, VkInstance& instance, VkDevice& device, VkPhysicalDevice& physicalDevice, VkQueue& graphicsQueue) :
		m_window(window), m_instance(instance), m_device(device), m_physicalDevice(physicalDevice), m_graphicsQueue(graphicsQueue) {}

	void Init();
	void DrawFrame();
	
private:
	GLFWwindow* m_window;

	VkInstance m_instance;
	VkDevice m_device;
	VkPhysicalDevice m_physicalDevice;
	VkQueue m_graphicsQueue;
	
	VkDescriptorPool m_imguiDescriptorPool;
	std::vector<VkDescriptorSet> m_imguiDescriptorSets;
	VkRenderPass m_imguiRenderPass;
	VkCommandPool m_imguiCommandPool;
	std::vector<VkCommandBuffer> m_imguiCommandBuffers;
	std::vector<VkFramebuffer> m_imguiFrameBuffers;
};