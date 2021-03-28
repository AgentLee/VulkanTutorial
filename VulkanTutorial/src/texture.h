#pragma once

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#endif

class Texture
{
public:
	Texture(const char* path)
	{
		// Get image
		m_pixels = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);


		//if (!m_pixels)
		//{
		//	ASSERT(false, "Failed to load texture image");
		//}
	}

	void Free()
	{
		// We loaded everything into data so we can now clean up pixels.
		stbi_image_free(m_pixels);
	}

	int width, height, channels;
	stbi_uc* m_pixels;
};

class Mesh
{
public:
	Mesh(const char* path)
	{
		//using namespace tinyobj;
		//attrib_t attrib;
		//std::vector<shape_t> shapes;
		//std::vector<material_t> materials;
		//std::string warning, error;

		//if (!LoadObj(&attrib, &shapes, &materials, &warning, &error, path))
		//{
		//	throw std::runtime_error(warning + error);
		//}

		//std::unordered_map<Vertex, uint32_t> uniqueVertices{};

		//for (const auto& shape : shapes)
		//{
		//	for (const auto& index : shape.mesh.indices)
		//	{
		//		Vertex vertex{};
		//		{
		//			vertex.position = {
		//				attrib.vertices[3 * index.vertex_index + 0],
		//				attrib.vertices[3 * index.vertex_index + 1],
		//				attrib.vertices[3 * index.vertex_index + 2],
		//			};

		//			vertex.uv = {
		//				attrib.texcoords[2 * index.texcoord_index + 0],
		//				1 - attrib.texcoords[2 * index.texcoord_index + 1],	// TinyObj does some weird flipping.
		//			};

		//			vertex.color = { 1, 1, 1 };
		//		}

		//		if (uniqueVertices.count(vertex) == 0) {
		//			uniqueVertices[vertex] = static_cast<uint32_t>(m_vertices.size());
		//			m_vertices.push_back(vertex);
		//		}

		//		m_indices.push_back(uniqueVertices[vertex]);
		//	}
		//}
	}

	//std::vector<Vertex> m_vertices;
	//std::vector<uint32_t> m_indices;
};