#pragma once

#include "../libs/imgui/imgui.h"
#include "../libs/imgui/imgui_impl_glfw.h"
#include "../libs/imgui/imgui_impl_vulkan.h"

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>
#include "vulkan_manager.h"
#include "vulkan_base.h"
#include "imgui_manager.h"

#include "sample_model.h"

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <map>
#include <vector>
#include <optional>
#include <queue>


#include "constants.h"
#include "helpers.h"
#include "debug_layer.h"
#include "vertex.h"
#include "buffer.h"

#include "camera.h"
#include "input_manager.h"

class Window;

class HelloTriangle
{
public:
	void Run();

	// This needs to be static because GLFW doesn't know how to call it from a this pointer.
	static void FrameBufferResizeCallback(GLFWwindow* window, int width, int height);

	Camera& GetCamera() { return *g_camera; }
	ImGuiManager& GetImGui() { return m_imguiManager; }
	
	void ToggleResize(bool resized) { frameBufferResized = resized; }
	
private:
	void InitWindow();
	void MainLoop();
	void DrawFrame();
	void Cleanup();

	void InitVulkan();

	// These should be in VulkanManager
	void RecreateSwapChain();
	void CleanupSwapChain();
	
private:
	ImGuiManager m_imguiManager;
	SampleModel m_sampleModel;

	Camera* g_camera;
	
	GLFWwindow* m_window;
	size_t m_currentFrame = 0;
	bool frameBufferResized = false;
};
