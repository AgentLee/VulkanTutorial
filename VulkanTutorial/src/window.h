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
	// need to somehow lock the mouse to 0
	float lastX = 400;
	float lastY = 300;
	float yaw = 0;
	float pitch = 0;

	double clickPosX = 0;
	double clickPosY = 0;

	double upPosX = 0;
	double upPosY = 0;
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
		if (!g_mouseEvent.enteredWindow)
		{
			return;
		}

		if (!g_mouseEvent.mouseDown)
		{
			g_mouseEvent.lastX = xpos;
			g_mouseEvent.lastY = ypos;
			return;
		}
		
		if (g_mouseEvent.started)
		{
			g_mouseEvent.lastX = xpos;
			g_mouseEvent.lastY = ypos;
			g_mouseEvent.started = false;
		}

		// not necessarily lastx but the position where we clicked.
		
		float xOffset = xpos - g_mouseEvent.lastX;
		float yOffset = g_mouseEvent.lastY - ypos;

		std::cout << xOffset << ", " << yOffset << std::endl;
		
		g_mouseEvent.lastX = xpos;
		g_mouseEvent.lastY = ypos;

		const float sensitivity = 0.001f;
		xOffset *= sensitivity;
		yOffset *= sensitivity;

		g_mouseEvent.yaw += yOffset;
		g_mouseEvent.pitch += xOffset;

		//if (g_mouseEvent.pitch > 89.0f)
		//	g_mouseEvent.pitch = 89.0f;
		//if (g_mouseEvent.pitch < -89.0f)
		//	g_mouseEvent.pitch = -89.0f;

		glm::vec3 direction;
		direction.x = cos(glm::radians(g_mouseEvent.yaw)) * cos(glm::radians(g_mouseEvent.pitch));
		direction.y = sin(glm::radians(g_mouseEvent.pitch));
		direction.z = sin(glm::radians(g_mouseEvent.yaw)) * cos(glm::radians(g_mouseEvent.pitch));

		//static_cast<HelloTriangle*>(glfwGetWindowUserPointer(window))->GetCamera().SetTarget(glm::normalize(direction));
		static_cast<HelloTriangle*>(glfwGetWindowUserPointer(window))->GetCamera().Rotate(glm::vec3(0, 0, 1), xOffset);
		static_cast<HelloTriangle*>(glfwGetWindowUserPointer(window))->GetCamera().Rotate(glm::vec3(1,0, 0), yOffset);
	}

	static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
	{
		if (button == GLFW_MOUSE_BUTTON_LEFT)
		{
			if (action == GLFW_PRESS)
			{
				g_mouseEvent.mouseDown = true;

				glfwGetCursorPos(window, &g_mouseEvent.clickPosX, &g_mouseEvent.clickPosY);
				//std::cout << "Left mouse down " << g_mouseEvent.clickPosX << ", " << g_mouseEvent.clickPosY << std::endl;
			}
			else if (action == GLFW_RELEASE)
			{
				g_mouseEvent.mouseDown = false;

				glfwGetCursorPos(window, &g_mouseEvent.upPosX, &g_mouseEvent.upPosY);
				//std::cout << "Left mouse up " << g_mouseEvent.upPosX << ", " << g_mouseEvent.upPosY << std::endl;
			}
		}
	}

	static void MouseEnterCallback(GLFWwindow* window, int entered)
	{
		g_mouseEvent.enteredWindow = entered;

		//if (entered)
		//{
		//	std::cout << "Entered window" << std::endl;
		//}
		//else
		//{
		//	std::cout << "Exited window" << std::endl;
		//}
	}
};

class Window
{
public:
	Window()
	{
		//// Initialize GLFW library
		//glfwInit();

		//// Disable OpenGL
		//glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		//// Create window
		//// A cool thing here is that you can specify which monitor to open the window on.
		//m_window = glfwCreateWindow(WIDTH, HEIGHT, "Hello Triangle", nullptr, nullptr);

		//// Set pointer to window
		//glfwSetWindowUserPointer(m_window, this);

		//// Detect resizing
		//glfwSetFramebufferSizeCallback(m_window, FrameBufferResizeCallback);

		//// Set callbacks
		//glfwSetKeyCallback(m_window, KeyCallback);
		//glfwSetCursorPosCallback(m_window, MouseCallback);
		//glfwSetMouseButtonCallback(m_window, MouseButtonCallback);
		//glfwSetCursorEnterCallback(m_window, MouseEnterCallback);
	}
	
	~Window()
	{
	}

	//static void FrameBufferResizeCallback(GLFWwindow* window, int width, int height)
	//{
	//	auto app = reinterpret_cast<HelloTriangle*>(glfwGetWindowUserPointer(window));
	//	app->ToggleResize(true);
	//}

	GLFWwindow* m_window;
};
