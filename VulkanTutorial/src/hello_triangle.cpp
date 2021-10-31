#include "hello_triangle.h"
#include "vulkan_helper.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <map>
#include <set>
#include <cstdint>
#include <algorithm>
#include <unordered_map>
#include <filesystem>
#include <iostream>

#include "window.h"
#include "../libs/imgui/imgui_impl_glfw.h"
#include "../libs/imgui/imgui_impl_vulkan.h"

void HelloTriangle::Run()
{
	InitWindow();

	VulkanManager::CreateVulkanManager(m_window);
	VulkanManager::GetVulkanManager().Initialize();
	
	InitVulkan();
	
#if IMGUI_ENABLED
	m_imguiManager.SetupImGui(m_window, m_currentFrame);
#endif

	g_camera = new Camera(glm::vec3(0, 0, 10), 
		glm::vec3(0,0,0), 
		glm::vec3(0,1,0),
		45.0f,
		(float)VulkanManager::GetVulkanManager().GetSwapChainExtent().width / (float)VulkanManager::GetVulkanManager().GetSwapChainExtent().height,
		0.1f, 
		100.0f);
	
	MainLoop();

	Cleanup();
}
static bool test;
void HelloTriangle::MainLoop()
{
	while (!glfwWindowShouldClose(m_window))
	{
		glfwPollEvents();
		DrawFrame();
	}

	vkDeviceWaitIdle(VulkanManager::GetVulkanManager().GetDevice());
}

