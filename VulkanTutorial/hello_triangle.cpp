#include "hello_triangle.h"

#include <map>
#include <set>
#include <cstdint>
#include <algorithm>

void HelloTriangle::Run()
{
	InitWindow();
	InitVulkan();
	MainLoop();
	Cleanup();
}

void HelloTriangle::FrameBufferResizeCallback(GLFWwindow* window, int width, int height)
{
	auto app = reinterpret_cast<HelloTriangle*>(glfwGetWindowUserPointer(window));
	app->frameBufferResized = true;
}

void HelloTriangle::InitWindow()
{
	// Initialize GLFW library
	glfwInit();

	// Disable OpenGL
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	// Create window
	// A cool thing here is that you can specify which monitor to open the window on.
	m_window = glfwCreateWindow(WIDTH, HEIGHT, "Hello Triangle", nullptr, nullptr);

	// Set pointer to window
	glfwSetWindowUserPointer(m_window, this);
	
	// Detect resizing
	glfwSetFramebufferSizeCallback(m_window, FrameBufferResizeCallback);
}

void HelloTriangle::InitVulkan()
{
	CreateInstance();
	SetupDebugMessenger();
	CreateSurface();
	PickPhysicalDevice();
	CreateLogicalDevice();
	CreateSwapChain();
	CreateImageViews();
	CreateRenderPass();
	CreateGraphicsPipeline();
	CreateFrameBuffers();
	CreateCommandPool();
	CreateVertexBuffer();
	CreateCommandBuffers();
	CreateSyncObjects();
}

void HelloTriangle::CreateInstance()
{
	if (g_enableValidationLayers && !CheckValidationLayerSupport())
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
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}
	}
	
	// General pattern:
	// Pointer to creation info
	// Pointer to custom allocator callbacks (nullptr)
	// Pointer to variable that stores the object
	VK_ASSERT(vkCreateInstance(&createInfo, nullptr, &m_instance), "Failed to create Vulkan instance");
}

void HelloTriangle::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo = {};
	{
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
}

void HelloTriangle::SetupDebugMessenger()
{
	if (!g_enableValidationLayers)
		return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	PopulateDebugMessengerCreateInfo(createInfo);

	VK_ASSERT(DebugLayer::CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger), "Failed to set up debug messenger");
}

bool HelloTriangle::CheckValidationLayerSupport()
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	// Check to make sure the validation layers we want are available.
	for (auto layerName : g_validationLayers)
	{
		bool layerFound = false;

		for (auto& layerProperties : availableLayers)
		{
			if (strcmp(layerName, layerProperties.layerName) == 0)
			{
				layerFound = true;
				break;
			}
		}

		if (!layerFound)
			return false;
	}
	
	return true;
}

std::vector<const char*> HelloTriangle::GetRequiredExtensions()
{
	// Interface with GLFW
	uint32_t glfwExtensionCount = 0;
	auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (g_enableValidationLayers)
	{
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extensions;
}

void HelloTriangle::PickPhysicalDevice()
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
		int score = RateDeviceCompatibility(device);
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

bool HelloTriangle::IsDeviceCompatible(VkPhysicalDevice device)
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
	
	return indices.IsComplete() && extensionsSupported && swapChainUsable;
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

bool HelloTriangle::CheckDeviceExtensionSupport(VkPhysicalDevice device)
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

int HelloTriangle::RateDeviceCompatibility(VkPhysicalDevice device)
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

HelloTriangle::QueueFamilyIndices HelloTriangle::FindQueueFamilies(VkPhysicalDevice device)
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

void HelloTriangle::CreateLogicalDevice()
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
		// TODO
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

void HelloTriangle::CreateSurface()
{
	VK_ASSERT(glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface), "Failed to create window surface");
}

HelloTriangle::SwapChainSupportDetails HelloTriangle::QuerySwapChainSupport(VkPhysicalDevice device)
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

VkSurfaceFormatKHR HelloTriangle::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
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

VkPresentModeKHR HelloTriangle::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
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

VkExtent2D HelloTriangle::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
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

void HelloTriangle::CreateSwapChain()
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

