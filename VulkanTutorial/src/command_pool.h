#pragma once

#include "vk_object.h"
#include "helpers.h"

class CommandPool : public VKObject
{
public:
	CommandPool() = default;
	~CommandPool() = default;

	void Create(void* createInfo) override
	{
		auto vkManager = VulkanManager::GetVulkanManager();
		VkCommandPoolCreateInfo poolInfo = *static_cast<VkCommandPoolCreateInfo*>(createInfo);
		assert(vkCreateCommandPool(vkManager.GetDevice(), &poolInfo, nullptr, &m_commandPool) == VK_SUCCESS);
	}
	
	void Destroy() override
	{
		auto vkManager = VulkanManager::GetVulkanManager();
		vkDestroyCommandPool(vkManager.GetDevice(), m_commandPool, nullptr);
	}
	
	VkCommandPool m_commandPool;
};
