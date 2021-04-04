#pragma once

#include "vulkan_manager.h"
#include <vulkan/vulkan.h>

class VKObject
{
public:
	VKObject() = default;
	~VKObject() = default;

	virtual void Create(void* createInfo) = 0;
	virtual void Destroy() = 0;
};