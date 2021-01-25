#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <optional>
#include <array>

#include "constants.h"
#include "helpers.h"
#include "debug_layer.h"

struct Vertex
{
	glm::vec2 position;
	glm::vec3 color;

	static VkVertexInputBindingDescription GetBindingDescription()
	{
		// Determine per-vertex or per-instance, how many bytes between data entries.
		VkVertexInputBindingDescription desc{};
		{
			desc.binding = 0;	// Index of the binding in the array.
			desc.stride = sizeof(Vertex);	// We're packing everything into one struct.
			desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		}

		return desc;
	}

	static std::array<struct VkVertexInputAttributeDescription, 2> GetAttributeDescriptions()
	{
		// Describes how to handle vertex data.
		std::array<VkVertexInputAttributeDescription, 2> desc{};
		{
			// Position
			{
				desc[0].binding = 0;							// Which binding the data is coming from
				desc[0].location = 0;							// Where to find in the shader
				desc[0].format = VK_FORMAT_R32G32_SFLOAT;		// Type of data
				desc[0].offset = offsetof(Vertex, position);	// Bytes between data
			}

			// Color
			{
				desc[1].binding = 0;							// Which binding the data is coming from
				desc[1].location = 1;							// Where to find in the shader
				desc[1].format = VK_FORMAT_R32G32B32_SFLOAT;	// Type of data
				desc[1].offset = offsetof(Vertex, color);		// Bytes between data
			}
		}

		return desc;
	}
};

class HelloTriangle
{
public:
	void Run();

	// This needs to be static because GLFW doesn't know how to call it from a this pointer.
	static void FrameBufferResizeCallback(GLFWwindow* window, int width, int height);
	
private:
	void InitWindow();
	void MainLoop();
	void DrawFrame();
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
	void RecreateSwapChain();
	void CleanupSwapChain();
	void CreateImageViews();
	
	void CreateDescriptorSetLayout();
	void CreateGraphicsPipeline();
	VkShaderModule CreateShaderModule(const std::vector<char>& code);

	void CreateRenderPass();

	void CreateFrameBuffers();

	void CreateCommandPool();

	void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory);
	void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	// TODO: abstract out vbo and ibo functions.
	void CreateVertexBuffer();
	void CreateIndexBuffer();
	void CreateUniformBuffers();
	void CreateDescriptorPool();
	void CreateDescriptorSets();
	void UpdateUniformBuffers(uint32_t currentImage);
	uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	void CreateCommandBuffers();

	void CreateSyncObjects();

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
	
	size_t m_currentFrame = 0;

	bool frameBufferResized = false;
	
	GLFWwindow* m_window;

private:
	const std::vector<Vertex> vertices = {
		{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
		{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
		{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
		{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
	};

	const std::vector<uint16_t> indices = {
		0, 1, 2, 2, 3, 0
	};
};