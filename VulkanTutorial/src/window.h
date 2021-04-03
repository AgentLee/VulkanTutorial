#pragma once

#include <GLFW/glfw3.h>

class Camera;


class Window
{
public:
	Window()
	{
		//glfwSetKeyCallback(m_window, onKey);
	}

	~Window()
	{
		
	}

	void onKey(int key, int scancode, int actions, int mods)
	{
		
	}

	GLFWwindow* m_window;
	Camera* m_camera;
};
