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
}

void HelloTriangle::CreateInstance()
{
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

	
	VkInstanceCreateInfo createInfo{};
	{
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		// Interface with GLFW
		uint32_t glfwExtensionCount = 0;
		auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		
		createInfo.enabledExtensionCount = glfwExtensionCount;
		createInfo.ppEnabledExtensionNames = glfwExtensions;

		createInfo.enabledLayerCount = 0;
	}

	// General pattern:
	// Pointer to creation info
	// Pointer to custom allocator callbacks (nullptr)
	// Pointer to variable that stores the object
	if (vkCreateInstance(&createInfo, nullptr, &m_instance))
	{
		throw std::runtime_error("Failed to create Vulkan instance");
	}
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
	glfwDestroyWindow(m_window);
	glfwTerminate();
}
