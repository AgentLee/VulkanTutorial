#pragma once

#include <chrono>
#include <fstream>
#include <vector>

static std::vector<char> ReadFile(const std::string& filename)
{
	// Read from the end and read as a binary file.
	std::ifstream file(filename, std::ios::ate | std::ios::binary);
	if(!file.is_open())
	{
		throw std::runtime_error("Failed to open file");
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