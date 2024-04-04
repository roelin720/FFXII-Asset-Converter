#include "Renderer.h"
#include "ConverterInterface.h"
#include <glm/gtc/matrix_transform.hpp>

#undef near
#undef far

Camera          Renderer::camera;
DrawableScene   Renderer::scene;
Shader*         Renderer::model_shader = nullptr;
                
uint32_t        Renderer::multi_sampled_frame_buffer = 0;
uint32_t        Renderer::depth_render_buffer = 0;
uint32_t        Renderer::colour_render_buffer = 0;
uint32_t        Renderer::frame_buffer;
uint32_t        Renderer::colour_buffer = 0;
uint32_t        Renderer::dummy_texture;
glm::uvec2      Renderer::frame_buffer_size = glm::uvec2(0, 0);
                
uint32_t        Renderer::screen_vao = 0;
uint32_t        Renderer::screen_vbo = 0;
Shader*         Renderer::screen_shader = nullptr;

GLFWwindow*     Renderer::window = nullptr;

namespace
{
    constexpr char screen_vs[] = R"RAWSTR(#version 330 core

layout (location = 0) in vec2 in_pos;
layout (location = 1) in vec2 in_tex_coords;

out vec2 tex_coords;

void main()
{
    tex_coords = in_tex_coords;
    gl_Position = vec4(in_pos, 0.0, 1.0); 
} 
 
)RAWSTR";

    constexpr char screen_fs[] = R"RAWSTR(#version 330 core

out vec4 out_colour;

in vec2 tex_coords;

uniform sampler2D screen_tex;

void main()
{
    out_colour = texture(screen_tex, tex_coords);
}
 
)RAWSTR";

    constexpr char model_vs[] = R"RAWSTR(#version 330 core

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_tex_coords;

out vec3 position;
out vec3 normal;
out vec2 tex_coords;

uniform mat4 view;
uniform mat4 proj;

void main()
{

    position = in_pos;
    normal = in_normal;  
    tex_coords = in_tex_coords;
    
    gl_Position = proj * view * vec4(in_pos, 1.0);
}

)RAWSTR";

    constexpr char model_fs[] = R"RAWSTR(#version 330 core

out vec4 out_colour;
    
in vec3 position;
in vec3 normal;
in vec2 tex_coords;
    
uniform sampler2D diffuse_tex;     
uniform vec3 light_pos; 
uniform vec3 view_pos; 
uniform vec3 light_colour;

void main()
{

    vec4 diffuse_colour = texture(diffuse_tex, tex_coords);

    //if (diffuse_colour.a < 0.5)
    //{
    //    discard;
    //}

    float ambient_strength = 0.25;
    vec3 ambient = ambient_strength * light_colour;
  	
    vec3 norm = normalize(normal);
    vec3 light_dir = normalize(light_pos - position);
    float diff = max(dot(norm, light_dir), 0.0);
    vec3 diffuse = diff * light_colour;
    
    float specular_strength = 0.2;
    vec3 view_dir = normalize(view_pos - position);
    vec3 reflect_dir = reflect(-light_dir, norm);  
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32);
    vec3 specular = specular_strength * spec * light_colour;  
        
    diffuse_colour.rgb = (ambient + diffuse + specular) * diffuse_colour.rgb;
    out_colour = diffuse_colour;
} 

)RAWSTR";
}

Renderer::~Renderer()
{
    free();
}

void Renderer::free()
{
    scene.free();
    glDeleteVertexArrays(1, &screen_vao);
    glDeleteBuffers(1, &screen_vbo);
    glDeleteTextures(1, &colour_buffer);
    glDeleteTextures(1, &dummy_texture);
    glDeleteRenderbuffers(1, &depth_render_buffer);
    glDeleteRenderbuffers(1, &colour_render_buffer);
    glDeleteFramebuffers(1, &frame_buffer);
    glDeleteFramebuffers(1, &multi_sampled_frame_buffer);

    frame_buffer = 0;
    multi_sampled_frame_buffer = 0;
    depth_render_buffer = 0;
    colour_render_buffer = 0;
    colour_buffer = 0;
    dummy_texture = 0;
    frame_buffer_size = glm::uvec2(0, 0);
    screen_vao = 0;
    screen_vbo = 0;

    delete screen_shader;
    screen_shader = nullptr;

    delete model_shader;
    model_shader = nullptr;

    window = nullptr;
}

void Renderer::reorient_camera()
{
    camera.near = scene.radius / 1000.0f;
    camera.far = scene.radius * 10.0f;
    camera.theta = 0.0f;
    camera.phi = glm::pi<float>();
    camera.target = scene.center;
    camera.radius = scene.radius * 2.1f;
    camera.y_up = 1.0f;
}

