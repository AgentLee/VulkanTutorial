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

#include "../libs/imgui/imgui_impl_glfw.h"
#include "../libs/imgui/imgui_impl_vulkan.h"

#define IMGUI_ENABLED true

//https://stackoverflow.com/questions/36579771/glfw-key-callback-synchronization
//https://www.reddit.com/r/opengl/comments/bn6gbv/glfw_keyboard_input_handling/
//https://stackoverflow.com/questions/7676971/pointing-to-a-function-that-is-a-class-member-glfw-setkeycallback
//https://discourse.glfw.org/t/passing-parameters-to-callbacks/848/3
//https://www.glfw.org/docs/3.3/input_guide.html
struct KeyEvent
{
	KeyEvent(int key, int code, int action, int modifiers)
	{
		this->key = key;
		this->code = code;
		this->action = action;
		this->modifiers = modifiers;
	}
	int key, code, action, modifiers;
	std::chrono::steady_clock::time_point eventTime;
};

std::map<int, bool> g_keys;
std::queue<KeyEvent> g_unhandledKeys;

void HandleInput(HelloTriangle* app, float delta_time)
{
	//Anything that should happen "when the users presses the key" should happen here
	while (!g_unhandledKeys.empty()) 
	{
		KeyEvent event = g_unhandledKeys.front();
		g_unhandledKeys.pop();
		g_keys[event.key] = event.action == GLFW_PRESS || event.action == GLFW_REPEAT;
	}
	//Anything that should happen "while the key is held down" should happen here.
	float movement[2] = { 0,0 };
	if (g_keys[GLFW_KEY_W])
		app->GetCamera().Translate(Direction::Forward);
	if (g_keys[GLFW_KEY_A])
		app->GetCamera().Translate(Direction::Left);
	if (g_keys[GLFW_KEY_S])
		app->GetCamera().Translate(Direction::Backward);
	if (g_keys[GLFW_KEY_D])
		app->GetCamera().Translate(Direction::Right);
	if (g_keys[GLFW_KEY_C])
		app->GetCamera().Translate(Direction::Up);
	if (g_keys[GLFW_KEY_SPACE])
		app->GetCamera().Translate(Direction::Down);
}

void HandleKey(GLFWwindow* window, int key, int code, int actions, int modifiers)
{
	g_unhandledKeys.emplace(key, code, actions, modifiers);
}

void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	HandleKey(window, key, scancode, action, mods);
}

void HelloTriangle::Run()
{
	InitWindow();

	VulkanManager::CreateVulkanManager(m_window);
	VulkanManager::GetVulkanManager().Initialize();
	
	InitVulkan();
	
#if IMGUI_ENABLED
	InitImGui();
#endif

	g_camera = new Camera(glm::vec3(2, 2, 2), 
		glm::vec3(0,0,0), 
		glm::vec3(0,0,1),	// hmm z is up
		45.0f,
		(float)VulkanManager::GetVulkanManager().GetSwapChainExtent().width / (float)VulkanManager::GetVulkanManager().GetSwapChainExtent().height,
		0.1f, 
		10.0f);
	
	MainLoop();

	Cleanup();
}

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

	vkWaitForFences(VulkanManager::GetVulkanManager().GetDevice(), 1, &VulkanManager::GetVulkanManager().GetInFlightFences()[m_currentFrame], VK_TRUE, UINT64_MAX);

	uint32_t imageIndex;	// Store index of the image from the swap chain.
	auto acquireResult = vkAcquireNextImageKHR(VulkanManager::GetVulkanManager().GetDevice(), VulkanManager::GetVulkanManager().GetSwapChain(), UINT64_MAX, VulkanManager::GetVulkanManager().GetImageAvailableSemaphores()[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

	// If swap chain is out of date or suboptimal, recreate it.
	if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || acquireResult == VK_SUBOPTIMAL_KHR)
	{
		frameBufferResized = false;
		RecreateSwapChain();

#if IMGUI_ENABLED
		// Should be more elegant than this
		InitImGui();
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

	{
		float now = (float)glfwGetTime();
		static float lastUpdate = now;
		float deltaTime = now - lastUpdate;
		lastUpdate = now;

		HandleInput(this, deltaTime);
	}

	g_camera->Update();
	
	// Update uniforms
	m_sampleModel.SubmitDrawCall(imageIndex, *g_camera);

#if IMGUI_ENABLED
	// Submit ImGui commands
	m_imguiManager.SubmitDrawCall(imageIndex, *g_camera);
#endif

#if IMGUI_ENABLED
	std::array<VkCommandBuffer, 2> submitCommandBuffers =
	{
		m_sampleModel.m_commandBuffers[imageIndex],
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
	m_window = glfwCreateWindow(WIDTH, HEIGHT, "Hello Triangle", nullptr, nullptr);

	// Set pointer to window
	glfwSetWindowUserPointer(m_window, this);
	
	// Detect resizing
	glfwSetFramebufferSizeCallback(m_window, FrameBufferResizeCallback);

	glfwSetKeyCallback(m_window, KeyCallback);
}

void HelloTriangle::InitImGui()
{
	// GLFW has to be initialized in main program?
	
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	// Setup Platform/Renderer bindings
	ImGui_ImplGlfw_InitForVulkan(m_window, true);

	auto queueFamily = VulkanManager::GetVulkanManager().FindQueueFamilies(VulkanManager::GetVulkanManager().GetPhysicalDevice());
	auto swapChainSupport = VulkanManager::GetVulkanManager().QuerySwapChainSupport(VulkanManager::GetVulkanManager().GetPhysicalDevice());
	
	ImGui_ImplVulkan_InitInfo init_info = {};
	{
		init_info.Instance = VulkanManager::GetVulkanManager().GetInstance();
		init_info.PhysicalDevice = VulkanManager::GetVulkanManager().GetPhysicalDevice();
		init_info.Device = VulkanManager::GetVulkanManager().GetDevice();
		init_info.QueueFamily = queueFamily.graphicsFamily.value();
		init_info.Queue = VulkanManager::GetVulkanManager().GetGraphicsQueue();
		init_info.PipelineCache = nullptr;
		init_info.DescriptorPool = m_imguiManager.GetDescriptorPool();
		init_info.Allocator = nullptr;
		init_info.MinImageCount = swapChainSupport.capabilities.minImageCount + 1;
		init_info.ImageCount = VulkanManager::GetVulkanManager().NumSwapChainImages();
		init_info.CheckVkResultFn = nullptr;
	}
	ImGui_ImplVulkan_Init(&init_info, m_imguiManager.GetRenderPass());

	// Upload Fonts
	{
		// Use any command queue
		VkCommandPool command_pool = m_imguiManager.GetCommandPool();
		VK_ASSERT(vkResetCommandPool(VulkanManager::GetVulkanManager().GetDevice(), command_pool, 0), "");
		
		VkCommandBuffer command_buffer = m_imguiManager.GetCommandBuffers()[m_currentFrame];
		
		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(command_buffer, &begin_info);

		ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

		VkSubmitInfo end_info = {};
		end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		end_info.commandBufferCount = 1;
		end_info.pCommandBuffers = &command_buffer;
		vkEndCommandBuffer(command_buffer);
		vkQueueSubmit(VulkanManager::GetVulkanManager().GetGraphicsQueue(), 1, &end_info, VK_NULL_HANDLE);

		vkDeviceWaitIdle(VulkanManager::GetVulkanManager().GetDevice());
		ImGui_ImplVulkan_DestroyFontUploadObjects();
	}

	ImGui_ImplVulkan_DestroyFontUploadObjects();
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