#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include <array>

class Vertex
{
public:
	glm::vec3 position;
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

	static std::array<struct VkVertexInputAttributeDescription, 3> GetAttributeDescriptions()
	{
		// Describes how to handle vertex data.
		std::array<VkVertexInputAttributeDescription, 3> desc{};
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
		}

		return desc;
	}

	bool operator==(const Vertex& other) const
	{
		return position == other.position && color == other.color && uv == other.uv;
	}
};