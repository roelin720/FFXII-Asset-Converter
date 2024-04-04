#pragma once
#include "Camera.h"
#include "Drawable.h"
#include "Shader.h"
#include "GLFW/glfw3.h"

#undef near
#undef far

class Renderer
{
public:
	static Camera camera;
	static DrawableScene scene;
	static Shader* model_shader;

	static glm::uvec2 frame_buffer_size;
	static uint32_t multi_sampled_frame_buffer;
	static uint32_t depth_render_buffer;
	static uint32_t colour_render_buffer;
	static uint32_t frame_buffer;
	static uint32_t colour_buffer;
	static uint32_t dummy_texture;

	static uint32_t screen_vao;
	static uint32_t screen_vbo;
	static Shader* screen_shader;

	static GLFWwindow* window;

	~Renderer();
	static void free();

	static bool init(GLFWwindow* window);

	static void reorient_camera();

	static bool render_scene(const glm::uvec2& size);
	static void render_scene_to_screen();
	static bool render_scene_to_file(const std::string& path, const glm::uvec2& size);
};