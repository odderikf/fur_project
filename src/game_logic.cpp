#include "game_logic.hpp"

#include <glm/gtx/transform.hpp>


#include "shader_uniform_defines.h"
#include "scenegraph.hpp"
#include "utils/mesh.hpp"
#include "window.hpp"


SceneNode rootNode;
GeometryNode groundNode;
PointLightNode lampNode;
DirLightNode sunNode;

glm::mat4 VP;
Shader lighting_shader;

void update(GLFWwindow *window) {

    // todo responsive width height
    glm::mat4 projection = glm::perspective(glm::radians(80.0f), float(DEFAULT_WINDOW_WIDTH) / float(DEFAULT_WINDOW_HEIGHT), 0.1f, 350.f);

    // todo redo this
    glm::vec3 cameraPosition = glm::vec3(0, 2, -20);
    glm::mat4 cameraTransform =
            glm::rotate(0.3f + 0.2f * float(-0.4*0.4), glm::vec3(1, 0, 0)) *
            glm::rotate(-0.25f, glm::vec3(0, 1, 0)) *
            glm::translate(-cameraPosition);

    VP = projection * cameraTransform;

    rootNode.updateTFs(glm::identity<glm::mat4>());
}

void render(GLFWwindow *window) {
    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glViewport(0, 0, windowWidth, windowHeight);
    rootNode.render();
}

void initialize_game(GLFWwindow* window){
    rootNode.children.push_back(&groundNode);
    rootNode.children.push_back(&lampNode);
    rootNode.children.push_back(&sunNode);

    lighting_shader.create();
    lighting_shader.attach("../res/shaders/simple.frag");
    lighting_shader.attach("../res/shaders/simple.vert");
    lighting_shader.activate();

    Mesh groundMesh("../res/models/ground.obj");
    groundNode.register_vao(groundMesh);

    groundNode.position = {-50, 0, -90};

}