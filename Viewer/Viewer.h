#pragma once
#include "Renderer.h"
#include "GLFW/glfw3.h"

class Viewer
{
public:
	static Renderer* renderer;
	static GLFWwindow* window;
	static float delta_time;

	~Viewer();

	static bool init(const glm::uvec2& size);
	static bool load_scene(const std::string& scene_path);
	static void run();
	static void free();
};