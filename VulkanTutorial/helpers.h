#pragma once

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