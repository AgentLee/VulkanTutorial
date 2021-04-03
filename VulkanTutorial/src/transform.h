#pragma once

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>

struct Transform
{
	glm::vec3 translation;
	glm::mat4 matrix;

	Transform()
	{
		matrix = glm::mat4(1);
		Translate(glm::vec3(0));
	}
	Transform(glm::vec3 t)
	{
		matrix = glm::mat4(1);
		Translate(t);
	}

	void Translate(glm::vec3 t)
	{
		translation = t;
		matrix = glm::translate(matrix, t);
	}

	void Rotate(glm::vec3 axis, float angle)
	{
		matrix = glm::rotate(matrix, angle, axis);
	}
};
