//
//  Created by Bradley Austin Davis on 2016/07/01
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <sstream>
#include <mutex>

#include <GL/glew.h>

#include <glm/gtx/component_wise.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

// Bring the most commonly used GLM types into the default namespace
using glm::ivec2;
using glm::ivec3;
using glm::ivec4;
using glm::uvec2;
using glm::uvec3;
using glm::uvec4;
using glm::mat3;
using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::quat;

#include <gli/gli.hpp>
#include <gli/convert.hpp>
#include <gli/generate_mipmaps.hpp>
#include <gli/load.hpp>
#include <gli/save.hpp>

#include <GLFW/glfw3.h>

#include <Windows.h>

static const size_t MAX_TEXTURES = 50;


static const double LOG_2 = log(2.0);

struct GLMem {
    GLint dedicatedMemory;
    GLint availableMemory;
    GLint currentAvailableVidMem;
    GLint evictionCount;
    GLint evictedMemory;

    void update() {
#define GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX          0x9047
#define GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX    0x9048
#define GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX  0x9049
#define GPU_MEMORY_INFO_EVICTION_COUNT_NVX            0x904A
#define GPU_MEMORY_INFO_EVICTED_MEMORY_NVX            0x904B
        glGetIntegerv(GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &dedicatedMemory);
        glGetIntegerv(GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &availableMemory);
        glGetIntegerv(GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &currentAvailableVidMem);
        glGetIntegerv(GPU_MEMORY_INFO_EVICTION_COUNT_NVX, &evictionCount);
        glGetIntegerv(GPU_MEMORY_INFO_EVICTED_MEMORY_NVX, &evictedMemory);
    }

    void report() {
        static char buffer[8192];
        //sprintf(
        //    buffer,
        //    "Dedicated: %u\n"
        //    "Available: %u\n"
        //    "Current:   %u\n"
        //    "Eviction:  %u\n"
        //    "Evicted:   %u\n",
        //    dedicatedMemory, availableMemory, currentAvailableVidMem, evictionCount, evictedMemory);
        auto used = availableMemory - currentAvailableVidMem;
        sprintf(buffer, "Used: %u\n", used);
        OutputDebugString(buffer);
    }
};

            
uint16_t evalNumMips(const uvec3& size) {
    double dim = glm::compMax(size);
    double val = log(dim) / LOG_2;
    return 1 + (uint16_t)val;
}

glm::uvec3 evalMipDimensions(const uvec3& size, uint16_t mip) {
    auto result = size;
    result >>= mip;
    return glm::max(result, uvec3(1));
}

void GLAPIENTRY debugMessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
    if (GL_DEBUG_SEVERITY_NOTIFICATION == severity) {
        return;
    }
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
}

const char * VERTEX_SHADER = R"SHADER(
#version 450 core

out vec2 varTexCoord0;

void main(void) {
    const vec4 UNIT_QUAD[4] = vec4[4](
        vec4(-1.0, -1.0, 0.0, 1.0),
        vec4(1.0, -1.0, 0.0, 1.0),
        vec4(-1.0, 1.0, 0.0, 1.0),
        vec4(1.0, 1.0, 0.0, 1.0)
    );
    vec4 pos = UNIT_QUAD[gl_VertexID];
    gl_Position = pos;
    varTexCoord0 = (pos.xy + 1) * 0.5;
}
)SHADER";

const char * FRAGMENT_SHADER = R"SHADER(
#version 450 core

uniform sampler2D tex;

in vec2 varTexCoord0;
out vec4 outFragColor;

void main(void) {
    outFragColor = texture(tex, varTexCoord0);
}

)SHADER";


