#include "renderer.h"

void Renderer::Initialize()
{
	InitializeWindow();
}

void Renderer::Run()
{
}

void Renderer::InitializeWindow()
{
	// Initialize GLFW library
	glfwInit();

	// Disable OpenGL
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	// Create window
	// A cool thing here is that you can specify which monitor to open the window on.
	m_window = glfwCreateWindow(800, 600, "Hello Triangle", nullptr, nullptr);

	// Set pointer to window
	glfwSetWindowUserPointer(m_window, this);

	// Detect resizing
	glfwSetFramebufferSizeCallback(m_window, FrameBufferResizeCallback);
}

void Renderer::FrameBufferResizeCallback(GLFWwindow* window, int width, int height)
{
	auto app = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
	app->m_frameBufferResized = true;
}