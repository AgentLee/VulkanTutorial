#pragma once

#include <GLFW/glfw3.h>

class HelloTriangle;

//https://stackoverflow.com/questions/36579771/glfw-key-callback-synchronization
//https://www.reddit.com/r/opengl/comments/bn6gbv/glfw_keyboard_input_handling/
//https://stackoverflow.com/questions/7676971/pointing-to-a-function-that-is-a-class-member-glfw-setkeycallback
//https://discourse.glfw.org/t/passing-parameters-to-callbacks/848/3
//https://www.glfw.org/docs/3.3/input_guide.html

struct MouseEvent
{
	bool enteredWindow = false;
	bool started = true;
	bool mouseDown = false;
	float lastX = 400;
	float lastY = 300;
};

struct KeyEvent
{
	KeyEvent(int key, int code, int action, int modifiers)
	{
		this->key = key;
		this->code = code;
		this->action = action;
		this->modifiers = modifiers;
	}
	
	int key, code, action, modifiers;
	std::chrono::steady_clock::time_point eventTime;
};

MouseEvent g_mouseEvent;
std::map<int, bool> g_keys;
std::queue<KeyEvent> g_unhandledKeys;

class InputHandler
{
public:
	static void Update(HelloTriangle* app)
	{
		float now = (float)glfwGetTime();
		static float lastUpdate = now;
		float deltaTime = now - lastUpdate;
		lastUpdate = now;
		
		//Anything that should happen "when the users presses the key" should happen here
		while (!g_unhandledKeys.empty())
		{
			KeyEvent event = g_unhandledKeys.front();
			g_unhandledKeys.pop();
			g_keys[event.key] = event.action == GLFW_PRESS || event.action == GLFW_REPEAT;
		}
		
		//Anything that should happen "while the key is held down" should happen here.
		if (g_keys[GLFW_KEY_W])
			app->GetCamera().Translate(Direction::Forward);
		if (g_keys[GLFW_KEY_A])
			app->GetCamera().Translate(Direction::Left);
		if (g_keys[GLFW_KEY_S])
			app->GetCamera().Translate(Direction::Backward);
		if (g_keys[GLFW_KEY_D])
			app->GetCamera().Translate(Direction::Right);
		if (g_keys[GLFW_KEY_C])
			app->GetCamera().Translate(Direction::Down);
		if (g_keys[GLFW_KEY_SPACE])
			app->GetCamera().Translate(Direction::Up);
	}

	static void HandleKey(GLFWwindow* window, int key, int code, int actions, int modifiers)
	{
		g_unhandledKeys.emplace(key, code, actions, modifiers);
	}

	static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		HandleKey(window, key, scancode, action, mods);
	}

	static void MouseCallback(GLFWwindow* window, double xpos, double ypos)
	{
		auto& app = *static_cast<HelloTriangle*>(glfwGetWindowUserPointer(window));

		// Don't do anything if the mouse is hovering over the ImGui window or if the mouse is outside the window.
		if (app.GetImGui().m_active || !g_mouseEvent.enteredWindow)
		{
			return;
		}

		// If the mouse is up, save the position but don't move the camera.
		// This will make sure the camera will continue to rotate correctly
		// when the mouse isn't down otherwise the camera will almost "reset" 
		if (!g_mouseEvent.mouseDown)
		{
			g_mouseEvent.lastX = xpos;
			g_mouseEvent.lastY = ypos;
			return;
		}

		// If it's the first time the mouse is being used, set the last positions to the current position
		if (g_mouseEvent.started)
		{
			g_mouseEvent.lastX = xpos;
			g_mouseEvent.lastY = ypos;
			g_mouseEvent.started = false;
		}

		float xOffset = xpos - g_mouseEvent.lastX;
		float yOffset = g_mouseEvent.lastY - ypos;

		const float sensitivity = 0.001f;
		xOffset *= sensitivity;
		yOffset *= sensitivity;

		// Rotate camera angle-axis since it's easier :)
		app.GetCamera().Rotate(glm::vec3(0, 0, 1), xOffset);
		app.GetCamera().Rotate(glm::vec3(1,0,0), yOffset);

		// Capture the position for the next move
		g_mouseEvent.lastX = xpos;
		g_mouseEvent.lastY = ypos;
	}

	static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
	{
		if (button == GLFW_MOUSE_BUTTON_LEFT)
		{
			if (action == GLFW_PRESS)
			{
				g_mouseEvent.mouseDown = true;
			}
			else if (action == GLFW_RELEASE)
			{
				g_mouseEvent.mouseDown = false;
			}
		}
	}

	static void MouseEnterCallback(GLFWwindow* window, int entered)
	{
		g_mouseEvent.enteredWindow = entered;
	}
};