bool compileShader(GLenum shaderDomain, const std::string& shaderSource, GLuint &shaderObject) {
    if (shaderSource.empty()) {
        return false;
    }

    // Create the shader object
    GLuint glshader = glCreateShader(shaderDomain);
    if (!glshader) {
        return false;
    }

    // Assign the source
    const GLchar* srcstr[] = { shaderSource.c_str() };
    glShaderSource(glshader, 1, srcstr, NULL);

    // Compile !
    glCompileShader(glshader);

    // check if shader compiled
    GLint compiled = 0;
    glGetShaderiv(glshader, GL_COMPILE_STATUS, &compiled);

    // if compilation fails
    if (!compiled) {
        GLint infoLength = 0;
        glGetShaderiv(glshader, GL_INFO_LOG_LENGTH, &infoLength);
        char* temp = new char[infoLength];
        glGetShaderInfoLog(glshader, infoLength, NULL, temp);
        delete[] temp;
        glDeleteShader(glshader);
        return false;
    }
    shaderObject = glshader;
    return true;
}

GLuint compileProgram(const std::vector<GLuint>& glshaders) {
    // A brand new program:
    GLuint glprogram = glCreateProgram();
    if (!glprogram) {
        return 0;
    }
    for (auto so : glshaders) {
        glAttachShader(glprogram, so);
    }
    glLinkProgram(glprogram);
    GLint linked = 0;
    glGetProgramiv(glprogram, GL_LINK_STATUS, &linked);

    if (!linked) {
        GLint infoLength = 0;
        glGetProgramiv(glprogram, GL_INFO_LOG_LENGTH, &infoLength);
        char* temp = new char[infoLength];
        glGetProgramInfoLog(glprogram, infoLength, NULL, temp);
        delete[] temp;
        glDeleteProgram(glprogram);
        return 0;
    }
    return glprogram;
}


class GlWindow {
public:
    GlWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
        window = glfwCreateWindow(_size.x, _size.y, "Window Title", NULL, NULL);
        if (!window) {
            glfwTerminate();
            throw std::runtime_error("Could not create window");
        }
        glfwSetWindowUserPointer(window, this);
        glfwSetWindowCloseCallback(window, CloseHandler);
        glfwSetFramebufferSizeCallback(window, FramebufferSizeHandler);
        glfwSetWindowPos(window, -800, 0);
        glfwShowWindow(window);

        glfwMakeContextCurrent(window);
        glewExperimental = true;
        glewInit();
        glDebugMessageCallback(debugMessageCallback, NULL);
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
        static const char* TEST_MESSAGE = "Test Message";
        glDebugMessageInsert(GL_DEBUG_SOURCE_OTHER, GL_DEBUG_TYPE_ERROR, 1, GL_DEBUG_SEVERITY_HIGH, strlen(TEST_MESSAGE), TEST_MESSAGE);
        glCreateVertexArrays(1, &_vao);
        glBindVertexArray(_vao);
        {
            std::vector<GLuint> shaders;
            shaders.resize(2);
            compileShader(GL_VERTEX_SHADER, VERTEX_SHADER, shaders[0]);
            compileShader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER, shaders[1]);
            _program = compileProgram(shaders);
        }
        glUseProgram(_program);
    }

    virtual void run() {

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            update();
            draw();
            glfwSwapBuffers(window);
        }
        glfwTerminate();
    }

