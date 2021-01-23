#include "hello_triangle.h"

#include <map>

void HelloTriangle::Run()
{
	InitWindow();
	InitVulkan();
	MainLoop();
	Cleanup();
}

void HelloTriangle::InitWindow()
{
	// Initialize GLFW library
	glfwInit();

	// Disable OpenGL
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	// Disable resizing
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	// Create window
	// A cool thing here is that you can specify which monitor to open the window on.
	m_window = glfwCreateWindow(WIDTH, HEIGHT, "Hello Triangle", nullptr, nullptr);
}

void HelloTriangle::InitVulkan()
{
	CreateInstance();
	SetupDebugMessenger();
	PickPhysicalDevice();
	CreateLogicalDevice();
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
		appInfo.apiVersion = VK_API_VERSION_1_0;	// I have 1.2 installed, but stay consistent with tutorial.
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

			std::cout << "Available extensions: " << std::endl;
			for (auto& extension : availableExtensions)
			{
				std::cout << "\t" << extension.extensionName << std::endl;
			}

			// Try to find all of the required extensions in the available extensions.
			for (auto& requiredExtension : requiredExtensions)
			{
				bool match = false;
				for (auto& availableExtension : availableExtensions)
				{
					if(strcmp(requiredExtension, availableExtension.extensionName) == 0)
					{
						match = true;
						break;
					}
				}

				if(!match)
				{
					throw std::runtime_error("Required extension not available.");
				}
			}

			std::cout << std::endl << "Required extensions found." << std::endl;
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
	if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create Vulkan instance");
	}
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

	if (DebugLayer::CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to set up debug messenger");
	}
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
	return indices.IsComplete();
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

	// TODO: Extract all these into their own functions
	// Describes the number of queues for a single queue family.
	VkDeviceQueueCreateInfo queueCreateInfo{};
	float queuePriority = 1;
	{
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
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
		createInfo.pQueueCreateInfos = &queueCreateInfo;
		createInfo.queueCreateInfoCount = 1;
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = 0;

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

	if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create logical device");
	}

	// Assign handle to queue
	vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_graphicsQueue);
}

void HelloTriangle::MainLoop()
{
	while (!glfwWindowShouldClose(m_window))
	{
		glfwPollEvents();
	}
}

void HelloTriangle::Cleanup()
{
	// Device queues (graphics queue) are implicitly destroyed when the device is destroyed.
	vkDestroyDevice(m_device, nullptr);
	
	if (g_enableValidationLayers)
	{
		DebugLayer::DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
	}

	// Don't destroy physical device since it is destroyed implicitly when the instance is destroyed.
	vkDestroyInstance(m_instance, nullptr);

	glfwDestroyWindow(m_window);
	glfwTerminate();
}
