#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <optional>

#include "constants.h"
#include "debug_layer.h"

class HelloTriangle
{
public:
	void Run();
	
private:
	void InitWindow();
	void MainLoop();
	void Cleanup();

	void InitVulkan();
	
	void CreateInstance();

	void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	// Need to set up the handle to the debug callback.
	void SetupDebugMessenger();
	bool CheckValidationLayerSupport();
	// Return required list of extensions based on the enabled validation layers.
	std::vector<const char*> GetRequiredExtensions();

	// Look for graphics card to use.
	void PickPhysicalDevice();
	bool IsDeviceCompatible(VkPhysicalDevice device);
	int RateDeviceCompatibility(VkPhysicalDevice device);

	struct QueueFamilyIndices
	{
		std::optional<uint32_t> graphicsFamily;

		bool IsComplete() { return graphicsFamily.has_value(); }
	};

	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);

	void CreateLogicalDevice();
	
	VkInstance m_instance;
	VkDebugUtilsMessengerEXT m_debugMessenger;
	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	VkDevice m_device;
	VkQueue m_graphicsQueue;
	
	GLFWwindow* m_window;
};