#pragma once

#include "vertex.h"
#include <vector>

class Mesh
{
public:
	Mesh() {}
	~Mesh() = default;

	void LoadModel(const char* path);

	std::vector<Vertex> m_vertices;
	std::vector<uint32_t> m_indices;
};
