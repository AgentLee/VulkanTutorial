#include "vulkan_manager.h"

#include <algorithm>
#include <set>

#include "constants.h"
#include "helpers.h"
#include "debug_layer.h"
#include "vulkan_helper.h"

void VulkanManager::Initialize()
{
	CreateInstance();
	SetupDebugMessenger();
	CreateSurface();
	PickPhysicalDevice();
	CreateLogicalDevice();
	CreateSwapChain();
	CreateImageViews();
	CreateSyncObjects();
}

void VulkanManager::CreateInstance()
{
	if (g_enableValidationLayers && !DebugLayer::CheckValidationLayerSupport())
	{
		throw std::runtime_error("Validation layers requested are not available");
	}

	// You can potentially optimize the application by filing out information in the struct.
	VkApplicationInfo appInfo{};
	{
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "N-Gin";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_2;	// I have 1.2 installed, but stay consistent with tutorial.
	}

	// Call outside since I bracketed all the createInfo stuff.
	// Ensures these don't get destroyed before we actually create the instance.
	auto requiredExtensions = GetRequiredExtensions();
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;

	VkInstanceCreateInfo createInfo{};
	{
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		// Check extension support
		{
			// Query for number of extensions.
			uint32_t extensionCount = 0;
			vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

			// Get those extensions.
			std::vector<VkExtensionProperties> availableExtensions(extensionCount);
			vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

			// Check for availability.
			DebugLayer::CheckExtensions(requiredExtensions, availableExtensions);
		}

		createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
		createInfo.ppEnabledExtensionNames = requiredExtensions.data();

		if (g_enableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(g_validationLayers.size());
			createInfo.ppEnabledLayerNames = g_validationLayers.data();

			PopulateDebugMessengerCreateInfo(debugCreateInfo);
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		}
		else
		{
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}
	}

	// General pattern:
	// Pointer to creation info
	// Pointer to custom allocator callbacks (nullptr)
	// Pointer to variable that stores the object
	VK_ASSERT(vkCreateInstance(&createInfo, nullptr, &m_instance), "Failed to create Vulkan instance");
}

void VulkanManager::SetupDebugMessenger()
{
	if (!g_enableValidationLayers)
		return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	PopulateDebugMessengerCreateInfo(createInfo);

	VK_ASSERT(DebugLayer::CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger), "Failed to set up debug messenger");
}

void VulkanManager::CreateSurface()
{
	VK_ASSERT(glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface), "Failed to create window surface");
}

void VulkanManager::PickPhysicalDevice()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

	if (deviceCount == 0)
	{
		throw std::runtime_error("Vulkan supported GPU not detected");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

#ifdef SELECT_FIRST_DEVICE
	for (const auto& device : devices)
	{
		if (IsDeviceCompatible(device))
		{
			m_physicalDevice = device;
			m_msaaSamples = GetMaxUsableSampleCount();
			break;
		}
	}

	if (m_physicalDevice == VK_NULL_HANDLE)
	{
		throw std::runtime_error("Couldn't find suitable GPU");
	}
#else
	// Query all GPUs and look for the best one.
	std::multimap<int, VkPhysicalDevice> candidates;
	for (const auto& device : devices)
	{
		int score = m_vkManager.RateDeviceCompatibility(device);
		candidates.insert(std::make_pair(score, device));
	}

	if (candidates.rbegin()->first > 0)
	{
		m_physicalDevice = candidates.rbegin()->second;
	}
	else
	{
		throw std::runtime_error("Couldn't find suitable GPU");
	}
#endif
}

