#include "hello_triangle.h"

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

void HelloTriangle::MainLoop()
{
	while (!glfwWindowShouldClose(m_window))
	{
		glfwPollEvents();
	}
}

void HelloTriangle::Cleanup()
{
	if (g_enableValidationLayers)
	{
		DebugLayer::DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
	}
	
	vkDestroyInstance(m_instance, nullptr);

	glfwDestroyWindow(m_window);
	glfwTerminate();
}