void HelloTriangle::DrawFrame()
{	
	// -Get image from swap chain
	// -Execute command buffer with the image as an attachment in the frame buffer
	// -Return the image to the swap chain to present

	// These events are asynchronous, we need to sync them up.
	// Semaphores vs fences:
	// -Fences used for syncing rendering
	// -Semaphores used for syncing operations across command queues

#if IMGUI_ENABLED
	m_imguiManager.Begin();

	DrawMenu();
#endif
	
	vkWaitForFences(VulkanManager::GetVulkanManager().GetDevice(), 1, &VulkanManager::GetVulkanManager().GetInFlightFences()[m_currentFrame], VK_TRUE, UINT64_MAX);

	uint32_t imageIndex;	// Store index of the image from the swap chain.
	auto acquireResult = vkAcquireNextImageKHR(VulkanManager::GetVulkanManager().GetDevice(), VulkanManager::GetVulkanManager().GetSwapChain(), UINT64_MAX, VulkanManager::GetVulkanManager().GetImageAvailableSemaphores()[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

	// If swap chain is out of date or suboptimal, recreate it.
	if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || acquireResult == VK_SUBOPTIMAL_KHR)
	{
		frameBufferResized = false;
		RecreateSwapChain();

#if IMGUI_ENABLED
		m_imguiManager.SetupImGui(m_window, m_currentFrame);
#endif
		
		return;
	}
	else if (acquireResult != VK_SUCCESS)
	{
		ASSERT(false, "Failed to acquire swap chain image");
	}

	// Check if previous frame is using this image (waiting on a fence)
	if (VulkanManager::GetVulkanManager().GetImagesInFlight()[imageIndex] != VK_NULL_HANDLE)
	{
		vkWaitForFences(VulkanManager::GetVulkanManager().GetDevice(), 1, &VulkanManager::GetVulkanManager().GetImagesInFlight()[imageIndex], VK_TRUE, UINT64_MAX);
	}

	// Mark image as being used
	VulkanManager::GetVulkanManager().GetImagesInFlight()[imageIndex] = VulkanManager::GetVulkanManager().GetInFlightFences()[m_currentFrame];

	VkSemaphore waitSemaphores[] = { VulkanManager::GetVulkanManager().GetImageAvailableSemaphores()[m_currentFrame] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSemaphore signalSemaphores[] = { VulkanManager::GetVulkanManager().GetRenderFinishedSemaphores()[m_currentFrame] };

	InputHandler::Update(this);
	
	g_camera->Update();
	
	// Update uniforms
	m_sampleModel.SubmitDrawCall(imageIndex, *g_camera);

#if IMGUI_ENABLED
	// Submit ImGui commands
	m_imguiManager.SubmitDrawCall(imageIndex, *g_camera);
	
	std::array<VkCommandBuffer, 2> submitCommandBuffers =
	{
		VulkanManager::GetVulkanManager().GetCommandBuffers()[imageIndex],
		m_imguiManager.GetCommandBuffers()[imageIndex]
	};
#else
	std::array<VkCommandBuffer, 1> submitCommandBuffers =
	{
		m_commandBuffers[imageIndex],
	};
#endif
	
	// Submit command buffer
	VkSubmitInfo submitInfo{};
	{
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		// Specify which semaphores to wait on before execution and which stage to wait on.
		// We want to wait until the image is available so when the graphics pipeline writes to the color attachment.
		// The wait semaphores should correspond to the wait stages array.
		{
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = waitSemaphores;
			submitInfo.pWaitDstStageMask = waitStages;
		}

		// Specify which command buffers to submit for execution.
		// We want to submit the buffer that binds the swap chain image we acquired as color attachment.
		{
			submitInfo.commandBufferCount = submitCommandBuffers.size();
			submitInfo.pCommandBuffers = submitCommandBuffers.data();
		}

		// Specify which semaphores to signal after execution.
		{
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = signalSemaphores;
		}

		vkResetFences(VulkanManager::GetVulkanManager().GetDevice(), 1, &VulkanManager::GetVulkanManager().GetImagesInFlight()[m_currentFrame]);

		VK_ASSERT(vkQueueSubmit(VulkanManager::GetVulkanManager().GetGraphicsQueue(), 1, &submitInfo, VulkanManager::GetVulkanManager().GetImagesInFlight()[m_currentFrame]), "Failed to submit draw command buffer");
	}
	
	// Present
	// Submit result back to swap chain toc show on screen.

	VkSwapchainKHR swapChains[] = { VulkanManager::GetVulkanManager().GetSwapChain() };
	VkPresentInfoKHR presentInfo{};
	{
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		// Specify which semaphores to wait on before presenting.
		{
			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = signalSemaphores;
		}

		// Specify swap chains to present images to and which image to present.
		{
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = swapChains;
			presentInfo.pImageIndices = &imageIndex;
		}

		// Specify array of results to check if the result was successful.
		presentInfo.pResults = nullptr;
	}

	vkQueuePresentKHR(VulkanManager::GetVulkanManager().GetPresentQueue(), &presentInfo);

	// Wait for work to finish after submission.
	vkQueueWaitIdle(VulkanManager::GetVulkanManager().GetPresentQueue());

	m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void HelloTriangle::FrameBufferResizeCallback(GLFWwindow* window, int width, int height)
{
	auto app = reinterpret_cast<HelloTriangle*>(glfwGetWindowUserPointer(window));
	app->frameBufferResized = true;
}

void HelloTriangle::InitWindow()
{
	// Initialize GLFW library
	glfwInit();

	// Disable OpenGL
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	// Create window
	// A cool thing here is that you can specify which monitor to open the window on.
	m_window = glfwCreateWindow(WIDTH, HEIGHT, "3D N-Gin", nullptr, nullptr);

	// Set pointer to window
	glfwSetWindowUserPointer(m_window, this);
	
	// Detect resizing
	glfwSetFramebufferSizeCallback(m_window, FrameBufferResizeCallback);

	glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	
	// Set callbacks
	glfwSetKeyCallback(m_window, InputHandler::KeyCallback);
	glfwSetCursorPosCallback(m_window, InputHandler::MouseCallback);
	glfwSetMouseButtonCallback(m_window, InputHandler::MouseButtonCallback);
	glfwSetCursorEnterCallback(m_window, InputHandler::MouseEnterCallback);
}

void HelloTriangle::InitVulkan()
{
	m_sampleModel.Initialize();
	m_imguiManager.Initialize();
}

void HelloTriangle::RecreateSwapChain()
{
	// Pause while the window is minimized.
	{
		int width = 0;
		int height = 0;
		glfwGetFramebufferSize(m_window, &width, &height);

		while (width == 0 || height == 0)
		{
			glfwGetFramebufferSize(m_window, &width, &height);
			glfwWaitEvents();
		}
	}
	
	vkDeviceWaitIdle(VulkanManager::GetVulkanManager().GetDevice());

	CleanupSwapChain();
	
	VulkanManager::GetVulkanManager().CreateSwapChain();
	VulkanManager::GetVulkanManager().CreateImageViews();

	m_sampleModel.Reinitialize();
	m_imguiManager.Reinitialize();
}

void HelloTriangle::CleanupSwapChain()
{
	m_imguiManager.Cleanup(true);
	m_sampleModel.Cleanup(true);
	
	for (auto& imageView : VulkanManager::GetVulkanManager().GetSwapChainImageViews())
	{
		vkDestroyImageView(VulkanManager::GetVulkanManager().GetDevice(), imageView, nullptr);
	}

	vkDestroySwapchainKHR(VulkanManager::GetVulkanManager().GetDevice(), VulkanManager::GetVulkanManager().GetSwapChain(), nullptr);
}

void HelloTriangle::DrawMenu()
{
	namespace fs = std::filesystem;


	const std::string path = "D:/Development/VulkanTutorial/models/";

	//https://www.codegrepper.com/code-examples/cpp/c%2B%2B+get+file+list+of+directory
	std::vector<std::string> modelPaths;
	for (const auto& f : fs::directory_iterator(path))
	{
		auto filePath = f.path().string();
		modelPaths.push_back(filePath);
	}

	ImGui::Begin("Models");
	{
		int curr = 0;
		for (int i = 0; i < modelPaths.size(); ++i)
		{
			const bool isSelected = curr == i;
			if (ImGui::Selectable(modelPaths[i].c_str(), isSelected))
			{
				curr = i;
			}

			if (isSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
	}
	ImGui::End();
}

void HelloTriangle::Cleanup()
{
	CleanupSwapChain();

	m_sampleModel.Cleanup(false);
	
	for(auto i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		vkDestroySemaphore(VulkanManager::GetVulkanManager().GetDevice(), VulkanManager::GetVulkanManager().GetImageAvailableSemaphores()[i], nullptr);
		vkDestroySemaphore(VulkanManager::GetVulkanManager().GetDevice(), VulkanManager::GetVulkanManager().GetRenderFinishedSemaphores()[i], nullptr);
		vkDestroyFence(VulkanManager::GetVulkanManager().GetDevice(), VulkanManager::GetVulkanManager().GetInFlightFences()[i], nullptr);
	}
	
	// Device queues (graphics queue) are implicitly destroyed when the device is destroyed.
	vkDestroyDevice(VulkanManager::GetVulkanManager().GetDevice(), nullptr);
	
	if (g_enableValidationLayers)
	{
		DebugLayer::DestroyDebugUtilsMessengerEXT(VulkanManager::GetVulkanManager().GetInstance(), VulkanManager::GetVulkanManager().GetDebugMessenger(), nullptr);
	}

	vkDestroySurfaceKHR(VulkanManager::GetVulkanManager().GetInstance(), VulkanManager::GetVulkanManager().GetSurface(), nullptr);
	
	// Don't destroy physical device since it is destroyed implicitly when the instance is destroyed.
	vkDestroyInstance(VulkanManager::GetVulkanManager().GetInstance(), nullptr);

	glfwDestroyWindow(m_window);
	glfwTerminate();
}