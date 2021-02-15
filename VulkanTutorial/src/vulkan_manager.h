#pragma once

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <optional>
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>

#include <vector>

struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	// Ensures the device has the features we need and can present to the surface.
	bool IsComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

// The device needs to support these swap chain details.
struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;		// min/max number of images in swap chain, swap chain dimensions, transform data
	std::vector<VkSurfaceFormatKHR> formats;	// pixel format, color space
	std::vector<VkPresentModeKHR> presentModes;
};

class VulkanManager
{
public:
	VulkanManager() = default;
	VulkanManager(GLFWwindow* window) : m_window(window) {}
	~VulkanManager() = default;

	void Initialize();
	void CreateInstance();
	void SetupDebugMessenger();
	void CreateSurface();
	void PickPhysicalDevice();
	void CreateLogicalDevice();
	void CreateSwapChain();
	void RecreateSwapChain();
	VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
	void CreateImageViews();
	void CreateSyncObjects();
	
	// Helpers
	// TODO: move these to vulkan_helper.h
	void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
	bool IsDeviceCompatible(VkPhysicalDevice device);
	int RateDeviceCompatibility(VkPhysicalDevice device);
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
	VkSampleCountFlagBits GetMaxUsableSampleCount();
	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	
	// Should these be const?
	VkInstance& GetInstance() { return m_instance; }
	VkSurfaceKHR& GetSurface() { return m_surface; }
	VkDevice& GetDevice() { return m_device; }
	VkPhysicalDevice& GetPhysicalDevice() { return m_physicalDevice; }
	VkSampleCountFlagBits& GetMSAASamples() { return m_msaaSamples; }
	VkQueue& GetGraphicsQueue() { return m_graphicsQueue; }
	VkQueue& GetPresentQueue() { return m_presentQueue; }
	VkSwapchainKHR& GetSwapChain() { return m_swapChain; }
	std::vector<VkImage>& GetSwapChainImages() { return m_swapChainImages; }
	size_t NumSwapChainImages() { return m_swapChainImages.size(); }
	VkFormat& GetSwapChainImageFormat() { return m_swapChainImageFormat; }
	VkExtent2D& GetSwapChainExtent() { return m_swapChainExtent; }
	std::vector<VkImageView>& GetSwapChainImageViews() { return m_swapChainImageViews; }
	std::vector<VkFramebuffer>& GetSwapChainFrameBuffers() { return m_swapChainFrameBuffers; }
	std::vector<VkSemaphore>& GetImageAvailableSemaphores() { return m_imageAvailableSemaphores; }
	std::vector<VkSemaphore>& GetRenderFinishedSemaphores() { return m_renderFinishedSemaphores; }
	std::vector<VkFence>& GetInFlightFences() { return m_inFlightFences; }
	std::vector<VkFence>& GetImagesInFlight() { return m_imagesInFlight; }

	VkDebugUtilsMessengerEXT& GetDebugMessenger() { return m_debugMessenger; }
	
private:
	// Vulkan
	VkInstance m_instance;
	VkSurfaceKHR m_surface;
	VkDevice m_device;
	VkPhysicalDevice m_physicalDevice;
	VkSampleCountFlagBits m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;
	VkQueue m_graphicsQueue, m_presentQueue;
	VkSwapchainKHR m_swapChain;
	std::vector<VkImage> m_swapChainImages;
	VkFormat m_swapChainImageFormat;
	VkExtent2D m_swapChainExtent;
	std::vector<VkImageView> m_swapChainImageViews;
	std::vector<VkFramebuffer> m_swapChainFrameBuffers;

	// One sem to signal acquisition and another to signal rendering finished for present. 
	std::vector<VkSemaphore> m_imageAvailableSemaphores, m_renderFinishedSemaphores;
	// CPU-GPU sync
	std::vector<VkFence> m_inFlightFences;
	std::vector<VkFence> m_imagesInFlight;
	
	VkDebugUtilsMessengerEXT m_debugMessenger;

	// GLFW
	GLFWwindow* m_window;
};