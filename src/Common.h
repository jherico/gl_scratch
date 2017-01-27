#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <sstream>
#include <unordered_map>
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

#include <Windows.h>

#include <GLFW/glfw3.h>

