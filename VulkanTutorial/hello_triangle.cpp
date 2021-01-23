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
