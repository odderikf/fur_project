#pragma once

#include "scenegraph.hpp"

void updateNodeTransformations(SceneNode* node, glm::mat4 transformationThusFar);
void initialize_game(GLFWwindow* window);
void updateFrame(GLFWwindow* window);
void renderFrame(GLFWwindow* window);