void HelloTriangle::RecreateSwapChain()
{
	// Pause while the window is minimized.
	{
		int width = 0;
		int height = 0;
		glfwGetFramebufferSize(m_window, &width, &height);

		while (width == 0 || height == 0)
		{
			glfwGetFramebufferSize(m_window, &width, &height);
			glfwWaitEvents();
		}
	}
	
	vkDeviceWaitIdle(m_device);

	CleanupSwapChain();
	
	CreateSwapChain();
	CreateImageViews();
	CreateRenderPass();
	CreateGraphicsPipeline();
	CreateFrameBuffers();
	CreateCommandBuffers();
}

void HelloTriangle::CreateImageViews()
{
	// Describe how to use the image.
	
	m_swapChainImageViews.resize(m_swapChainImages.size());

	for (size_t i = 0; i < m_swapChainImages.size(); ++i)
	{
		VkImageViewCreateInfo createInfo{};
		{
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = m_swapChainImages[i];

			// How to interpret the image data
			{
				createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				createInfo.format = m_swapChainImageFormat;
			}

			// Swizzle color channels
			{
				createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			}

			// Describes what the purpose of the image is and how to access the data
			{
				createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				createInfo.subresourceRange.baseMipLevel = 0;
				createInfo.subresourceRange.levelCount = 1;
				createInfo.subresourceRange.baseArrayLayer = 0;
				createInfo.subresourceRange.layerCount = 1;
			}
		}

		VK_ASSERT(vkCreateImageView(m_device, &createInfo, nullptr, &m_swapChainImageViews[i]), "Failed to create image view");
	}
}

