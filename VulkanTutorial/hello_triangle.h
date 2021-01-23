#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>

#include "constants.h"

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
	bool CheckValidationLayerSupport();
	
	VkInstance m_instance;

	GLFWwindow* m_window;
};