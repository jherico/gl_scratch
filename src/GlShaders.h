#pragma once

#include <string>
#include <vector>
#include <GL/glew.h>

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
    glShaderSource(glshader, 1, srcstr, nullptr);

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