void VulkanManager::CreateLogicalDevice()
{
	if (m_physicalDevice == VK_NULL_HANDLE)
	{
		_ASSERT("Physical device is null");
	}

	auto indices = FindQueueFamilies(m_physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

	// TODO: Extract all these into their own functions
	// Describes the number of queues for a single queue family.
	float queuePriority = 1;
	for (auto& queueFamily : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;

		queueCreateInfos.push_back(queueCreateInfo);
	}

	// Specify device features
	VkPhysicalDeviceFeatures deviceFeatures{};
	{
		deviceFeatures.samplerAnisotropy = VK_TRUE;
	}

	// Create logical device
	VkDeviceCreateInfo createInfo{};
	{
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(g_deviceExtensions.size());
		createInfo.ppEnabledExtensionNames = g_deviceExtensions.data();

		if (g_enableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(g_validationLayers.size());
			createInfo.ppEnabledLayerNames = g_validationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}
	}

	VK_ASSERT(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device), "Failed to create logical device");

	// Assign handles to queue
	vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_graphicsQueue);
	vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_presentQueue);
}

void VulkanManager::CreateSwapChain()
{
	auto swapChainSupport = QuerySwapChainSupport(m_physicalDevice);

	auto surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
	auto presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
	auto extent = ChooseSwapExtent(swapChainSupport.capabilities);

	// How many images in swap chain? Add one to prevent waiting on the driver.
	auto imageCount = swapChainSupport.capabilities.minImageCount + 1;
	// Clamp to the max number of images allowed
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
	{
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo{};
	{
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = m_surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;								// How many layers each image consists of.
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;	// What operations the images are for. We're rendering directly to the image.
																		// VK_IMAGE_USAGE_TRANSFER_DST_BIT would be for post processing

		// How to handle swap chain images across multiple queue families.
		auto indices = FindQueueFamilies(m_physicalDevice);
		uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

		if (indices.graphicsFamily != indices.presentFamily)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;	// Image owned by one queue family at a time - requires ownership transfer.
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;	// Image used by multiple queue families - no ownership transfer.
			createInfo.queueFamilyIndexCount = 0;
			createInfo.pQueueFamilyIndices = nullptr;
		}

		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;	// How the image should be transformed
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;	// Ignore alpha channel.
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;

		// TODO:
		// It's possible for Vulkan to invalidate the swap chain during its run (resize window).
		// We need to recreate the swap chain and reference the old swap chain here.
		createInfo.oldSwapchain = VK_NULL_HANDLE;
	}

	VK_ASSERT(vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain), "Failed to create swap chain");

	// Get handles to the swap chain images.
	vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);
	m_swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, m_swapChainImages.data());

	m_swapChainImageFormat = surfaceFormat.format;
	m_swapChainExtent = extent;
}

void VulkanManager::RecreateSwapChain()
{
	// todo
}

void VulkanManager::CleanupSwapChain()
{
	// todo
}

void VulkanManager::CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples,
	VkFormat format, VkImageTiling tiliing, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image,
	VkDeviceMemory& imageMemory)
{
	VkImageCreateInfo imageInfo{};
	{
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = static_cast<uint32_t>(width);
		imageInfo.extent.height = static_cast<uint32_t>(height);
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = mipLevels;
		imageInfo.arrayLayers = 1;
		imageInfo.format = format;
		imageInfo.tiling = tiliing;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = numSamples;
		imageInfo.flags = 0;
	}

	// Create the image
	VK_ASSERT(vkCreateImage(m_device, &imageInfo, nullptr, &image), "Failed to create image");

	VkMemoryRequirements memoryRequirements{};
	vkGetImageMemoryRequirements(m_device, image, &memoryRequirements);

	VkMemoryAllocateInfo allocInfo{};
	{
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memoryRequirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, properties);
	}

	VK_ASSERT(vkAllocateMemory(m_device, &allocInfo, nullptr, &imageMemory), "Failed to allocate image memory");

	vkBindImageMemory(m_device, image, imageMemory, 0);
}

VkImageView VulkanManager::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels)
{
	VkImageViewCreateInfo viewInfo{};
	{
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;

		// How to interpret the image data
		{
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = format;
		}

		// Swizzle color channels
		{
			viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		}

		// Describes what the purpose of the image is and how to access the data
		{
			viewInfo.subresourceRange.aspectMask = aspectFlags;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = mipLevels;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;
		}
	}

	VkImageView imageView;
	VK_ASSERT(vkCreateImageView(m_device, &viewInfo, nullptr, &imageView), "Failed to create image view");

	return imageView;
}

