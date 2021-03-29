#pragma once

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <optional>
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>

#include <vector>

#include "vertex.h"

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
	// C++17 requires this to be an inline for singleton to work.
	static inline VulkanManager* s_manager;
	
public:
	VulkanManager() = default;
	VulkanManager(GLFWwindow* window) : m_window(window) {}
	~VulkanManager() = default;

	static void CreateVulkanManager(GLFWwindow* window)
	{
		s_manager = new VulkanManager(window);
	}
	static VulkanManager& GetVulkanManager()
	{
		return *VulkanManager::s_manager;
	}
	
	void Initialize();
	void CreateInstance();
	void SetupDebugMessenger();
	void CreateSurface();
	void PickPhysicalDevice();
	void CreateLogicalDevice();
	void CreateSwapChain();
	void RecreateSwapChain();
	void CleanupSwapChain();

	// I think these make more sense in helper...
	void CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiliing, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
	void CreateTextureImage(const char* path);
	VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
	void TransitionImageLayout(VkCommandPool& commandPool, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
	void CreateTextureSampler(VkSampler& sampler, float maxLod);

	// TODO: This should be a helper function.
	void GenerateMipMaps(VkCommandPool& commandPool, VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
	
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

	uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	
	VkCommandBuffer BeginSingleTimeCommands(VkCommandPool& commandPool);
	void EndSingleTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool& commandPool);

	void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory);
	void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkCommandPool& commandPool, VkDeviceSize size);
	void CopyBufferToImage(VkCommandPool& commandPool, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	
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

	GLFWwindow* GetWindow() { return m_window; };
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