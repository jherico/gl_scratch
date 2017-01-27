#pragma once

class GlWindow {
    static std::once_flag glfwInitFlag;
public:

    GlWindow() {
        std::call_once(glfwInitFlag, [] {
            glfwInit();
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
        });

        window = glfwCreateWindow(_size.x, _size.y, "Window Title", NULL, NULL);
        if (!window) {
            glfwTerminate();
            throw std::runtime_error("Could not create window");
        }
        glfwSetWindowUserPointer(window, this);
        glfwSetWindowCloseCallback(window, CloseHandler);
        glfwSetFramebufferSizeCallback(window, FramebufferSizeHandler);
        glfwShowWindow(window);

        glfwMakeContextCurrent(window);
        glewExperimental = true;
        glewInit();
        glDebugMessageCallback(DebugMessageCallback, this);
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
    }

    void move(const glm::ivec2& position) {
        glfwSetWindowPos(window, position.x, position.y);
    }

    void resize(const glm::uvec2& size) {
        glfwSetWindowSize(window, size.x, size.y);
    }

    virtual void run() {
        double start = glfwGetTime();
        double last = start;
        init();
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            double now = glfwGetTime();
            update(now - start, now - last);
            last = now;
            draw();
            glfwSwapBuffers(window);
        }
        glfwTerminate();
    }

protected:
    virtual void windowResize(const uvec2& size) {
        _size = size;
    }

    virtual void update(double time, double interval) = 0;
    virtual void draw() = 0;
    virtual void init() = 0;

protected:
    glm::uvec2 _size{ 800, 600 };
    GLFWwindow* window{ nullptr };

    static void GLAPIENTRY DebugMessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
        if (GL_DEBUG_SEVERITY_NOTIFICATION == severity) {
            return;
        }
        OutputDebugStringA(message);
        OutputDebugStringA("\n");
    }

    static void FramebufferSizeHandler(GLFWwindow* window, int width, int height) {
        GlWindow* example = (GlWindow*)glfwGetWindowUserPointer(window);
        example->windowResize(glm::uvec2(width, height));
    }

    static void CloseHandler(GLFWwindow* window) {
        GLFWwindow* example = (GLFWwindow*)glfwGetWindowUserPointer(window);
        glfwSetWindowShouldClose(window, 1);
    }
};