void VulkanManager::TransitionImageLayout(VkCommandPool& commandPool, VkImage image, VkFormat format, VkImageLayout oldLayout,
	VkImageLayout newLayout, uint32_t mipLevels)
{
	// Transition image from old layout to new layout

	// TODO: caller should be setting these
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands(commandPool);

	// Use a barrier to sync access to resources, transition image layouts, transfer queue family ownership.
	// It's important to be explicit in what you're doing with the resource.

	VkImageMemoryBarrier barrier{};
	{
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;

		// Which part of the image are we syncing
		{
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = mipLevels;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;

			if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
			{
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

				if (vkHelpers::HasStencilComponent(format))
				{
					barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
				}
			}
			else
			{
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			}
		}
	}

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	// Transition rules:
	// Undefined --> transfer destination
	// Transfer destination --> shader read
	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		// TODO: read up more on this
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;	// Earliest pipeline stage
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	else
	{
		ASSERT(false, "Unsupported layout transition");
	}

	vkCmdPipelineBarrier(
		commandBuffer,
		sourceStage,		// Which stage the operations should occur before the barrier
		destinationStage,	// Which stage should wait on the barrier
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&barrier);

	// TODO: caller should be setting these
	EndSingleTimeCommands(commandBuffer, commandPool);
}

void VulkanManager::CreateTextureSampler(VkSampler& sampler, float maxLod)
{
	// Textures should be accessed through a sampler.
// Samplers will apply filtering and transformations.

	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);

	VkSamplerCreateInfo samplerInfo{};
	{
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

		// How to interpolate texels
		{
			samplerInfo.magFilter = VK_FILTER_LINEAR;
			samplerInfo.minFilter = VK_FILTER_LINEAR;
		}

		// Clamp, Wrap
		{
			samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		}

		// Limit the amount of texel samples used to calculate the final color.
		{
			samplerInfo.anisotropyEnable = VK_TRUE;
			samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
		}

		// Which color is returned when sampling beyond the image.
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

		// Which coordinate system to use to address texels the image.
		// TRUE -> [0, texWidth) and [0, texHeight)
		// FALSE -> [0, 1)
		samplerInfo.unnormalizedCoordinates = VK_FALSE;

		// Texels will be compared to a value and the result is used for filtering.
		// TODO: future chapter.
		{
			samplerInfo.compareEnable = VK_FALSE;
			samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		}

		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = maxLod;
	}

	// The sampler is distinct from the image. The sampler is a way to get data from a texture so we don't need to ref the image here.
	// This is different than other APIs which requires referring to the actual image.
	VK_ASSERT(vkCreateSampler(m_device, &samplerInfo, nullptr, &sampler), "Couldn't create texture sampler");
}