protected:

    virtual void windowResize(const uvec2& size) {
        _size = size;
    }

    virtual void update() {
        double now = glfwGetTime();
        double delta = now - _textureTimer;
        if (_textureTimer < 0 || delta > 0.1) {
            uploadTexture();
            _textureTimer = now;
        }
    }

    virtual void draw() {
        glClearColor(1, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glViewport(10, 10, _size.x - 20, _size.y - 20);
        if (_textures.size() && _textures.back() != _currentTexture) {
            static size_t textureIndex = 0;
            textureIndex = ++textureIndex % _textures.size();
            _currentTexture = _textures[textureIndex];
            glBindTexture(GL_TEXTURE_2D, _currentTexture);
        }

        if (_currentTexture) {
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
    }

    
    void uploadTexture() {
        static GLMem glmem;
        static std::once_flag once;
        static gli::texture2d image(gli::flip(gli::texture2d(gli::load(PROJECT_ROOT"/test.dds"))));
        static const auto extent = image.extent();
        static gli::gl GL(gli::gl::PROFILE_GL33);
        static gli::gl::format const format = GL.translate(image.format(), image.swizzles());
        static GLuint maxSparseLevel = 0;
        static GLuint minMip = 0;
        static bool finishedLoading = false;
        if (!finishedLoading && _textures.size() < MAX_TEXTURES) {
            static std::once_flag once;
            std::call_once(once, [] {
                glmem.update();
                glmem.report();
            });
            GLuint texture = 0;
            uint16_t mips = image.levels();
            glCreateTextures(GL_TEXTURE_2D, 1, &texture);
            _textures.push_back(texture);
            glTextureParameteri(texture, GL_TEXTURE_BASE_LEVEL, 0);
            glTextureParameteri(texture, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(image.levels() - 1));
            glTextureParameteri(texture, GL_TEXTURE_SPARSE_ARB, GL_TRUE);
            glTextureParameteri(texture, GL_VIRTUAL_PAGE_SIZE_INDEX_ARB, 0);
            glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTextureStorage2D(texture, mips, GL_RGBA8, extent.x, extent.y);
            if (!maxSparseLevel) {
                glGetTextureParameterIuiv(texture, GL_NUM_SPARSE_LEVELS_ARB, &maxSparseLevel);
            }
            for (uint16_t mip = 0; mip < mips; ++mip) {
                auto extent = image.extent(mip);
                if (mip <= maxSparseLevel) {
                    glTexturePageCommitmentEXT(texture, mip, 0, 0, 0, extent.x, extent.y, 1, GL_TRUE);
                }
                glTextureSubImage2D(texture, mip, 0, 0, extent.x, extent.y, format.External, format.Type, image.data(0, 0, mip));
                glGenerateTextureMipmap(texture);
            }
        } else {
            finishedLoading = true;
            static std::once_flag once;
            std::call_once(once, [] {
                OutputDebugString("Max \n");
                glmem.update();
                glmem.report();
            });
            static auto lastTickCount = GetTickCount();
            auto now = GetTickCount();
            auto elapsed = now - lastTickCount;
            if (elapsed >= 500) {
                if (minMip < maxSparseLevel) {
                    glFinish();
                    std::cout << "Reducing" << std::endl;
                    auto extent = image.extent(minMip);
                    std::cout << "Removing mip " << minMip << " with dimesions " << extent.x << " x " << extent.y << std::endl;
                    for (auto texture : _textures) {
                        glTextureParameteri(texture, GL_TEXTURE_BASE_LEVEL, minMip + 1);
                        glTexturePageCommitmentEXT(texture, minMip, 0, 0, 0, extent.x, extent.y, 1, GL_FALSE);
                    }
                    glFinish();
                    ++minMip;
                    lastTickCount = now;
                    glmem.update();
                    glmem.report();
                } else if (_textures.size() > 1) {
                //    OutputDebugString("Deleting \n");
                //    std::cout << "Deleting texture " <<
                //    glDeleteTextures(1, &(_textures.back()));
                //    _textures.pop_back();
                //    glmem.update();
                //    glmem.report();
                //} else {
                //    int i = 0;
                }
            }
        }
    }

private:
    std::vector<GLuint> _textures;
    glm::uvec2 _size{ 800, 600 };
    double _textureTimer = -1;
    size_t _textureCount { 0 };
    GLuint _currentTexture { 0 };
    GLuint _program { 0 };
    GLuint _vao { 0 };

    GLFWwindow* window { nullptr };
    static void FramebufferSizeHandler(GLFWwindow* window, int width, int height) {
        GlWindow* example = (GlWindow*)glfwGetWindowUserPointer(window);
        example->windowResize(glm::uvec2(width, height));
    }

    static void CloseHandler(GLFWwindow* window) {
        GLFWwindow* example = (GLFWwindow*)glfwGetWindowUserPointer(window);
        glfwSetWindowShouldClose(window, 1);
    }
};

int main(int argc, char** argv) {
    GlWindow().run();
    return 0;
}
