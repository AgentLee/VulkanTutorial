#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>

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
	// Need to set up the handle to the debug callback.
	void SetupDebugMessenger();
	bool CheckValidationLayerSupport();
	// Return required list of extensions based on the enabled validation layers.
	std::vector<const char*> GetRequiredExtensions();
	
	VkInstance m_instance;
	VkDebugUtilsMessengerEXT m_debugMessenger;
	
	GLFWwindow* m_window;
};