void VulkanManager::GenerateMipMaps(VkCommandPool& commandPool, VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
{
	// Check if format supports linear blitting. Not all platforms support this.
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(m_physicalDevice, imageFormat, &formatProperties);

	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
	{
		throw std::runtime_error("No support for linear blitting");
	}

	VkCommandBuffer commandBuffer = BeginSingleTimeCommands(commandPool);

	VkImageMemoryBarrier barrier{};
	{
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = image;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;
	}

	auto mipWidth = texWidth;
	auto mipHeight = texHeight;

	for (uint32_t i = 1; i < mipLevels; ++i)
	{
		auto currentMipLevel = i - 1;
		auto nextMipLevel = i;

		barrier.subresourceRange.baseMipLevel = currentMipLevel;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0,
			0,
			nullptr,
			0,
			nullptr,
			1,
			&barrier);

		// Transition current mip level to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
		VkImageBlit blit{};
		{
			// Specify regions of the blit.

			{
				blit.srcOffsets[0] = { 0,0,0 };
				blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
				blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blit.srcSubresource.mipLevel = currentMipLevel;
				blit.srcSubresource.baseArrayLayer = 0;
				blit.srcSubresource.layerCount = 1;
			}

			{
				blit.dstOffsets[0] = { 0,0,0 };
				blit.dstOffsets[1] = {
					mipWidth > 1 ? mipWidth / 2 : 1,		// Divide by two since each mip level is half the previous.
					mipHeight > 1 ? mipHeight / 2 : 1,
					1
				};
				blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blit.dstSubresource.mipLevel = nextMipLevel;
				blit.dstSubresource.baseArrayLayer = 0;
				blit.dstSubresource.layerCount = 1;
			}
		}

		// Blitting between different levels of the same image, source and destination are the same.
		vkCmdBlitImage(
			commandBuffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&blit,
			VK_FILTER_LINEAR);

		// Transition from current mip level to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		{
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(
				commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier);
		}

		if (mipWidth > 1)
			mipWidth /= 2;
		if (mipHeight > 1)
			mipHeight /= 2;
	}

	// Transition the last mip level.
	{
		barrier.subresourceRange.baseMipLevel = mipLevels - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier);
	}

	EndSingleTimeCommands(commandBuffer, commandPool);
}

void VulkanManager::CreateImageViews()
{
	// Describe how to use the image.

	m_swapChainImageViews.resize(m_swapChainImages.size());

	for (size_t i = 0; i < m_swapChainImages.size(); ++i)
	{
		m_swapChainImageViews[i] = CreateImageView(m_swapChainImages[i], m_swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
	}
}

void VulkanManager::CreateSyncObjects()
{
	// Resize sync objects.
	{
		m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
		m_imagesInFlight.resize(m_swapChainImages.size(), VK_NULL_HANDLE);
	}

	VkSemaphoreCreateInfo semaphoreInfo{};
	{
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	}

	VkFenceCreateInfo fenceInfo{};
	{
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	}

	for (auto i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		VK_ASSERT(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]), "Failed to create semaphore");
		VK_ASSERT(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]), "Failed to create semaphore");
		VK_ASSERT(vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]), "Failed to create fence");
	}
}

void VulkanManager::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	// What kind of severities we want.
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	// What kind of messages we want.
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	// Specify the callback function.
	createInfo.pfnUserCallback = DebugLayer::DebugCallback;
	createInfo.pUserData = nullptr;
}

bool VulkanManager::CheckDeviceExtensionSupport(VkPhysicalDevice device)
{
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(g_deviceExtensions.begin(), g_deviceExtensions.end());

	for (const auto& extension : availableExtensions)
	{
		requiredExtensions.erase(extension.extensionName);
	}

	// Return true if all extensions are found.
	return requiredExtensions.empty();
}

bool VulkanManager::IsDeviceCompatible(VkPhysicalDevice device)
{
#if 1
	auto indices = FindQueueFamilies(device);

	bool extensionsSupported = CheckDeviceExtensionSupport(device);

	bool swapChainUsable = false;
	if (extensionsSupported)
	{
		auto swapChainSupport = QuerySwapChainSupport(device);
		swapChainUsable = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	// TODO: These should probably be an enum.
	VkPhysicalDeviceFeatures supportedFeatures;
	vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

	return indices.IsComplete() && extensionsSupported && swapChainUsable && supportedFeatures.samplerAnisotropy;
#else
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);

	// Features include texture compression, 64 bit floats, multi viewport rendering (VR)
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	// Return after finding the first compatible GPU and has required features
	return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && deviceFeatures.geometryShader;
#endif
}

int VulkanManager::RateDeviceCompatibility(VkPhysicalDevice device)
{
	int score = 0;

	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);

	// Features include texture compression, 64 bit floats, multi viewport rendering (VR)
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	// Discrete GPUs have performance advantage.
	if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
	{
		score += 100;
	}

	// Maximum possible size of textures affects graphics quality
	score += deviceProperties.limits.maxImageDimension2D;

	// We need geometry shaders
	if (!deviceFeatures.geometryShader)
	{
		score = 0;
	}

	return score;
}

