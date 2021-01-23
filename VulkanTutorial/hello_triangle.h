#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <optional>

#include "constants.h"
#include "helpers.h"
#include "debug_layer.h"

class HelloTriangle
{
public:
	void Run();
	
private:
	void InitWindow();
	void MainLoop();
	void Cleanup();

	void InitVulkan();
	
	void CreateInstance();

	void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	// Need to set up the handle to the debug callback.
	void SetupDebugMessenger();
	bool CheckValidationLayerSupport();
	// Return required list of extensions based on the enabled validation layers.
	std::vector<const char*> GetRequiredExtensions();

	// Look for graphics card to use.
	void PickPhysicalDevice();

	// Check device compatibility
	bool IsDeviceCompatible(VkPhysicalDevice device);
	bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
	int RateDeviceCompatibility(VkPhysicalDevice device);

	struct QueueFamilyIndices
	{
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;

		// Ensures the device has the features we need and can present to the surface.
		bool IsComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
	};

	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);

	void CreateLogicalDevice();

	void CreateSurface();

	// The device needs to support these swap chain details.
	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities;		// min/max number of images in swap chain, swap chain dimensions, transform data
		std::vector<VkSurfaceFormatKHR> formats;	// pixel format, color space
		std::vector<VkPresentModeKHR> presentModes;	
	};

	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	void CreateSwapChain();
	void CreateImageViews();
	
	void CreateGraphicsPipeline();
	VkShaderModule CreateShaderModule(const std::vector<char>& code);
	
private:
	VkInstance m_instance;
	VkDebugUtilsMessengerEXT m_debugMessenger;
	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	VkDevice m_device;
	VkQueue m_graphicsQueue, m_presentQueue;
	VkSurfaceKHR m_surface;

	VkSwapchainKHR m_swapChain;
	std::vector<VkImage> m_swapChainImages;
	VkFormat m_swapChainImageFormat;
	VkExtent2D m_swapChainExtent;
	std::vector<VkImageView> m_swapChainImageViews;
	
	GLFWwindow* m_window;
};