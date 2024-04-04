#pragma once
#include "glad/glad.h"
#include "glm/glm.hpp"
#undef near
#undef far

struct AABB
{
	glm::vec3 min;
	glm::vec3 max;
};

class Camera
{
public:
	float theta;
	float phi;
	float radius;

	float near = 0.0f;
	float far = 0.0f;
	float y_up = 1.0f;

	glm::vec3 target;

	void rotate(float dTheta, float dPhi);
	void zoom(float distance);
	void pan(float dx, float dy);

	glm::vec3 get_pos();
};