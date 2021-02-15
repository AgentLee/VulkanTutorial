#pragma once

#include "../libs/imgui/imgui.h"
#include "../libs/imgui/imgui_impl_glfw.h"
#include "../libs/imgui/imgui_impl_vulkan.h"

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <optional>

#include "constants.h"
#include "helpers.h"
#include "debug_layer.h"
#include "vertex.h"

class HelloTriangle
{
public:
	void Run();

	// This needs to be static because GLFW doesn't know how to call it from a this pointer.
	static void FrameBufferResizeCallback(GLFWwindow* window, int width, int height);
	
private:
	void InitWindow();
	void InitImGui();
	void MainLoop();
	void DrawFrame();
	void Cleanup();

	void LoadModel();
	
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
	VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	
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
	void RecreateSwapChain();
	void CleanupSwapChain();
	void CreateImageViews();
	
	void CreateDescriptorSetLayout();
	void CreateGraphicsPipeline();
	VkShaderModule CreateShaderModule(const std::vector<char>& code);

	void CreateRenderPasses();
	void CreateRenderPass();
	void CreateImGuiRenderPass();

	void CreateFrameBuffers();

	void CreateCommandPool();

	void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory);
	void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	// TODO: abstract out vbo and ibo functions.
	void CreateVertexBuffer();
	void CreateIndexBuffer();
	void CreateUniformBuffers();
	void CreateDescriptorPools();
	void CreateMainDescriptorPool();
	void CreateImGuiDescriptorPool();
	void CreateDescriptorSets();
	void UpdateUniformBuffers(uint32_t currentImage);
	uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	void CreateCommandBuffers();
	void CreateCommandBuffers(VkCommandBuffer* commandBuffer, uint32_t commandBufferCount, VkCommandPool& commandPool);
	void CreateImGuiCommandBuffers();
	VkCommandBuffer BeginSingleTimeCommands();
	void EndSingleTimeCommands(VkCommandBuffer commandBuffer);

	void CreateSyncObjects();

	void CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiliing, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
	void CreateTextureImage();
	void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
	VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
	void CreateTextureImageView();
	void CreateTextureSampler();
	void GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

	void CreateDepthResources();
	VkFormat FindDepthFormat();
	bool HasStencilComponent(VkFormat format);

	VkSampleCountFlagBits GetMaxUsableSampleCount();
	void CreateColorResources();
	
public:
	size_t NumSwapChainImages() { return m_swapChainImages.size(); }

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
	std::vector<VkFramebuffer> m_swapChainFrameBuffers;
	
	VkRenderPass m_renderPass;
	VkDescriptorSetLayout m_descriptorSetLayout;
	VkPipelineLayout m_pipelineLayout;
	VkPipeline m_graphicsPipeline;

	// Manage the memory being used to store/allocate buffers.
	// Write all the ops you want into the command buffer then
	// tell Vulkan which commands to execute.
	VkCommandPool m_commandPool;
	std::vector<VkCommandBuffer> m_commandBuffers;
	VkDescriptorPool m_descriptorPool;
	std::vector<VkDescriptorSet> m_descriptorSets;
	
	// One sem to signal acquisition and another to signal rendering finished for present. 
	std::vector<VkSemaphore> m_imageAvailableSemaphores, m_renderFinishedSemaphores;
	// CPU-GPU sync
	std::vector<VkFence> m_inFlightFences;
	std::vector<VkFence> m_imagesInFlight;

	// Buffers should be allocated in one go.
	VkBuffer m_vertexBuffer;
	VkDeviceMemory m_vertexBufferMemory;
	VkBuffer m_indexBuffer;
	VkDeviceMemory m_indexBufferMemory;

	// Copying new data each frame so no staging buffer.
	// Multiple buffers make sense since multiple frames can be in flight at the same time.
	std::vector<VkBuffer> m_uniformBuffers;
	std::vector<VkDeviceMemory> m_uniformBuffersMemory;

	// Textures
	uint32_t m_mipLevels;
	VkImage m_textureImage;
	VkDeviceMemory m_textureImageMemory;
	VkImageView m_textureImageView;
	VkSampler m_textureSampler;

	// Depth
	VkImage m_depthImage;
	VkDeviceMemory m_depthImageMemory;
	VkImageView m_depthImageView;

	VkSampleCountFlagBits m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;
	VkImage m_colorImage;
	VkDeviceMemory m_colorImageMemory;
	VkImageView m_colorImageView;
	
	size_t m_currentFrame = 0;

	bool frameBufferResized = false;
	
	GLFWwindow* m_window;

	// IMGUI
	VkDescriptorPool m_imguiDescriptorPool;
	std::vector<VkDescriptorSet> m_imguiDescriptorSets;
	VkRenderPass m_imguiRenderPass;
	VkCommandPool m_imguiCommandPool;
	std::vector<VkCommandBuffer> m_imguiCommandBuffers;
	std::vector<VkFramebuffer> m_imguiFrameBuffers;

private:
	std::vector<Vertex> m_vertices;
	std::vector<uint32_t> m_indices;
};