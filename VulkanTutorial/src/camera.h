#pragma once

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
#include <glm/ext/matrix_transform.hpp>

#include "transform.h"

enum Direction
{
	Left = 0,
	Right = 1,
	Forward = 2,
	Backward = 3,
	Up = 4,
	Down = 5,
};

class Camera
{
public:
	Camera() = default;
	Camera(glm::vec3 eye, glm::vec3 target, glm::vec3 up, float fov, float aspect, float near, float far);
	~Camera() = default;

	void UpdateForward() { m_forward = glm::normalize(m_eye - m_target); }
	void Translate(Direction d);
	void Translate(glm::vec3 d);
	void Rotate(glm::vec3 axis, float angle);
	void Update();
	void SetTarget(glm::vec3 target)
	{
		m_target = target;
		UpdateForward();
	}

	glm::vec3 Eye() { return m_eye; }
	glm::vec3 Up() { return m_up; }
	glm::vec3 Right() { return glm::normalize(glm::cross(m_up, m_forward)); }
	
	glm::vec3 Target() { return m_target; }
	glm::mat4x4 View() { return m_viewMatrix; }
	glm::mat4x4 Projection() { return m_projMatrix; }
	
	float FOV() { return m_fov; }
	float Near() { return m_near; }
	float Far() { return m_far; }

	float m_fov;
	float m_aspect;
	float m_near;
	float m_far;

	float yaw, pitch;
	
	glm::vec3 m_eye;
	glm::vec3 m_up;
	glm::vec3 m_target;
	glm::vec3 m_forward;

	float m_yRotation = 0.0f;

	glm::mat4x4 m_viewMatrix;
	glm::mat4x4 m_projMatrix;

	Transform m_transform;

	float m_speed = 0.001f;
};