void HelloTriangle::CreateGraphicsPipeline()
{
	// Load shader code
	auto vsCode = ReadFile("shaders/vert.spv");
	auto fsCode = ReadFile("shaders/frag.spv");

	// Create modules
	auto vsModule = CreateShaderModule(vsCode);
	auto fsModule = CreateShaderModule(fsCode);

	// Create shader stages
	VkPipelineShaderStageCreateInfo vsStageInfo{};
	{
		vsStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vsStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vsStageInfo.module = vsModule;
		vsStageInfo.pName = "main";	// Specify entry point function
		//vsStageInfo.pSpecializationInfo	// Specify values for shader constants
	}

	VkPipelineShaderStageCreateInfo fsStageInfo{};
	{
		fsStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fsStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fsStageInfo.module = fsModule;
		fsStageInfo.pName = "main";	// Specify entry point function
		//fsStageInfo.pSpecializationInfo	// Specify values for shader constants
	}

	VkPipelineShaderStageCreateInfo shaderStages[] = { vsStageInfo, fsStageInfo };
	
	// Describe the vertex data being passed to the shader
	// Bindings - spacing between data and whether the data is per vertex or per instance
	auto bindingDescription = Vertex::GetBindingDescription();
	// Attribute descriptions - the type of data being passed in and how to load them
	auto attributeDescription = Vertex::GetAttributeDescriptions();
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	{
		// We have no vertex data to pass to the shader so we don't really need to do anything else here.
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescription.size());
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescription.data();
	}

	// Input assembly
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	{
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;
	}

	// Viewports
	// Describe the region of the frame buffer to render to
	VkViewport viewport{};
	{
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)m_swapChainExtent.width;
		viewport.height = (float)m_swapChainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 0.0f;
	}

	// Scissors
	// Which regions pixels will be stored.
	// The rasterizer will discard pixels that are outside the scissor.
	VkRect2D scissor{};
	{
		scissor.offset = { 0, 0 };
		scissor.extent = m_swapChainExtent;
	}
	
	// Create the viewport
	VkPipelineViewportStateCreateInfo viewportState{};
	{
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;
	}

	// Rasterizer
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	{
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;	// Setting to true results in geometry never going to the fragment shader
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;	// How fragments are generated for geometry
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;	// Vertex order to be considered front facing
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasConstantFactor = 0.0f;
		rasterizer.depthBiasClamp = 0.0f;
		rasterizer.depthBiasSlopeFactor = 0.0f;
	}

	// Multisampling (AA)
	// Combines results of the fragment shader from multiple polygons that write to the same pixel.
	// This only runs if *multiple* polygons are detected.
	VkPipelineMultisampleStateCreateInfo multisampling{};
	{
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.minSampleShading = 1.0f;
		multisampling.pSampleMask = nullptr;
		multisampling.alphaToCoverageEnable = VK_FALSE;
		multisampling.alphaToOneEnable = VK_FALSE;
	}

	// Depth testing
	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	{
		// TODO:
	}

	// Color blending
	// Describes how to blend results from the fragment shader of a pixel.
	// VkPipelineColorBlendAttachmentState for the attached frame buffer
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	{
		// The idea is:
		// if(blendEnable)
		// {
		//		finalColor = (srcColorBlendFactor * newColor) <blend op> (dstColorBlendFactor * oldColor);
		//		finalAlpha = (srcAlphaBlendFactor * newAlpha) <alpha blend op> (dstAlphaBlendFactor * oldAlpha)
		// }
		// else
		// {
		//		finalColor = newColor;	// just take the new color.
		// }
		// 
		// finalColor = finalColor & colorWriteMask; // determine which channels get written to the buffer
		
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	}
	
	// VkPipelineColorBlendStateCreateInfo for all frame buffers.
	VkPipelineColorBlendStateCreateInfo colorBlending{};
	{
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f;
		colorBlending.blendConstants[1] = 0.0f;
		colorBlending.blendConstants[2] = 0.0f;
		colorBlending.blendConstants[3] = 0.0f;
	}

	// Dynamic state
	// Some states can be changed without recreating the pipeline (viewport, blending)
	// We will need to specify these values at each draw call.
	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_LINE_WIDTH,
	};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	{
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = 2;
		dynamicState.pDynamicStates = dynamicStates;
	}

	// Describe the pipeline
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	{
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 0;
		pipelineLayoutInfo.pSetLayouts = nullptr;
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		pipelineLayoutInfo.pPushConstantRanges = nullptr;
	}

	VK_ASSERT(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout), "Failed to create pipeline layout");

	// Create graphics pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	{
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;

		// Reference all the structures describing the fixed-function stage.
		{
			pipelineInfo.pVertexInputState = &vertexInputInfo;
			pipelineInfo.pInputAssemblyState = &inputAssembly;
			pipelineInfo.pViewportState = &viewportState;
			pipelineInfo.pRasterizationState = &rasterizer;
			pipelineInfo.pMultisampleState = &multisampling;
			pipelineInfo.pColorBlendState = &colorBlending;
		}

		pipelineInfo.layout = m_pipelineLayout;

		// Reference render pass and the subpass of this graphics pipeline.
		{
			pipelineInfo.renderPass = m_renderPass;
			pipelineInfo.subpass = 0;
		}

		// Any render pass can be used with this pipeline as long as they are compatible with m_renderPass.
		// You can create a new pass by deriving from this pipeline.
		{
			pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
			//pipelineInfo.basePipelineIndex = -1;
		}
	}

	VK_ASSERT(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline), "Failed to create a graphics pipeline");
	
	vkDestroyShaderModule(m_device, vsModule, nullptr);
	vkDestroyShaderModule(m_device, fsModule, nullptr);
}

VkShaderModule HelloTriangle::CreateShaderModule(const std::vector<char>& code)
{
	// Wraps the code in a VkShaderModule object.

	VkShaderModuleCreateInfo createInfo{};
	{
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
	}

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule))
	{
		throw std::runtime_error("Failed to create shader module");
	}

	return shaderModule;
}

