#pragma once

#include "vulkan_manager.h"

#include <vulkan/vulkan.h>

class Image
{
public:
	Image() = default;
	~Image() = default;

	void CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits msaaSamples, VkFormat colorFormat, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags memoryProperties)
	{
		VulkanManager::GetVulkanManager().CreateImage(
			width,
			height,
			mipLevels,
			msaaSamples,
			colorFormat,
			tiling,
			usage,
			memoryProperties,
			m_image,
			m_memory);
	}

	void CreateImage(uint32_t mipLevels, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags memoryProperties)
	{
		auto vkManager = VulkanManager::GetVulkanManager();
		VkFormat colorFormat = vkManager.GetSwapChainImageFormat();

		vkManager.CreateImage(
			vkManager.GetSwapChainExtent().width,
			vkManager.GetSwapChainExtent().height,
			1,
			vkManager.GetMSAASamples(),
			colorFormat,
			tiling,
			usage,
			memoryProperties,
			m_image,
			m_memory);
	}

	void CreateView(VkFormat format, VkImageAspectFlagBits aspect, uint32_t mipLevels)
	{
		m_view = VulkanManager::GetVulkanManager().CreateImageView(m_image, format, aspect, mipLevels);
	}

	void Cleanup()
	{
		vkDestroyImageView(VulkanManager::GetVulkanManager().GetDevice(), m_view, nullptr);
		vkDestroyImage(VulkanManager::GetVulkanManager().GetDevice(), m_image, nullptr);
		vkFreeMemory(VulkanManager::GetVulkanManager().GetDevice(), m_memory, nullptr);
	}
	
	VkImage m_image;
	VkDeviceMemory m_memory;
	VkImageView m_view;
};
