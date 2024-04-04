#pragma once
#include "Scene.h"
#include "Texture2D.h"

class DrawableTexture2D
{
public:
	uint32_t ID = 0;
	uint32_t width = 0;
	uint32_t height = 0;

	~DrawableTexture2D();
	void free();

	bool assign(const Texture2D& texture);
};

class DrawableMaterial
{
public:
	DrawableTexture2D diffuse;
};

class DrawableSubmesh
{
public:
	struct Vertex
	{
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec2 tex_coord;
	};
	std::vector<Vertex> vertices;
	std::vector<uint16_t> indices;

	int32_t material_index = 0;
	uint32_t vao = 0;
	uint32_t vbo = 0;
	uint32_t ebo = 0;
	
	~DrawableSubmesh();
	void free();

	bool assign(const Model& model, const SubMesh& submesh);
};

class DrawableScene
{
public:
	std::vector<DrawableMaterial*> materials;
	std::vector<DrawableSubmesh*> submeshes;

	glm::vec3 center;
	float radius;

	~DrawableScene();
	void free();

	bool load(const std::string& path);
};