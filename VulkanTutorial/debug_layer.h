#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define VALIDATION_LEVEL_OFF	0
#define VALIDATION_LEVEL_ERROR	1
#define VALIDATION_LEVEL_ALL	2

#ifndef NDEBUG
#define VALIDATION_LEVEL	VALIDATION_LEVEL_ERROR
#else
#define VALIDATION_LEVEL	VALIDATION_LEVEL_OFF
#endif

class DebugLayer
{
public:
	static void CheckExtensions(const std::vector<const char*>& requiredExtensions, const std::vector<VkExtensionProperties>& availableExtensions)
	{
#if VALIDATION_LEVEL == VALIDATION_LEVEL_ALL
		std::cout << "Available extensions: " << std::endl;
		for (auto& extension : availableExtensions)
		{
			std::cout << "\t" << extension.extensionName << std::endl;
		}
#endif

		// Try to find all of the required extensions in the available extensions.
		for (auto& requiredExtension : requiredExtensions)
		{
			bool match = false;
			for (auto& availableExtension : availableExtensions)
			{
				if (strcmp(requiredExtension, availableExtension.extensionName) == 0)
				{
					match = true;
					break;
				}
			}

			ASSERT(match, "Required extension not available.");
		}

#if VALIDATION_LEVEL == VALIDATION_LEVEL_ALL
		std::cout << std::endl << "Required extensions found." << std::endl;
#endif
	}
	
	// @messageSeverity
	// @messageType describes what happened to cause the problem
	// @pCallbackData contains the message
	// @pUserData contains the pointer specified during the callback set up
	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
	{
		std::string msg = pCallbackData->pMessage;
		
#if VALIDATION_LEVEL == VALIDATION_LEVEL_ERROR	// Report only errors
		if (msg.find("Error") != std::string::npos || msg.find("error") != std::string::npos)
#endif
		{
			// Add a breakpoint here to see where the errors are coming from.
			std::cerr << "[VK Validation] " << msg.c_str() << std::endl;
		}

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