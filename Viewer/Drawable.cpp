#include "Drawable.h"
#include "ConverterInterface.h"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include <DirectXTex/DirectXTex.h>
#include <filesystem>

DrawableSubmesh::~DrawableSubmesh()
{
    free();
}

void DrawableSubmesh::free()
{
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    vao = 0;
    vbo = 0;
    ebo = 0;
}

bool DrawableSubmesh::assign(const Model& model, const SubMesh& submesh)
{
    material_index = submesh.material ? submesh.material->ID : -1;

	vertices.resize(submesh.components.begin()->second[0]->elem_count);

    glm::mat4 transform(1.0f);
    for (Node* node = model.node; node; node = node->parent)
    {
        transform = node->transform * transform;
    }
    glm::mat3 normal_transform = glm::mat3(glm::transpose(glm::inverse(transform)));

    for (auto& tc : submesh.components)
    {
        if (tc.second.empty())
        {
            continue;
        }
        auto& component = tc.second[0];

        if (tc.first == VertexComponentType::Vertex || tc.first == VertexComponentType::SkinnableVertex)
        {
            glm::vec3* positions = (glm::vec3*)component->data.data();
            for (uint64_t k = 0; k < component->elem_count; ++k)
            {
                vertices[k].position = glm::vec3(transform * glm::vec4(positions[k], 1.0f));
            }
        }
        if (tc.first == VertexComponentType::Normal || tc.first == VertexComponentType::SkinnableNormal)
        {
            glm::vec3* normals = (glm::vec3*)component->data.data();
            for (uint64_t k = 0; k < component->elem_count; ++k)
            {
                vertices[k].normal = normal_transform * normals[k];
            }
        }
        if (tc.first == VertexComponentType::ST)
        {
            glm::vec2* tex_coords = (glm::vec2*)component->data.data();

            for (uint64_t k = 0; k < component->elem_count; ++k)
            {
                vertices[k].tex_coord = tex_coords[k];
            }
        }
    }
    indices = submesh.indices;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tex_coord));
    glEnableVertexAttribArray(2);

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint16_t), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

	return true;
}

bool DrawableTexture2D::assign(const Texture2D& texture)
{
    DirectX::ScratchImage image;
    if (!texture.to_image(image))
    {
        LOG(ERR) << "Failed to convert texture to image" << std::endl;
        return false;
    }

    if (image.GetMetadata().format != DXGI_FORMAT_R8G8B8A8_UNORM)
    {
        DirectX::ScratchImage unformatted_image;
        std::swap(image, unformatted_image);
        if (HRESULT res = DirectX::Convert(*unformatted_image.GetImage(0, 0, 0), DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, image); res < 0)
        {
            LOG(ERR) << "Failed to convert image to DXGI_FORMAT_R8G8B8A8_UNORM" << std::endl;
            return false;
        }
    }

    width = texture.prefix.width;
    height = texture.prefix.height;

    glGenTextures(1, &ID);
    glBindTexture(GL_TEXTURE_2D, ID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.GetImage(0,0,0)->pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

DrawableTexture2D::~DrawableTexture2D()
{
    free();
}

void DrawableTexture2D::free()
{
    glDeleteTextures(1, &ID);
}

DrawableScene::~DrawableScene()
{
    free();
}

void DrawableScene::free()
{
    for (auto* material : materials)
    {
        delete material;
    }

    for (auto* submesh : submeshes)
    {
        delete submesh;
    }

    materials.clear();
    submeshes.clear();
}

bool DrawableScene::load(const std::string& path)
{
    Scene scene;
    if (!scene.unpack(path)) 
    {
        return false;
    }
    scene.evaluate_local_material_textures(path, path, "dds.phyre");

    materials.resize(scene.materials.size());

    for (size_t i = 0; i < scene.materials.size(); ++i)
    {
        if (!scene.materials[i].diffuse_texture_path.empty() && std::filesystem::exists(scene.materials[i].diffuse_texture_path))
        {
            Texture2D texture2D;
            if (!texture2D.unpack(scene.materials[i].diffuse_texture_path))
            {
                continue;
            }

            materials[i] = new DrawableMaterial();
            if (!materials[i]->diffuse.assign(texture2D))
            {
                materials[i]->diffuse.free();
                continue;
            }
        }
    }

    glm::vec3 min = glm::vec3(FLT_MAX, FLT_MAX, FLT_MAX);
    glm::vec3 max = glm::vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    submeshes.reserve(scene.submeshes.size());

    for (auto& model : scene.models)
    {
        for (size_t i = 0; i < model.mesh->submeshes.size(); ++i)
        {
            submeshes.push_back(new DrawableSubmesh());      
            if (!submeshes.back()->assign(model, *model.mesh->submeshes[i]))
            {
                delete submeshes.back();
                submeshes.pop_back();
            }
            for (auto& vertex : submeshes.back()->vertices)
            {
                min = glm::min(min, vertex.position);
                max = glm::max(max, vertex.position);
            }
        }
    }

    center = (min + max) * 0.5f;
    radius = glm::distance(center, min);
    radius = glm::max(radius, glm::distance(center, glm::vec3(max.x, min.y, min.z)));
    radius = glm::max(radius, glm::distance(center, glm::vec3(max.x, max.y, min.z)));
    radius = glm::max(radius, glm::distance(center, glm::vec3(min.x, max.y, min.z)));
    radius = glm::max(radius, glm::distance(center, glm::vec3(min.x, min.y, max.z)));
    radius = glm::max(radius, glm::distance(center, glm::vec3(max.x, min.y, max.z)));
    radius = glm::max(radius, glm::distance(center, glm::vec3(min.x, max.y, max.z)));
    radius = glm::max(radius, glm::distance(center, max));
 
    return true;
}
