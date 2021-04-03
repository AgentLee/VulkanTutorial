#pragma once

#include "helpers.h"
#include "vulkan_manager.h"

class Buffer
{
public:
	Buffer() {}

	template<typename T>
	Buffer(std::vector<T>& data, VkCommandPool& commandPool, VkBufferUsageFlags usage)
	{
		m_size = sizeof(T) * data.size();

		// Create a temporary buffer -----
		// Use the staging buffer to map and copy vertex data.
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;

		// VK_BUFFER_USAGE_TRANSFER_SRC_BIT - Use this buffer as the source in transferring memory.
		VulkanManager::GetVulkanManager().CreateBuffer(m_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		// Fill the vertex buffer -----
		Map(stagingBufferMemory, data.data(), static_cast<size_t>(m_size));

		// VK_BUFFER_USAGE_TRANSFER_DST_BIT - Use this buffer as the destination in transferring memory.
		// VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT - The most optimal use of memory. We need to use a staging buffer for this since it's not directly accessible with CPU.
		// The vertex buffer is device local. This means that we can't map memory directly to it but we can copy data from another buffer over.
		VulkanManager::GetVulkanManager().CreateBuffer(m_size, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_buffer, m_memory);

		// Copy from staging to vertex buffer
		VulkanManager::GetVulkanManager().CopyBuffer(stagingBuffer, m_buffer, commandPool, m_size);

		// Destroy staging buffer
		{
			vkDestroyBuffer(VulkanManager::GetVulkanManager().GetDevice(), stagingBuffer, nullptr);
			vkFreeMemory(VulkanManager::GetVulkanManager().GetDevice(), stagingBufferMemory, nullptr);
		}
	}
	
	Buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) : m_size(size)
	{
		VulkanManager::GetVulkanManager().CreateBuffer(size, usage, properties, m_buffer, m_memory);
	}

	template<typename T>
	void Map(VkDeviceMemory memory, T* copyData, size_t size)
	{
		auto device = VulkanManager::GetVulkanManager().GetDevice();
		
		void* data;
		//VK_ASSERT(vkMapMemory(VulkanManager::GetVulkanManager().GetDevice(), m_memory, 0, m_size, 0, &data), "Failed to map buffer");
		vkMapMemory(device, memory, 0, size, 0, &data);
		memcpy(data, copyData, size);
		vkUnmapMemory(device, memory);
	}

	template<typename T>
	void Map(T* copyData)
	{
		auto device = VulkanManager::GetVulkanManager().GetDevice();
		
		void* data;
		//VK_ASSERT(vkMapMemory(VulkanManager::GetVulkanManager().GetDevice(), m_memory, 0, m_size, 0, &data), "Failed to map buffer");
		vkMapMemory(device, m_memory, 0, m_size, 0, &data);
		memcpy(data, copyData, static_cast<size_t>(m_size));
		vkUnmapMemory(device, m_memory);
	}

	VkDeviceSize m_size;
	VkBuffer m_buffer;
	VkDeviceMemory m_memory;
};