void HelloTriangle::CreateRenderPass()
{
	// Tell Vulkan about the frame buffer attachments used for rendering.
	// This includes how many color/depth buffers, how many samples for each, and blending.
	// We encapsulate that in a render pass object.

	VkAttachmentDescription colorAttachment{};
	{
		colorAttachment.format = m_swapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

		// What to do with the attachment data before/after rendering.
		// Color and depth
		{
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;	// Clear buffer for next frame.
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;	// Save for later since we want to see the triangle.
		}

		// Same as above except for the stencil data.
		{
			colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		}

		// Initial - Which layout the image will have before the pass begins.
		// Final - Specifies layout to automatically transition to when the render pass finishes.
		{
			colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;		// We don't care about the previous image since we clear it anyway.
			colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;	// The image should be ready to present in the swap chain.
		}
	}

	// Subpasses - passes that rely on the previous pass.
	VkAttachmentReference colorAttachmentRef{};
	{
		colorAttachmentRef.attachment = 0;	// Which attachment to reference by index.
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	// Which layout we want the attachment to have.
	}

	VkSubpassDescription subpass{};
	{
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		// Not too sure about this...
		// The index of the attachment in this array is directly referenced from the fragment shader with
		// the layout(location = 0) out vec4 outColor directive!
	}

	VkSubpassDependency dependency{};
	{
		// Use a dependency to make sure the render pass waits until the pipeline writes to the buffer.
		// Prevent the transition from happening until it's ready.

		// Dependency indices and subpass
		{
			dependency.srcSubpass = VK_SUBPASS_EXTERNAL;	// Implicit subpass before/after render pass.
			dependency.dstSubpass = 0;	// Our subpass
		}

		// Specify which operations to wait on and which stages they should occur
		// Wait for swap chain to finish reading from image before we can access it.
		{
			dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.srcAccessMask = 0;
		}

		{
			dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		}
	}

	// Create the render pass
	VkRenderPassCreateInfo renderPassInfo{};
	{
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;
	}

	VK_ASSERT(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass), "Failed to create render pass");
}

void HelloTriangle::CreateFrameBuffers()
{
	// Each FBO will need to reference each of the image view objects.

	m_swapChainFrameBuffers.resize(m_swapChainImageViews.size());

	// Iterate over all of the views to create a frame buffer for them.
	for (auto i = 0; i < m_swapChainImageViews.size(); ++i)
	{
		VkImageView attachments[] = {
			m_swapChainImageViews[i],
		};

		// Frame buffers can only be used with compatible render passes.
		// They roughly have the same attachments.
		
		VkFramebufferCreateInfo frameBufferInfo{};
		{
			frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			frameBufferInfo.renderPass = m_renderPass;
			frameBufferInfo.attachmentCount = 1;
			frameBufferInfo.pAttachments = attachments;
			frameBufferInfo.width = m_swapChainExtent.width;
			frameBufferInfo.height = m_swapChainExtent.height;
			frameBufferInfo.layers = 1;
		}

		VK_ASSERT(vkCreateFramebuffer(m_device, &frameBufferInfo, nullptr, &m_swapChainFrameBuffers[i]), "Failed to create frame buffer");
	}
}

void HelloTriangle::CreateCommandPool()
{
	auto queueFamilyIndices = FindQueueFamilies(m_physicalDevice);

	VkCommandPoolCreateInfo poolInfo{};
	{
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();	// We want to draw, use the graphics family
		poolInfo.flags = 0;
	}

	VK_ASSERT(vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool), "Failed to create command pool");
}

void HelloTriangle::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory)
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
void HelloTriangle::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	// Transfer operations are done through command buffers.
	// This can be a temporary command buffer or through the command pool.
	
	VkCommandBufferAllocateInfo allocInfo{};
	{
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = m_commandPool;
		allocInfo.commandBufferCount = 1;
	}

	VkCommandBuffer commandBuffer;
	VK_ASSERT(vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer), "Failed to allocate command buffer");

	// Start recording the command buffer
	VkCommandBufferBeginInfo beginInfo{};
	{
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;	// We're only using the command buffer once.
	}
	VK_ASSERT(vkBeginCommandBuffer(commandBuffer, &beginInfo), "Couldn't begin recording command buffer");

	VkBufferCopy copyRegion{};
	{
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = size;
	}
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	// Stop recording since we already recorded the copy command.
	vkEndCommandBuffer(commandBuffer);

	// Execute the command buffer
	VkSubmitInfo submitInfo{};
	{
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
	}
	vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);

	// Wait for the transfer to finish.
	vkQueueWaitIdle(m_graphicsQueue);
	
	vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);
}