bool Renderer::render_scene(const glm::uvec2& size)
{
    if (size.x == 0 || size.y == 0)
    {
        return false;
    }
    if (!model_shader->ID)
    {
        LOG(ERR) << "No shader has been loaded" << std::endl;
        return false;
    }
    if (scene.submeshes.empty())
    {
        return false;
    }

    if (size != frame_buffer_size)
    {
        glDeleteTextures(1, &colour_buffer);
        glDeleteRenderbuffers(1, &depth_render_buffer);
        glDeleteRenderbuffers(1, &colour_render_buffer);
        glDeleteFramebuffers(1, &multi_sampled_frame_buffer);
        glDeleteFramebuffers(1, &frame_buffer);

        frame_buffer = 0;
        multi_sampled_frame_buffer = 0;
        depth_render_buffer = 0;
        colour_render_buffer = 0;
        colour_buffer = 0;
        frame_buffer_size = size;

        glGenFramebuffers(1, &frame_buffer);
        glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer);

        glGenTextures(1, &colour_buffer);
        glBindTexture(GL_TEXTURE_2D, colour_buffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colour_buffer, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            LOG(ERR) << "framebuffer is incomplete" << std::endl;
            frame_buffer_size = glm::uvec2(0, 0);
            return false;
        }

        glGenFramebuffers(1, &multi_sampled_frame_buffer);
        glBindFramebuffer(GL_FRAMEBUFFER, multi_sampled_frame_buffer);

        glGenRenderbuffers(1, &depth_render_buffer);
        glBindRenderbuffer(GL_RENDERBUFFER, depth_render_buffer);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT, size.x, size.y);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_render_buffer); 
        
        glGenRenderbuffers(1, &colour_render_buffer);
        glBindRenderbuffer(GL_RENDERBUFFER, colour_render_buffer);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA8, size.x, size.y);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colour_render_buffer);
        
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            LOG(ERR) << "multisampled framebuffer is incomplete" << std::endl;
            frame_buffer_size = glm::uvec2(0, 0);
            return false;
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, multi_sampled_frame_buffer);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);

    glViewport(0, 0, size.x, size.y);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    model_shader->use();

    glm::mat4 view = glm::lookAt(camera.get_pos(), camera.target, glm::vec3(0.0f, camera.y_up, 0.0f));
    glm::mat4 proj = glm::perspectiveFov(0.25f * glm::pi<float>(), float(size.x), float(size.y), camera.near, camera.far);

    model_shader->set_mat4("view", view);
    model_shader->set_mat4("proj", proj);
    model_shader->set_vec3("light_pos", camera.get_pos());
    model_shader->set_vec3("view_pos", camera.get_pos());
    model_shader->set_vec3("light_colour", glm::vec3(1.0f));

    for (auto* submesh : scene.submeshes)
    {
        screen_shader->set_int("diffuse_tex", 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, submesh->material_index < 0 ? dummy_texture : scene.materials[submesh->material_index]->diffuse.ID);

        glBindVertexArray(submesh->vao);

        glDrawElements(GL_TRIANGLES, submesh->indices.size(), GL_UNSIGNED_SHORT, 0);
    }

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frame_buffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, multi_sampled_frame_buffer);
    glBlitFramebuffer(0, 0, size.x, size.y, 0, 0, size.x, size.y, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);

    return true;
}

void Renderer::render_scene_to_screen()
{
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    if (!render_scene(glm::uvec2(width, height)))
    {
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glViewport(0, 0, width, height);

    screen_shader->use();
    screen_shader->set_int("screen_tex", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colour_buffer);

    glBindVertexArray(screen_vao);

    glDrawArrays(GL_TRIANGLES, 0, 6);
}

bool Renderer::render_scene_to_file(const std::string& path, const glm::uvec2& size)
{
    if (!render_scene(size))
    {
        return false;
    }

    DirectX::ScratchImage image;

    if (image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, size.x, size.y, 1, 1) < 0)
    {
        LOG(ERR) << "Failed to initialise image" << std::endl;
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, colour_buffer);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.GetPixels());

    {
        DirectX::ScratchImage flipped_image;
        std::swap(image, flipped_image);
        if (DirectX::FlipRotate(*flipped_image.GetImage(0, 0, 0), DirectX::TEX_FR_FLIP_VERTICAL, image) < 0)
        {
            LOG(ERR) << "Failed to flip image" << std::endl;
            return false;
        }
    }

    if (DirectX::SaveToWICFile(
        *image.GetImage(0, 0, 0),
        DirectX::WIC_FLAGS_FORCE_SRGB,
        DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG),
        std::wstring(path.begin(), path.end()).c_str()
    ) < 0)
    {
        LOG(ERR) << "Failed to save " << path << std::endl;
        return false;
    }
}

bool Renderer::init(GLFWwindow* window)
{
    Renderer::window = window;

    float screen_vertices[] =
    {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &screen_vao);
    glBindVertexArray(screen_vao);

    glGenBuffers(1, &screen_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, screen_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(screen_vertices), &screen_vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);

    uint32_t dummy_colour = 0xFF00FFFF;
    glGenTextures(1, &dummy_texture);
    glBindTexture(GL_TEXTURE_2D, dummy_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &dummy_colour);
    glBindTexture(GL_TEXTURE_2D, 0);

    screen_shader = new Shader();
    if (!screen_shader->compile(screen_vs, screen_fs))
    {
        LOG(ERR) << "Failed to compile screen shader" << std::endl;
        return false;
    }

    model_shader = new Shader();
    if (!model_shader->compile(model_vs, model_fs))
    {
        LOG(ERR) << "Failed to compile scene shader" << std::endl;
        return false;
    }

    return true;
}
