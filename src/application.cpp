#include "application.h"

static void GlfwKeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_W && action == GLFW_PRESS && (mods & GLFW_MOD_SUPER))
        Application::Get().StopRunning();
}

static void GlfwErrorCallback(int code, const char *message)
{
    fprintf(stderr, "GLFW: %s\n", message);
}

bool Application::Run()
{
    glfwSetErrorCallback(GlfwErrorCallback);
    if (!glfwInit()) {
        fprintf(stderr, "ERROR: Failed to initialize GLFW,\n");
        return false;
    }

    int width = 1280;
    int height = 720;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_window = glfwCreateWindow(width, height, "Window title", nullptr, nullptr);
    if (!m_window) {
        fprintf(stderr, "ERROR: Failed to create window GLFW,\n");
        return false;
    }
    glfwSetKeyCallback(m_window, GlfwKeyCallback);

    if (!m_renderer.Init(m_window)) {
        return false;
    }

    double lastTime = -glfwGetTime();

    m_running = true;
    while (m_running) {
        glfwPollEvents();
        if (glfwWindowShouldClose(m_window))
            m_running = false;

        double now = glfwGetTime();
        double dt = now - lastTime;
        lastTime = now;
        if (!m_renderer.Render(m_camera, m_window, dt)) {
            return false;
        }

        char windowTitle[1024] = {};
        snprintf(windowTitle, sizeof(windowTitle), "%f ms | %f fps", dt, 1 / dt);
        glfwSetWindowTitle(m_window, windowTitle);
    }

    m_renderer.Shutdown();

    glfwDestroyWindow(m_window);
    glfwTerminate();
    return true;
}
