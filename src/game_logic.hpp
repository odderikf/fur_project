#pragma once

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include "shader.hpp"

void update(GLFWwindow *window);
void render(GLFWwindow *window);
void initialize_game(GLFWwindow* window);

// globals
extern glm::mat4 VP;
extern Shader lighting_shader;