QueueFamilyIndices VulkanManager::FindQueueFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	for (uint32_t i = 0; i < queueFamilyCount; ++i)
	{
		auto queueFamily = queueFamilies[i];
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			indices.graphicsFamily = i;
		}

		// Can the device present to the surface?
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
		if (presentSupport)
		{
			indices.presentFamily = i;
		}

		if (indices.IsComplete())
		{
			break;
		}
	}

	return indices;
}

VkSampleCountFlagBits VulkanManager::GetMaxUsableSampleCount()
{
	VkPhysicalDeviceProperties physicalDeviceProperties;
	vkGetPhysicalDeviceProperties(m_physicalDevice, &physicalDeviceProperties);

	VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;

	if (counts & VK_SAMPLE_COUNT_64_BIT)
		return VK_SAMPLE_COUNT_64_BIT;
	if (counts & VK_SAMPLE_COUNT_32_BIT)
		return VK_SAMPLE_COUNT_32_BIT;
	if (counts & VK_SAMPLE_COUNT_16_BIT)
		return VK_SAMPLE_COUNT_16_BIT;
	if (counts & VK_SAMPLE_COUNT_8_BIT)
		return VK_SAMPLE_COUNT_8_BIT;
	if (counts & VK_SAMPLE_COUNT_4_BIT)
		return VK_SAMPLE_COUNT_4_BIT;
	if (counts & VK_SAMPLE_COUNT_2_BIT)
		return VK_SAMPLE_COUNT_2_BIT;

	return VK_SAMPLE_COUNT_1_BIT;
}

SwapChainSupportDetails VulkanManager::QuerySwapChainSupport(VkPhysicalDevice device)
{
	SwapChainSupportDetails details;
	{
		// Query surface capabilities
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

		// Query supported surface formats
		{
			uint32_t formatCount = 0;
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
			if (formatCount != 0)
			{
				details.formats.resize(formatCount);
				vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
			}
		}

		// Query available formats
		{
			uint32_t presentModeCount = 0;
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);
			if (presentModeCount != 0)
			{
				details.presentModes.resize(presentModeCount);
				vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.presentModes.data());
			}
		}
	}

	return details;
}

VkSurfaceFormatKHR VulkanManager::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
	for (const auto& availableFormat : availableFormats)
	{
		// We are using SRGB.
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return availableFormat;
		}
	}

	// Could create a stronger condition that rates like the device.
	return availableFormats[0];
}

VkPresentModeKHR VulkanManager::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
	// Vertical Blank = refresh
	// VK_PRESENT_MODE_IMMEDIATE_KHR - immediately show next image (possible tearing)
	// VK_PRESENT_MODE_FIFO_KHR - display takes image from front of the queue when the display refreshed. waits to add to back buffer if full.
	// VK_PRESENT_MODE_FIFO_RELAXED_KHR - present the image immediately without waiting for refresh or the queue to fill (could show empty frames)
	// VK_PRESENT_MODE_MAILBOX_KHR - replace back buffer with newest image. implement with triple buffering.

	for (const auto& mode : availablePresentModes)
	{
		if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return mode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanManager::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
	// Resolution of the swap chain images. This should be the same resolution as the window.
	if (capabilities.currentExtent.width != UINT32_MAX)
	{
		return capabilities.currentExtent;
	}
	else
	{
		// Since Vulkan works with pixels, we need to get that information from GLFW which is in screen coordinates.
		int width, height;
		glfwGetFramebufferSize(m_window, &width, &height);

		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height),
		};

		// Clamp resolution
		actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

		return actualExtent;
	}
}

