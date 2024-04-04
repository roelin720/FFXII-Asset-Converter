#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>

void Camera::rotate(float d_theta, float d_phi) 
{
	theta += copysignf(1.0f, y_up) * d_theta;
	phi += d_phi;

	phi -= phi > glm::pi<float>() * 2.0f ? glm::pi<float>() * 2.0f : 0.0f;
	phi += phi < -glm::pi<float>() * 2.0f ? glm::pi<float>() * 2.0f : 0.0f;

	y_up = phi > 0.0f && phi < glm::pi<float>() || phi < -glm::pi<float>() && phi > -glm::pi<float>() * 2.0f ? 1.0f : -1.0f;
}

void Camera::zoom(float distance) 
{
    radius = std::max(0.00001f, radius - distance);
}

void Camera::pan(float dx, float dy)
{
    glm::vec3 pos = normalize(get_pos() - target);
    glm::vec3 up = glm::vec3(0.0f, y_up, 0.0f);
    glm::vec3 right = glm::cross(pos, up);
    up = glm::cross(pos, right);

    target += (right * dx) + (up * dy);
}

glm::vec3 Camera::get_pos()
{
    return target + radius * glm::vec3(sinf(phi) * sinf(theta), cosf(phi), sinf(phi) * cosf(theta));
}