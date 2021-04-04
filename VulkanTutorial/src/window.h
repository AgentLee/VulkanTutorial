#pragma once

#include <GLFW/glfw3.h>

class Window
{
public:
	Window()
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
		//glfwSetFramebufferSizeCallback(m_window, FrameBufferResizeCallback);
	}
	
	~Window()
	{
		glfwDestroyWindow(m_window);
		glfwTerminate();
	}

	GLFWwindow* m_window;
};
