#pragma once

#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#endif
#include <GLFW/glfw3.h>

#include <chrono>
#include <fstream>
#include <vector>

static std::vector<char> ReadFile(const std::string& filename)
{
	// Read from the end and read as a binary file.
	std::ifstream file(filename, std::ios::ate | std::ios::binary);
	if(!file.is_open())
	{
		std::string msg = "Failed to open file: " + filename;
		throw std::runtime_error(msg.c_str());
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);
	
	// Go back to the beginning.
	file.seekg(0);
	// Read all the bytes at once.
	file.read(buffer.data(), fileSize);
	// Close the file.
	file.close();

	if (buffer.size() != fileSize)
	{
		throw std::runtime_error("Couldn't read file");
	}

	return buffer;
}

static float GetCurrentTime()
{
	static auto startTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	return time;
}

//https://stackoverflow.com/questions/3767869/adding-message-to-assert
// TODO: Fix so that intellisense can still work filling out a function in the condition.
// Actually not a fan of the way this assert works.
#ifndef NDEBUG
#define ASSERT(condition, message) \
    do { \
        if (! (condition)) { \
            std::cerr << "Assertion `" #condition "` failed in " << __FILE__ \
                      << " line " << __LINE__ << ": " << message << std::endl; \
            _ASSERT(false); \
        } \
    } while (false)
#else
#   define ASSERT(condition, message) do { } while (false)
#endif

#ifndef NDEBUG
#define VK_ASSERT(condition, message) \
    do { \
        if (condition != VK_SUCCESS) { \
            std::cerr << "Assertion `" #condition "` failed in " << __FILE__ \
                      << " line " << __LINE__ << ": " << message << std::endl; \
            _ASSERT(false); \
		} \
    } while (false)
#else
#   define VK_ASSERT(condition, message) do { } while (false)
#endif

static std::vector<const char*> GetRequiredExtensions()
{
	// Interface with GLFW
	uint32_t glfwExtensionCount = 0;
	auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	//if (g_enableValidationLayers)
	{
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extensions;
}