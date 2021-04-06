#pragma once

#include <vulkan/vulkan.h>

#ifndef GLM_FORCE_RADIANS
#define GLM_FORCE_RADIANS
#endif
#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif
#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#ifndef GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#endif
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include <array>

struct UniformBufferObject
{
	alignas(16) glm::mat4 model;
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
};

class Vertex
{
public:
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 color;
	glm::vec2 uv;
	
	static VkVertexInputBindingDescription GetBindingDescription()
	{
		// Determine per-vertex or per-instance, how many bytes between data entries.
		VkVertexInputBindingDescription desc{};
		{
			desc.binding = 0;	// Index of the binding in the array.
			desc.stride = sizeof(Vertex);	// We're packing everything into one struct.
			desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		}

		return desc;
	}

	static std::array<struct VkVertexInputAttributeDescription, 4> GetAttributeDescriptions()
	{
		// Describes how to handle vertex data.
		std::array<VkVertexInputAttributeDescription, 4> desc{};
		{
			// Position
			{
				desc[0].binding = 0;							// Which binding the data is coming from
				desc[0].location = 0;							// Where to find in the shader
				desc[0].format = VK_FORMAT_R32G32B32_SFLOAT;	// Type of data
				desc[0].offset = offsetof(Vertex, position);	// Bytes between data
			}

			// Color
			{
				desc[1].binding = 0;							// Which binding the data is coming from
				desc[1].location = 1;							// Where to find in the shader
				desc[1].format = VK_FORMAT_R32G32B32_SFLOAT;	// Type of data
				desc[1].offset = offsetof(Vertex, color);		// Bytes between data
			}

			// UV
			{
				desc[2].binding = 0;							// Which binding the data is coming from
				desc[2].location = 2;							// Where to find in the shader
				desc[2].format = VK_FORMAT_R32G32_SFLOAT;		// Type of data
				desc[2].offset = offsetof(Vertex, uv);			// Bytes between data
			}

			// Normal
			{
				desc[3].binding = 0;
				desc[3].location = 3;
				desc[3].format = VK_FORMAT_R32G32B32_SFLOAT;
				desc[3].offset = offsetof(Vertex, normal);
			}
		}

		return desc;
	}

	bool operator==(const Vertex& other) const
	{
		return position == other.position && color == other.color && uv == other.uv;
	}
};

namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.position) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.uv) << 1);
		}
	};
}