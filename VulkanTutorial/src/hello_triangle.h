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

	// Should be a global?
	VulkanManager m_vkManager;
	ImGuiManager m_imguiManager;

private:
	void InitWindow();
	void InitImGui();
	void MainLoop();
	void DrawFrame();
	void Cleanup();

	void LoadModel();
	
	void InitVulkan();
	
	VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	
	void RecreateSwapChain();
	void CleanupSwapChain();
	
	void CreateDescriptorSetLayout();
	void CreateGraphicsPipeline();
	VkShaderModule CreateShaderModule(const std::vector<char>& code);

	void CreateRenderPasses();
	void CreateRenderPass();

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
	void CreateDescriptorSets();
	void UpdateUniformBuffers(uint32_t currentImage);
	uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	void CreateCommandBuffers();
	VkCommandBuffer BeginSingleTimeCommands();
	void EndSingleTimeCommands(VkCommandBuffer commandBuffer);

	void CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiliing, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
	void CreateTextureImage();
	void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
	void CreateTextureImageView();
	void CreateTextureSampler();
	void GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

	void CreateDepthResources();
	VkFormat FindDepthFormat();
	bool HasStencilComponent(VkFormat format);

	void CreateColorResources();
	
private:
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

	//VkSampleCountFlagBits m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;
	VkImage m_colorImage;
	VkDeviceMemory m_colorImageMemory;
	VkImageView m_colorImageView;
	
	size_t m_currentFrame = 0;

	bool frameBufferResized = false;
	
	GLFWwindow* m_window;

private:
	std::vector<Vertex> m_vertices;
	std::vector<uint32_t> m_indices;
};