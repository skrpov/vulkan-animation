#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdio.h>
#include <volk.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vk_enum_string_helper.h>

#include "renderer.h"

class Application
{
  public:
    static inline Application &Get()
    {
        static Application app;
        return app;
    }
    inline void StopRunning() { m_running = false; };
    bool Run();

  private:
    bool LoadScene(Scene &scene);

    Renderer m_renderer;
    GLFWwindow *m_window = nullptr;
    bool m_running = false;
    Scene m_scene;
};
