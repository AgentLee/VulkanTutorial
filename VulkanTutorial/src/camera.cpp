#include "camera.h"
#include "helpers.h"
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtx/rotate_vector.hpp>

Camera::Camera(glm::vec3 eye, glm::vec3 target, glm::vec3 up, float fov, float aspect, float near, float far) :
	m_eye(eye), m_target(target), m_up(up), m_fov(glm::radians(fov)), m_aspect(aspect), m_near(near), m_far(far)
{
	UpdateForward();
	
	m_viewMatrix = glm::lookAt(m_eye, m_target, m_up);
	m_projMatrix = glm::perspective(m_fov, m_aspect, m_near, m_far);
	m_projMatrix[1][1] *= -1.0f;
}

void Camera::Translate(Direction d)
{
	switch(d)
	{
	case Direction::Backward:
		m_eye += m_speed * m_forward;
		m_target += m_speed * m_forward;
		break;
	case Direction::Forward:
		m_eye -= m_speed * m_forward;
		m_target -= m_speed * m_forward;
		break;
	case Direction::Right:
		m_eye += m_speed * Right();
		m_target += m_speed * Right();
		break;
	case Direction::Left:
		m_eye -= m_speed * Right();
		m_target -= m_speed * Right();
		break;
	case Direction::Up:
		m_eye += m_speed * m_up;
		m_target += m_speed * m_up;
		break;
	case Direction::Down:
		m_eye -= m_speed * m_up;
		m_target -= m_speed * m_up;
		break;
	}

	UpdateForward();
}

void Camera::Translate(glm::vec3 d)
{
	m_eye += d;
	UpdateForward();
}

void Camera::Rotate(glm::vec3 axis, float angle)
{
	m_eye = glm::rotate(m_eye, angle, axis);
}

void Camera::Update()
{
	m_viewMatrix = glm::lookAt(m_eye, m_target, m_up);
}
