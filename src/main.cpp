//
//  Created by Bradley Austin Davis on 2016/07/01
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "Common.h"
#include "GlWindow.h"
#include "GlShaders.h"

static const size_t MAX_TEXTURES = 64;
static const double LOG_2 = log(2.0);

#define SPARSE 1

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


const char * VERTEX_SHADER = R"SHADER(
#version 450 core

layout(location = 0) uniform mat4 projection = mat4(1.0);
layout(location = 4) uniform mat4 modelView = mat4(1.0);

out vec2 varTexCoord0;

void main(void) {
    const vec4 UNIT_QUAD[4] = vec4[4](
        vec4(-1.0, -1.0, 0.0, 1.0),
        vec4(1.0, -1.0, 0.0, 1.0),
        vec4(-1.0, 1.0, 0.0, 1.0),
        vec4(1.0, 1.0, 0.0, 1.0)
    );
    vec4 pos = UNIT_QUAD[gl_VertexID];
    //gl_Position = projection * modelView * pos;
    gl_Position = pos;
    varTexCoord0 = (pos.xy + 1) * 0.5;
}
)SHADER";

const char * FRAGMENT_SHADER = R"SHADER(
#version 450 core
#extension GL_ARB_bindless_texture : require

in vec2 varTexCoord0;
out vec4 outFragColor;

layout(binding = 0) uniform Material
{
	uvec2 diffuse;
} material;

void main(void) {
    sampler2DArray sampler = sampler2DArray(material.diffuse);
    vec3 texCoord0 = vec3(varTexCoord0, 0);
    float mipmapLevel = textureQueryLod(sampler, texCoord0.xy).x;
    outFragColor = textureLod(sampler, texCoord0, mipmapLevel);
}

)SHADER";

using TextureTypeFormat = std::pair<GLenum, GLenum>;

static std::vector<uvec3> getPageDimensionsForFormat(const TextureTypeFormat& typeFormat) {
    GLint count = 0;
    glGetInternalformativ(typeFormat.first, typeFormat.second, GL_NUM_VIRTUAL_PAGE_SIZES_ARB, 1, &count);

    std::vector<uvec3> result;
    if (count > 0) {
        std::vector<GLint> x, y, z;
        x.resize(count);
        glGetInternalformativ(typeFormat.first, typeFormat.second, GL_VIRTUAL_PAGE_SIZE_X_ARB, 1, &x[0]);
        y.resize(count);
        glGetInternalformativ(typeFormat.first, typeFormat.second, GL_VIRTUAL_PAGE_SIZE_Y_ARB, 1, &y[0]);
        z.resize(count);
        glGetInternalformativ(typeFormat.first, typeFormat.second, GL_VIRTUAL_PAGE_SIZE_Z_ARB, 1, &z[0]);

        result.resize(count);
        for (GLint i = 0; i < count; ++i) {
            result[i] = uvec3(x[i], y[i], z[i]);
        }
    }

    return result;
}

static std::vector<uvec3> getPageDimensionsForFormat(GLenum target, GLenum format) {
    return getPageDimensionsForFormat({ target, format });
}

struct MaterialDescriptor {
    uvec4 handleAndMipRange;
    vec4 uvScale;


};

class TestWindow : public GlWindow {
protected:

    void init() override {
        image = std::make_shared<gli::texture2d>(gli::flip(gli::texture2d(gli::load(PROJECT_ROOT"/test.dds"))));
        imageExtent = image->extent();
        gli::gl GL(gli::gl::PROFILE_GL33);
        imageFormat = GL.translate(image->format(), image->swizzles());

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
        glCreateBuffers(1, &_materialBuffer);
        glNamedBufferStorage(_materialBuffer, sizeof(GLuint64), nullptr, GL_DYNAMIC_STORAGE_BIT);
        glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &_textureArray);
#if SPARSE
        glTextureParameteri(_textureArray, GL_VIRTUAL_PAGE_SIZE_INDEX_ARB, 0);
        glTextureParameteri(_textureArray, GL_TEXTURE_SPARSE_ARB, GL_TRUE);
#endif
        glTextureParameteri(_textureArray, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(_textureArray, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        auto pageSizes = getPageDimensionsForFormat(GL_TEXTURE_2D_ARRAY, imageFormat.Internal);

        auto pageSize = pageSizes[0];
        auto arraySize = uvec3({ 4096, 4096, 2 });
        auto arrayMips = evalNumMips(arraySize);
        glTextureStorage3D(_textureArray, arrayMips, GL_RGBA8, arraySize.x, arraySize.y, arraySize.z);
        glGetTextureParameterIuiv(_textureArray, GL_NUM_SPARSE_LEVELS_ARB, &_maxSparseLevel);
        _textureHandle = glGetTextureHandleARB(_textureArray);
        glMakeTextureHandleResidentARB(_textureHandle);

        // Load up the image into the first layer
        uint16_t mips = (uint16_t)image->levels();
        for (uint16_t mip = 0; mip < mips; ++mip) {
            auto extent = image->extent(mip);
#if SPARSE
            if (extent.x < pageSize.x || extent.y < pageSize.y) {
                break;
            }
            if (mip <= _maxSparseLevel) {
                glTexturePageCommitmentEXT(_textureArray, mip, 0, 0, 0, extent.x, extent.y, 1, GL_TRUE);
            }
#endif
            glTextureSubImage3D(_textureArray, mip, 0, 0, 0, extent.x, extent.y, 1, imageFormat.External, imageFormat.Type, image->data(0, 0, mip));
        }
        glGenerateTextureMipmap(_textureArray);
    }

    void update(double time, double interval) override {
    }

    void draw() override {
        glClearColor(1, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glViewport(10, 10, _size.x - 20, _size.y - 20);
        glNamedBufferSubData(_materialBuffer, 0, sizeof(GLuint64), &_textureHandle);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, _materialBuffer);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    

private:
    GLuint _textureArray { 0 };
    GLuint64 _textureHandle { 0 };
    uvec3 _textureArraySize;
    std::unordered_map<GLuint, GLuint64> _textureHandles;
    double _textureTimer = -1;
    size_t _textureCount { 0 };
    GLuint _materialBuffer { 0 };
    GLuint _currentTexture { 0 };
    GLuint _program { 0 };
    GLuint _vao { 0 };
    GLuint _maxSparseLevel { 0 };

    std::shared_ptr<gli::texture2d> image;
    gli::texture2d::extent_type imageExtent;
    gli::gl::format imageFormat;
};

int main(int argc, char** argv) {
    TestWindow().run();
    return 0;
}
