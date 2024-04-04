#include "Viewer.h"
#include "ConverterInterface.h"

Renderer* Viewer::renderer = nullptr;
GLFWwindow* Viewer::window = nullptr;
float Viewer::delta_time;

Viewer::~Viewer()
{
    free();
}

void Viewer::free()
{
    delete renderer;
    renderer = nullptr;
    glfwDestroyWindow(window);
    glfwTerminate();
    window = nullptr;

    CoUninitialize();
}

bool Viewer::init(const glm::uvec2& size)
{
    if ((CoInitializeEx(NULL, COINIT_MULTITHREADED)) < 0) //needed for DirectXTex
    {
        LOG(ERR) << "COINIT_MULTITHREADED failed " << std::endl;
        return false;
    }

    glfwSetErrorCallback([](int error, const char* description)
    {
        LOG(ERR) << "Glfw Error " << error << ": " << description << std::endl;
    });

    if (!glfwInit())
    {
        return false;
    }

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(size.x, size.y, "Converter Viewer", NULL, NULL);
    if (window == NULL)
    {
        return false;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGL())
    {
        LOG(ERR) << "Unable to initialize GLAD " << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return false;
    }

    if (glDebugMessageCallback)
    {
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback([](GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* user_param)
        {
            if (severity == GL_DEBUG_SEVERITY_HIGH)
            {
                LOG(ERR) << "OpenGL Error: " << message << std::endl;
                __debugbreak();
            }
        }, nullptr);
    }
    else
    {
        LOG(WARN) << "OpenGL debug output is not available or supported on this driver." << std::endl;
    }

    static glm::ivec2 m_mouseLastPos;

    static bool left_button_down = false;
    static bool right_button_down = false;
    static float lastX = 0;
    static float lastY = 0;
    static float deltaX = 0;
    static float deltaY = 0;

    glfwSetCursorPosCallback(window, [](GLFWwindow* window, double xpos, double ypos)
    {
        deltaX = xpos - lastX;
        deltaY = ypos - lastY;
        lastX = xpos;
        lastY = ypos;

        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        {
            float rotation_factor = 0.015f * delta_time;
            float dPhi = (float(deltaY) / 300);
            float dTheta = (float(deltaX) / 300);

            Renderer::camera.rotate(-dTheta, -dPhi);
        }
        else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
        {
            float pan_factor = 0.1f * Renderer::camera.radius * delta_time;
            float dx = float(deltaX) * pan_factor;
            float dy = float(deltaY) * pan_factor;

            Renderer::camera.pan(-dx, dy);
        }
    });

    glfwSetScrollCallback(window, [](GLFWwindow* window, double x_stroll, double y_scroll)
    {
        float scroll_factor = 2.0f * Renderer::camera.radius * delta_time;
        Renderer::camera.zoom((float)y_scroll * scroll_factor);
    });

    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        if (key == GLFW_KEY_R && action == GLFW_PRESS)
        {
            Renderer::reorient_camera();
        }
    });

    Viewer::renderer = new Renderer();

    if (!renderer->init(window))
    {
        LOG(ERR) << "Failed to initialise renderer" << std::endl;
        return false;
    }

    return true;
}

bool Viewer::load_scene(const std::string& scene_path)
{
    if (!renderer->scene.load(scene_path))
    {
        LOG(ERR) << "Failed to load scene from path \"" << scene_path << "\"" << std::endl;
        return false;
    }
    renderer->reorient_camera();

    return true;
}

void Viewer::run()
{
    glfwShowWindow(window);

    static double prev_time = glfwGetTime();

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        double curr_time = glfwGetTime();
        delta_time = curr_time - prev_time;
        prev_time = curr_time;

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);;

        renderer->render_scene_to_screen();

        glfwSwapBuffers(window);
    }
}
