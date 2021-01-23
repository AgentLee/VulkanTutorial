#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class DebugLayer
{
public:
	// @messageSeverity
	// @messageType describes what happened to cause the problem
	// @pCallbackData contains the message
	// @pUserData contains the pointer specified during the callback set up
	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
	{
		std::cerr << "[Validation Layer] " << pCallbackData->pMessage << std::endl;

		return VK_FALSE;
	}
	
	// Vulkan doesn't have a built in debug messenger function since it's an extension. We have to load it up ourselves.
	static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* pDebugMessenger)
	{
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

		// Check if the function was properly loaded.
		if (func != nullptr)
		{
			return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
		}
		else
		{
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
	{
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

		// Check if the function was properly loaded.
		if (func != nullptr)
		{
			func(instance, debugMessenger, pAllocator);
		}
	}
};