void HelloTriangle::CreateVertexBuffer()
{
	auto bufferSize = sizeof(Vertex) * vertices.size();

	// Create a temporary buffer -----
	// Use the staging buffer to map and copy vertex data.
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	
	// VK_BUFFER_USAGE_TRANSFER_SRC_BIT - Use this buffer as the source in transferring memory.
	CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
	
	// Fill the vertex buffer -----
	void* data;
	{
		// Map buffer memory into CPU accessible memory.
		VK_ASSERT(vkMapMemory(m_device, stagingBufferMemory, 0, bufferSize, 0, &data), "Failed to map data to vertex buffer");
		
		// Copy data over
		memcpy(data, vertices.data(), (size_t)bufferSize);
		
		// Unmap
		vkUnmapMemory(m_device, stagingBufferMemory);
	}

	// VK_BUFFER_USAGE_TRANSFER_DST_BIT - Use this buffer as the destination in transferring memory.
	// VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT - The most optimal use of memory. We need to use a staging buffer for this since it's not directly accessible with CPU.
	// The vertex buffer is device local. This means that we can't map memory directly to it but we can copy data from another buffer over.
	CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_vertexBuffer, m_vertexBufferMemory);

	// Copy from stage to vertex buffer
	CopyBuffer(stagingBuffer, m_vertexBuffer, bufferSize);

	// Destroy staging buffer
	{
		vkDestroyBuffer(m_device, stagingBuffer, nullptr);
		vkFreeMemory(m_device, stagingBufferMemory, nullptr);
	}
}