uint32_t VulkanManager::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	// Query the types of memory

	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memoryProperties);

	// Find the memory type that is suitable with the buffer and make sure it has the properties to do so.
	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
	{
		if (typeFilter & (1 << i) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	ASSERT(false, "Failed to find suitable memory type");

	return -1;
}

VkCommandBuffer VulkanManager::BeginSingleTimeCommands(VkCommandPool& commandPool)
{
	VkCommandBufferAllocateInfo allocInfo{};
	{
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = commandPool;
		allocInfo.commandBufferCount = 1;
	}

	VkCommandBuffer commandBuffer;
	VK_ASSERT(vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer), "Failed to allocate command buffers");

	VkCommandBufferBeginInfo beginInfo{};
	{
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	}

	VK_ASSERT(vkBeginCommandBuffer(commandBuffer, &beginInfo), "Failed to begin command buffer");

	return commandBuffer;
}

void VulkanManager::EndSingleTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool& commandPool)
{
	vkEndCommandBuffer(commandBuffer);

	// Stop recording since we already recorded the copy command.
	VkSubmitInfo submitInfo{};
	{
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
	}

	VK_ASSERT(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE), "Failed to submit queue");

	// Wait for the transfer to finish.
	VK_ASSERT(vkQueueWaitIdle(m_graphicsQueue), "Failed to wait");

	vkFreeCommandBuffers(m_device, commandPool, 1, &commandBuffer);
}

void VulkanManager::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
	VkBuffer& buffer, VkDeviceMemory& memory)
{
	// Create a buffer for CPU to store data in for the GPU to read -----
	VkBufferCreateInfo bufferInfo{};
	{
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		// Buffers can be owned by specific queue families or shared between multiple families.
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;	// We are using this buffer from the graphics queue.

		VK_ASSERT(vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer), "Failed to create vertex buffer");
	}

	// Figure out how much memory we need to load -----
	VkMemoryRequirements memoryRequirements;
	{
		vkGetBufferMemoryRequirements(m_device, buffer, &memoryRequirements);
	}

	// Memory allocation -----
	VkMemoryAllocateInfo allocInfo{};
	{
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memoryRequirements.size;
		// FINDING THE RIGHT MEMORY TYPE IS VERY VERY VERY IMPORTANT TO MAP BUFFERS.
		allocInfo.memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, properties);

		// Store handle to memory
		VK_ASSERT(vkAllocateMemory(m_device, &allocInfo, nullptr, &memory), "Failed to allocate vertex buffer memory");
	}

	// Bind the buffer memory -----
	// We're allocating specifically for this vertex buffer, no offset.
	// Offset has to be divisible by memoryRequirements.alignment otherwise.
	VK_ASSERT(vkBindBufferMemory(m_device, buffer, memory, 0), "Failed to bind vertex buffer");
}

// Copy from srcBuffer to dstBuffer.
void VulkanManager::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkCommandPool& commandPool, VkDeviceSize size)
{
	// Transfer operations are done through command buffers.
	// This can be a temporary command buffer or through the command pool.

	VkCommandBuffer commandBuffer = BeginSingleTimeCommands(commandPool);

	VkBufferCopy copyRegion{};
	{
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = size;
	}

	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	// Probably don't want this here, this should be handled by the caller.
	EndSingleTimeCommands(commandBuffer, commandPool);
}

void VulkanManager::CopyBufferToImage(VkCommandPool& commandPool, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands(commandPool);

	VkBufferImageCopy region{};
	{
		region.bufferOffset = 0;	// Offset where pixels start

		// How pixels are laid out in memory
		// Zero means pixels are tightly packed.
		// Nonzero means the pixels are padded.
		{
			region.bufferRowLength = 0;
			region.bufferImageHeight = 0;
		}

		// Which region of the image should be copied
		{
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.mipLevel = 0;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.layerCount = 1;
			region.imageOffset = { 0, 0, 0 };
			region.imageExtent = { width, height, 1 };
		}
	}

	vkCmdCopyBufferToImage(
		commandBuffer,
		buffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,	// Which layout the image is currently using
		1,
		&region);
	
	EndSingleTimeCommands(commandBuffer, commandPool);
}