#pragma once

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>

class Renderer
{
public:
	Renderer();
	~Renderer();

	void Initialize();
	void Run();

	void InitializeWindow();
	void FrameBufferResizeCallback(GLFWwindow* window, int width, int height);
	
private:
	GLFWwindow* m_window;
	bool m_frameBufferResized = false;
};