uint32_t HelloTriangle::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	// Query the types of memory

	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memoryProperties);

	// Find the memory type that is suitable with the buffer and make sure it has the properties to do so.
	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
	{
		if(typeFilter & (1 << i) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	ASSERT(false, "Failed to find suitable memory type");

	return -1;
}

void HelloTriangle::CreateCommandBuffers()
{
	// We need to allocate command buffers to be able write commands.
	// Each image in the swap chain needs to have a command buffer written to.
	
	m_commandBuffers.resize(m_swapChainFrameBuffers.size());

	VkCommandBufferAllocateInfo allocInfo{};
	{
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = m_commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;	// Submit to queue for execution but cannot be called from other command buffers (secondary).
		allocInfo.commandBufferCount = (uint32_t)m_commandBuffers.size();

		VK_ASSERT(vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data()), "Failed to allocate command buffers");
	}

	// Starting command buffer recording
	for (auto i = 0; i < m_commandBuffers.size(); ++i)
	{
		// Describe how the command buffers are being used.
		VkCommandBufferBeginInfo beginInfo{};
		{
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = 0;	// How to use command buffer.
			beginInfo.pInheritanceInfo = nullptr;	// Only relevant for secondary command buffers - which state to inherit from primary buffer.
		}

		VK_ASSERT(vkBeginCommandBuffer(m_commandBuffers[i], &beginInfo), "Failed to begin recording command buffer");

		// Starting a render pass
		VkClearValue clearColor = { 0, 0, 0, 1 };
		VkRenderPassBeginInfo renderPassInfo{};
		{
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = m_renderPass;
			renderPassInfo.framebuffer = m_swapChainFrameBuffers[i];
			renderPassInfo.renderArea.offset = {0, 0};
			renderPassInfo.renderArea.extent = m_swapChainExtent;

			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearColor;
		}

		vkCmdBeginRenderPass(	m_commandBuffers[i], 
								&renderPassInfo, 
								VK_SUBPASS_CONTENTS_INLINE	// Details the render pass - It's a primary command buffer, everything will be sent in one go.
		);

		// Basic drawing
		vkCmdBindPipeline(	m_commandBuffers[i], 
							VK_PIPELINE_BIND_POINT_GRAPHICS,	// Graphics or compute pipeline	
							m_graphicsPipeline
		);

		// Bind vertex buffer
		VkBuffer vertexBuffers[] = { m_vertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(m_commandBuffers[i], 0, 1, vertexBuffers, offsets);

		vkCmdDraw(m_commandBuffers[i], 3, 1, 0, 0);

		// End render pass
		vkCmdEndRenderPass(m_commandBuffers[i]);

		VK_ASSERT(vkEndCommandBuffer(m_commandBuffers[i]), "Failed to record command buffer");
	}
}

void HelloTriangle::CreateSyncObjects()
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

void HelloTriangle::MainLoop()
{
	while (!glfwWindowShouldClose(m_window))
	{
		glfwPollEvents();
		DrawFrame();
	}

	vkDeviceWaitIdle(m_device);
}

void HelloTriangle::DrawFrame()
{
	// -Get image from swap chain
	// -Execute command buffer with the image as an attachment in the frame buffer
	// -Return the image to the swap chain to present

	// These events are asynchronous, we need to sync them up.
	// Semaphores vs fences:
	// -Fences used for syncing rendering
	// -Semaphores used for syncing operations across command queues

	vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
	
	uint32_t imageIndex;	// Store index of the image from the swap chain.
	auto acquireResult = vkAcquireNextImageKHR(m_device, m_swapChain, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

	// If swap chain is out of date or suboptimal, recreate it.
	if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || acquireResult == VK_SUBOPTIMAL_KHR)
	{
		frameBufferResized = false;
		RecreateSwapChain();
		return;
	}
	else if (acquireResult != VK_SUCCESS)
	{
		ASSERT(false, "Failed to acquire swap chain image");
	}
	
	// Check if previous frame is using this image (waiting on a fence)
	if (m_imagesInFlight[imageIndex] != VK_NULL_HANDLE)
	{
		vkWaitForFences(m_device, 1, &m_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
	}

	// Mark image as being used
	m_imagesInFlight[imageIndex] = m_inFlightFences[m_currentFrame];
	
	VkSemaphore waitSemaphores[] = { m_imageAvailableSemaphores[m_currentFrame] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSemaphore signalSemaphores[] = { m_renderFinishedSemaphores[m_currentFrame] };

	// Submit command buffer
	
	VkSubmitInfo submitInfo{};
	{
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		// Specify which semaphores to wait on before execution and which stage to wait on.
		// We want to wait until the image is available so when the graphics pipeline writes to the color attachment.
		// The wait semaphores should correspond to the wait stages array.
		{
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = waitSemaphores;
			submitInfo.pWaitDstStageMask = waitStages;
		}

		// Specify which command buffers to submit for execution.
		// We want to submit the buffer that binds the swap chain image we acquired as color attachment.
		{
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &m_commandBuffers[imageIndex];
		}

		// Specify which semaphores to signal after execution.
		{
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = signalSemaphores;
		}

		vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);
		
		VK_ASSERT(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]), "Failed to submit draw command buffer");
	}

	// Present
	// Submit result back to swap chain to show on screen.
	
	VkSwapchainKHR swapChains[] = { m_swapChain };
	VkPresentInfoKHR presentInfo{};
	{
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		// Specify which semaphores to wait on before presenting.
		{
			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = signalSemaphores;
		}

		// Specify swap chains to present images to and which image to present.
		{
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = swapChains;
			presentInfo.pImageIndices = &imageIndex;
		}

		// Specify array of results to check if the result was successful.
		presentInfo.pResults = nullptr;
	}
	
	vkQueuePresentKHR(m_presentQueue, &presentInfo);

	// Wait for work to finish after submission.
	vkQueueWaitIdle(m_presentQueue);

	m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void HelloTriangle::CleanupSwapChain()
{
	for (auto& framebuffer : m_swapChainFrameBuffers)
	{
		vkDestroyFramebuffer(m_device, framebuffer, nullptr);
	}

	vkFreeCommandBuffers(m_device, m_commandPool, static_cast<uint32_t>(m_commandBuffers.size()), m_commandBuffers.data());

	vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
	vkDestroyRenderPass(m_device, m_renderPass, nullptr);

	for (auto& imageView : m_swapChainImageViews)
	{
		vkDestroyImageView(m_device, imageView, nullptr);
	}

	vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
}

void HelloTriangle::Cleanup()
{
	CleanupSwapChain();

	vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
	vkFreeMemory(m_device, m_vertexBufferMemory, nullptr);
	
	for(auto i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
		vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
	}
	
	vkDestroyCommandPool(m_device, m_commandPool, nullptr);
	
	// Device queues (graphics queue) are implicitly destroyed when the device is destroyed.
	vkDestroyDevice(m_device, nullptr);
	
	if (g_enableValidationLayers)
	{
		DebugLayer::DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
	}

	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	
	// Don't destroy physical device since it is destroyed implicitly when the instance is destroyed.
	vkDestroyInstance(m_instance, nullptr);

	glfwDestroyWindow(m_window);
	glfwTerminate();
}
