#include <chrono>
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <iostream>
#include <fmt/format.h>

#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

#include "utilities/imageLoader.hpp"
#include "utilities/glfont.hpp"
#include "utilities/timeutils.h"
#include "utilities/shapes.hpp"
#include "utilities/glutils.hpp"
#include "utilities/shader.hpp"

#include "gamelogic.h"
#include "shader_uniform_defines.hpp"
#include "window.hpp"

double padPositionX = 0;
double padPositionZ = 0;

SceneNode* rootNode;
SceneNode* terrainNode;
SceneNode* boxNode;
SceneNode* rickyNode;
SceneNode* rickyFurNode;
SceneNode* padNode;
SceneNode* padLightNode;
SceneNode* topLeftLightNode;
SceneNode* topRightLightNode;
SceneNode* trio0LightNode;
SceneNode* trio1LightNode;
SceneNode* trio2LightNode;
SceneNode* whiteLightNode;
SceneNode* textNode;

double ballRadius = 3.0f;

// These are heap allocated, because they should not be initialised at the start of the program
Gloom::Shader* lighting_shader;
Gloom::Shader* flat_geometry_shader;
Gloom::Shader* fur_shader;
Gloom::Shader* skybox_shader;

const glm::vec3 boxDimensions(1, 1, 1);
const glm::vec3 padDimensions(30, 3, 40);

// global so it can be multiplied in to make MVP locally
glm::mat4 VP;

GLint uniform_light_sources_position_loc[UNIFORM_POINT_LIGHT_SOURCES_LEN];
GLint uniform_light_sources_color_loc[UNIFORM_POINT_LIGHT_SOURCES_LEN];

GLint fur_uniform_light_sources_position_loc[UNIFORM_POINT_LIGHT_SOURCES_LEN];
GLint fur_uniform_light_sources_color_loc[UNIFORM_POINT_LIGHT_SOURCES_LEN];

unsigned int turbulenceID;

bool hasStarted        = false;
bool isPaused          = false;

bool mouseLeftPressed   = false;
bool mouseLeftReleased  = false;
bool mouseRightPressed  = false;
bool mouseRightReleased = false;

// Modify if you want the music to start further on in the track. Measured in seconds.
const float debug_startTime = 0;
double realTime = debug_startTime;

double mouseSensitivity = 1.0;
double lastMouseX = DEFAULT_WINDOW_WIDTH / 2;
double lastMouseY = DEFAULT_WINDOW_HEIGHT / 2;

glm::vec3 cameraPosition = glm::vec3(0, 2, -20);
glm::vec3 cameraRotation = glm::vec3(0, 0, 0);

void mouseCallback(GLFWwindow* window, double x, double y) {
    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glViewport(0, 0, windowWidth, windowHeight);

    double deltaX = x - lastMouseX;
    double deltaY = y - lastMouseY;

    padPositionX -= mouseSensitivity * deltaX / windowWidth;
    padPositionZ -= mouseSensitivity * deltaY / windowHeight;

    if (padPositionX > 1) padPositionX = 1;
    if (padPositionX < 0) padPositionX = 0;
    if (padPositionZ > 1) padPositionZ = 1;
    if (padPositionZ < 0) padPositionZ = 0;

    glfwSetCursorPos(window, windowWidth / 2, windowHeight / 2);
}

GLuint create_cubemap(const std::string &foldername) {
    GLuint tex_id = 0;
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex_id);
    std::string s[6] = {"posx.png", "negx.png", "negy.png", "posy.png", "posz.png", "negz.png"};
    for (int i = 0; i < 6; ++i) {
        auto tex = loadPNGFile(foldername + s[i]);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, tex.width, tex.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex.pixels.data());
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return tex_id;
}

GLuint create_texture(const std::string &filename) {
    auto tex = loadPNGFile(filename);
    GLuint tex_id = 0;
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex.width, tex.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex.pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    return tex_id;
}

void initialize_game(GLFWwindow* window) {

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    glfwSetCursorPosCallback(window, mouseCallback);

    lighting_shader = new Gloom::Shader();
    lighting_shader->makeBasicShader("../res/shaders/simple.vert", "../res/shaders/simple.frag");
    lighting_shader->activate();

    flat_geometry_shader = new Gloom::Shader();
    flat_geometry_shader->makeBasicShader("../res/shaders/flat_geom.vert", "../res/shaders/flat_geom.frag");
    flat_geometry_shader->activate();

    fur_shader = new Gloom::Shader();
    fur_shader->attach("../res/shaders/fur.vert");
    fur_shader->attach("../res/shaders/fur.frag");
    fur_shader->attach("../res/shaders/fur.geom");
    fur_shader->link();
    fur_shader->activate();

    skybox_shader = new Gloom::Shader();
    skybox_shader->attach("../res/shaders/skybox.vert");
    skybox_shader->attach("../res/shaders/skybox.frag");
    skybox_shader->link();
    skybox_shader->activate();

    // create textures
    GLuint charmap_id = create_texture("../res/textures/charmap.png");

    GLuint paddle_normal_id    = create_texture("../res/textures/paddle_nrm.png");
    GLuint paddle_diffuse_id   = create_texture("../res/textures/paddle_col.png");
    GLuint paddle_roughness_id = create_texture("../res/textures/paddle_rgh.png");

    GLuint ricky_normal_id    = create_texture("../res/textures/ricky_nrm.png");
    GLuint ricky_fur_normal_id    = create_texture("../res/textures/ricky_fur_nrm.png");
    GLuint ricky_diffuse_id   = create_texture("../res/textures/ricky_col.png");
    GLuint ricky_roughness_id = create_texture("../res/textures/ricky_rgh.png");
    GLuint ricky_fur_texture_id = create_texture("../res/textures/ricky_fur.png");

    GLuint turbulence_id = create_texture("../res/textures/turbulence.png");

    GLuint skybox_id = create_cubemap("../res/textures/skybox/");

    // gen meshes
    Mesh pad = cube(padDimensions, glm::vec2(30, 40), true);
    Mesh box = cube(boxDimensions, glm::vec2(90), true, true);
    Mesh sphere = generateSphere(1.0, 40, 40);
    Mesh ricky("../res/models/ricky.obj");
    Mesh terrain("../res/models/terrain.obj");

    const float textwidth = 400;
    const float textratio = 1.34482759f;
    Mesh text_mesh = generateTextGeometryBuffer("Click to start", textratio, textwidth);


    // Fill buffers
    unsigned int boxVAO  = generateBuffer(box);
    unsigned int padVAO  = generateBuffer(pad);
    unsigned int textVAO = generateBuffer(text_mesh);
    unsigned int rickyVAO = generateBuffer(ricky);
    unsigned int terrainVAO = generateBuffer(terrain);

    // Construct scene
    rootNode = createSceneNode();
    boxNode  = createSceneNode();
    terrainNode = createSceneNode();
    padNode  = createSceneNode();
    rickyNode = createSceneNode();
    rickyFurNode = createSceneNode();
    padLightNode = createSceneNode();
    topLeftLightNode = createSceneNode();
    topRightLightNode = createSceneNode();
    trio0LightNode = createSceneNode();
    trio1LightNode = createSceneNode();
    trio2LightNode = createSceneNode();
    whiteLightNode = createSceneNode();
    textNode = createSceneNode();

//    rootNode->children.push_back(terrainNode);
    rootNode->children.push_back(padNode);
    rootNode->children.push_back(boxNode);
    rootNode->children.push_back(rickyNode);
    rickyNode->children.push_back(rickyFurNode);

    // transparency nodes
    rootNode->children.push_back(textNode);

    int which_lights = 0;

    // these three groups of light are mutually incompatible
    if (which_lights == 0){
        // three colored lights, in two corners and on the pad
        rootNode->children.push_back(topLeftLightNode);
        rootNode->children.push_back(topRightLightNode);
        padNode->children.push_back(padLightNode);
    } else if (which_lights == 1){
        // single white light behind scene
        rootNode->children.push_back(whiteLightNode);
    } else {
        // three overlapping rotating lights
        rootNode->children.push_back(trio0LightNode);
        rootNode->children.push_back(trio1LightNode);
        rootNode->children.push_back(trio2LightNode);
    }


    terrainNode->vertexArrayObjectID = terrainVAO;
    terrainNode->VAOIndexCount = terrain.indices.size();
    boxNode->vertexArrayObjectID  = boxVAO;
    boxNode->VAOIndexCount        = box.indices.size();

    padNode->vertexArrayObjectID  = padVAO;
    padNode->VAOIndexCount        = pad.indices.size();

    textNode->vertexArrayObjectID = textVAO;
    textNode->VAOIndexCount       = text_mesh.indices.size();

    rickyNode->vertexArrayObjectID = rickyVAO;
    rickyNode->VAOIndexCount       = ricky.indices.size();
    rickyFurNode->vertexArrayObjectID = rickyVAO;
    rickyFurNode->VAOIndexCount       = ricky.indices.size();

    topLeftLightNode->nodeType = POINT_LIGHT;
    topRightLightNode->nodeType = POINT_LIGHT;
    padLightNode->nodeType = POINT_LIGHT;

    trio0LightNode->nodeType = POINT_LIGHT;
    trio1LightNode->nodeType = POINT_LIGHT;
    trio2LightNode->nodeType = POINT_LIGHT;

    whiteLightNode->nodeType = POINT_LIGHT;

    textNode->nodeType = FLAT_GEOMETRY;

    boxNode->nodeType = SKYBOX;
    padNode->nodeType = NORMAL_MAPPED_GEOMETRY;
    rickyNode->nodeType = NORMAL_MAPPED_GEOMETRY;
    rickyFurNode->nodeType = FUR_GEOMETRY;

    topLeftLightNode->position = { -85, 30, -120};
    topRightLightNode->position = { 85, 30, -120};
    padLightNode->position = {0,5,13};
    whiteLightNode->position = {0,20,-20};

    textNode->position = { DEFAULT_WINDOW_WIDTH/2 - textwidth/2, DEFAULT_WINDOW_HEIGHT/2, 0};

    rickyNode->position = {-50, 20, -90};

    boxNode->position = { 0, 0, 0 };

    // texture IDs
    textNode->textureID = charmap_id;
    padNode->normalMapID = paddle_normal_id;
    padNode->textureID = paddle_diffuse_id;
    padNode->roughnessID = paddle_roughness_id;
    rickyNode->normalMapID = ricky_normal_id;
    rickyNode->textureID = ricky_diffuse_id;
    rickyNode->roughnessID = ricky_roughness_id;
    rickyFurNode->furID = ricky_fur_texture_id;
    rickyFurNode->normalMapID = ricky_fur_normal_id;
    rickyFurNode->textureID = ricky_diffuse_id;
    rickyFurNode->roughnessID = ricky_roughness_id;
    turbulenceID = turbulence_id;
    boxNode->textureID = skybox_id;

    // uniform array index IDs
    topLeftLightNode->lightID = 0;
    topRightLightNode->lightID = 1;
    padLightNode->lightID = 2;

    whiteLightNode->lightID = 0;

    trio0LightNode->lightID = 0;
    trio1LightNode->lightID = 1;
    trio2LightNode->lightID = 2;

    // one red, one green, one blue. These are also intensities, not normalized.
    topLeftLightNode->lightColor = {1., 0., 0.};
    topRightLightNode->lightColor = {0., 1., 0.};
    padLightNode->lightColor = {0., 0., 1.};

    whiteLightNode->lightColor = {0.9, 0.9, 0.9};

    trio0LightNode->lightColor = {1., 0., 0.};
    trio1LightNode->lightColor = {0., 1., 0.};
    trio2LightNode->lightColor = {0., 0., 1.};


    // find uniform locations
    for (auto node : {topLeftLightNode, topRightLightNode, padLightNode}) {
        std::string poslocname = fmt::format("{}[{}].{}", UNIFORM_POINT_LIGHT_SOURCES_NAME, node->lightID,
                                             UNIFORM_POINT_LIGHT_SOURCES_POSITION_NAME);
        uniform_light_sources_position_loc[node->lightID] = lighting_shader->getUniformFromName(poslocname);
        std::string collocname = fmt::format("{}[{}].{}", UNIFORM_POINT_LIGHT_SOURCES_NAME, node->lightID,
                                             UNIFORM_POINT_LIGHT_SOURCES_COLOR_NAME);
        uniform_light_sources_color_loc[node->lightID] = lighting_shader->getUniformFromName(collocname);
    }
    // do it again for the fur shader
    for (auto node : {topLeftLightNode, topRightLightNode, padLightNode}) {
        std::string poslocname = fmt::format("{}[{}].{}", UNIFORM_POINT_LIGHT_SOURCES_NAME, node->lightID,
                                             UNIFORM_POINT_LIGHT_SOURCES_POSITION_NAME);
        fur_uniform_light_sources_position_loc[node->lightID] = fur_shader->getUniformFromName(poslocname);
        std::string collocname = fmt::format("{}[{}].{}", UNIFORM_POINT_LIGHT_SOURCES_NAME, node->lightID,
                                             UNIFORM_POINT_LIGHT_SOURCES_COLOR_NAME);
        fur_uniform_light_sources_color_loc[node->lightID] = fur_shader->getUniformFromName(collocname);
    }

    getTimeDeltaSeconds();

    std::cout << fmt::format("Initialized scene with {} SceneNodes.", totalChildren(rootNode)) << std::endl;

    std::cout << "Ready. Click to start!" << std::endl;
    cameraPosition = glm::vec3(0, 2, -20);
    cameraRotation = glm::vec3(0, 0, 0);
}

void updateFrame(GLFWwindow* window) {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    double timeDelta = getTimeDeltaSeconds();

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1)) {
        mouseLeftPressed = true;
        mouseLeftReleased = false;
    } else {
        mouseLeftReleased = mouseLeftPressed;
        mouseLeftPressed = false;
    }
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_2)) {
        mouseRightPressed = true;
        mouseRightReleased = false;
    } else {
        mouseRightReleased = mouseRightPressed;
        mouseRightPressed = false;
    }

    glm::vec3 camera_position_delta = glm::vec3(0,0,0);
    glm::vec3 camera_rotation_delta = glm::vec3(0,0,0);
    float camera_move_speed = 10;
    float camera_rotation_speed = 1;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    {
        camera_position_delta.z += camera_move_speed * timeDelta;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    {
        camera_position_delta.z -= camera_move_speed * timeDelta;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    {
        camera_position_delta.x += camera_move_speed * timeDelta;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    {
        camera_position_delta.x -= camera_move_speed * timeDelta;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
    {
        camera_position_delta.y += camera_move_speed * timeDelta;
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
    {
        camera_position_delta.y -= camera_move_speed * timeDelta;
    }
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
    {
        camera_rotation_delta.y += camera_rotation_speed * timeDelta;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
    {
        camera_rotation_delta.y -= camera_rotation_speed * timeDelta;
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
    {
        camera_rotation_delta.x += camera_rotation_speed * timeDelta;
    }
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
    {
        camera_rotation_delta.x -= camera_rotation_speed * timeDelta;
    }

    realTime += timeDelta;

    glm::mat4 projection = glm::perspective(
        glm::radians(80.0f),
        float(DEFAULT_WINDOW_WIDTH) / float(DEFAULT_WINDOW_HEIGHT), //todo dynamic aspect
        0.1f,
        350.f
    );

    cameraRotation += camera_rotation_delta;
    if(cameraRotation.x > glm::radians(90.f)){
        cameraRotation.x = glm::radians(90.f);
    } else if (cameraRotation.x < glm::radians(-90.f)){
        cameraRotation.x = glm::radians(-90.f);
    }

    glm::mat4 rotation = glm::rotate(cameraRotation.z, glm::vec3(0,0,1));
    rotation = glm::rotate(rotation, cameraRotation.x, glm::vec3(1,0,0));
    rotation = glm::rotate(rotation, cameraRotation.y, glm::vec3(0,1,0));

    VP = projection * rotation;

    glm::mat4 cam_rot_mat = glm::rotate(-cameraRotation.y, glm::vec3(0,1,0));
    glm::vec4 camera_position_delta4 = cam_rot_mat * glm::vec4(camera_position_delta, 1);
    camera_position_delta = glm::vec3(camera_position_delta4);
    cameraPosition += camera_position_delta;
    VP = glm::translate(VP, cameraPosition);

    // Move and rotate various SceneNodes
    rickyNode->rotation = {0, 0.1*realTime, 0.01*realTime};

    glm::vec3 previous_boxDimensions = {180, 90, 90};
    glm::vec3 previous_boxPosition = { 0, -10, -80 };;
    padNode->position  = {
            previous_boxPosition.x - (previous_boxDimensions.x/2) + (padDimensions.x/2) + (1 - padPositionX) * (previous_boxDimensions.x - padDimensions.x),
            previous_boxPosition.y - (previous_boxDimensions.y/2) + (padDimensions.y/2),
            previous_boxPosition.z - (previous_boxDimensions.z/2) + (padDimensions.z/2) + (1 - padPositionZ) * (previous_boxDimensions.z - padDimensions.z)
    };

    // overlapping lights alternate code:
    glm::vec3 lightpos = {0, -53, -80};
    glm::vec4 rot = {0, 1.73205081, 0, 1};
    lightpos.z -= 4;
    trio0LightNode->position = trio1LightNode->position = trio2LightNode->position = lightpos;
    glm::mat4 rotlight = glm::rotate(float(realTime), glm::vec3(0,0,1));
    trio0LightNode->position += glm::vec3(rotlight * rot);
    rotlight = glm::rotate(float(realTime + 2*glm::pi<float>()/3.), glm::vec3(0,0,1));
    trio1LightNode->position += glm::vec3(rotlight * rot);
    rotlight = glm::rotate(float(realTime + 2*2*glm::pi<float>()/3.), glm::vec3(0,0,1));
    trio2LightNode->position += glm::vec3(rotlight * rot);

    updateNodeTransformations(rootNode, glm::identity<glm::mat4>());
    lighting_shader->activate();
    glUniform3fv(UNIFORM_CAMPOS_LOC, 1, glm::value_ptr(cameraPosition));
//    glUniform3fv(UNIFORM_BALLPOS_LOC, 1, glm::value_ptr(ballPosition));
    fur_shader->activate();
    glUniform3fv(UNIFORM_CAMPOS_LOC, 1, glm::value_ptr(cameraPosition));
//    glUniform3fv(UNIFORM_BALLPOS_LOC, 1, glm::value_ptr(ballPosition));
    skybox_shader->activate();
    glUniform3fv(UNIFORM_CAMPOS_LOC, 1, glm::value_ptr(cameraPosition));
//    glUniform3fv(UNIFORM_BALLPOS_LOC, 1, glm::value_ptr(ballPosition));

}

void updateNodeTransformations(SceneNode* node, glm::mat4 transformationThusFar) {
    glm::mat4 transformationMatrix =
              glm::translate(node->position)
            * glm::translate(node->referencePoint)
            * glm::rotate(node->rotation.y, glm::vec3(0,1,0))
            * glm::rotate(node->rotation.x, glm::vec3(1,0,0))
            * glm::rotate(node->rotation.z, glm::vec3(0,0,1))
            * glm::scale(node->scale)
            * glm::translate(-node->referencePoint);

    node->modelTF = transformationThusFar * transformationMatrix;

    switch(node->nodeType) {
        case GEOMETRY: break;
        case POINT_LIGHT: {
            // find total position in graph by model matrix
            lighting_shader->activate();
            glm::vec4 lightpos = node->modelTF * glm::vec4(0, 0, 0, 1);
            glUniform3fv(uniform_light_sources_position_loc[node->lightID], 1, glm::value_ptr(lightpos));
            glUniform3fv(uniform_light_sources_color_loc[node->lightID], 1, glm::value_ptr(node->lightColor));
            fur_shader->activate();
            glUniform3fv(fur_uniform_light_sources_position_loc[node->lightID], 1, glm::value_ptr(lightpos));
            glUniform3fv(fur_uniform_light_sources_color_loc[node->lightID], 1, glm::value_ptr(node->lightColor));
        }break;
        case SPOT_LIGHT: break;
        case FLAT_GEOMETRY: break;
        case NORMAL_MAPPED_GEOMETRY: break;
        case FUR_GEOMETRY: break;
        case SKYBOX: break;
    }

    for(SceneNode* child : node->children) {
        updateNodeTransformations(child, node->modelTF);
    }
}

void SceneNode::render() {
    switch(nodeType) {
        case NORMAL_MAPPED_GEOMETRY:
            if(vertexArrayObjectID != -1) {
                glm::mat4 mvp = VP * modelTF;
                glm::mat3 normal_matrix = glm::transpose(glm::inverse(modelTF));
                lighting_shader->activate();
                glUniform1i(UNIFORM_ENABLE_NMAP_LOC, 1);
                glUniformMatrix4fv(UNIFORM_MVP_LOC, 1, GL_FALSE, glm::value_ptr(mvp));
                glUniformMatrix4fv(UNIFORM_MODEL_LOC, 1, GL_FALSE, glm::value_ptr(modelTF));
                glUniformMatrix3fv(UNIFORM_NORMAL_MATRIX_LOC, 1, GL_FALSE, glm::value_ptr(normal_matrix));
                glBindTextureUnit(SIMPLE_NORMAL_SAMPLER, normalMapID);
                glBindTextureUnit(SIMPLE_TEXTURE_SAMPLER, textureID);
                glBindTextureUnit(SIMPLE_ROUGHNESS_SAMPLER, roughnessID);

                glBindVertexArray(vertexArrayObjectID);
                glDrawElements(GL_TRIANGLES, VAOIndexCount, GL_UNSIGNED_INT, nullptr);
            }
            break;

    case FUR_GEOMETRY:
        if(vertexArrayObjectID != -1) {
            glm::mat4 mvp = VP * modelTF;
            glm::mat3 normal_matrix = glm::transpose(glm::inverse(modelTF));
            fur_shader->activate();
            glUniformMatrix4fv(UNIFORM_MVP_LOC, 1, GL_FALSE, glm::value_ptr(mvp));
            glUniformMatrix4fv(UNIFORM_MODEL_LOC, 1, GL_FALSE, glm::value_ptr(modelTF));
            glUniformMatrix3fv(UNIFORM_NORMAL_MATRIX_LOC, 1, GL_FALSE, glm::value_ptr(normal_matrix));
            glBindTextureUnit(SIMPLE_TEXTURE_SAMPLER, textureID);
            glBindTextureUnit(SIMPLE_NORMAL_SAMPLER, normalMapID);
            glBindTextureUnit(SIMPLE_ROUGHNESS_SAMPLER, roughnessID);
            glBindTextureUnit(FUR_FUR_SAMPLER, furID);
            glBindTextureUnit(FUR_TURBULENCE_SAMPLER, turbulenceID);

            glBindVertexArray(vertexArrayObjectID);
            glDrawElements(GL_TRIANGLES, VAOIndexCount, GL_UNSIGNED_INT, nullptr);
        }
            break;

        case GEOMETRY:
            if(vertexArrayObjectID != -1) {
                glm::mat4 mvp = VP * modelTF;
                glm::mat3 normal_matrix = glm::transpose(glm::inverse(modelTF));
                lighting_shader->activate();
                glUniform1i(UNIFORM_ENABLE_NMAP_LOC, 0);
                glUniformMatrix4fv(UNIFORM_MVP_LOC, 1, GL_FALSE, glm::value_ptr(mvp));
                glUniformMatrix4fv(UNIFORM_MODEL_LOC, 1, GL_FALSE, glm::value_ptr(modelTF));
                glUniformMatrix3fv(UNIFORM_NORMAL_MATRIX_LOC, 1, GL_FALSE, glm::value_ptr(normal_matrix));

                glBindVertexArray(vertexArrayObjectID);
                glDrawElements(GL_TRIANGLES, VAOIndexCount, GL_UNSIGNED_INT, nullptr);
            }
            break;
        case POINT_LIGHT: break;
        case SPOT_LIGHT: break;
        case FLAT_GEOMETRY:
            if(vertexArrayObjectID != -1) {
                flat_geometry_shader->activate();
                glm::mat4 ortho = glm::ortho(0.f, (float)DEFAULT_WINDOW_WIDTH, 0.f, (float)DEFAULT_WINDOW_HEIGHT, -1.f, 1.f);
                ortho = ortho * modelTF;
                glUniformMatrix4fv(UNIFORM_MVP_LOC, 1, GL_FALSE, glm::value_ptr(ortho));

                glBindTextureUnit(TEX_TEXT_SAMPLER, textureID);
                glBindVertexArray(vertexArrayObjectID);
                glDrawElements(GL_TRIANGLES, VAOIndexCount, GL_UNSIGNED_INT, nullptr);
            }
            break;
        case SKYBOX:
            if(vertexArrayObjectID != -1) {
                glm::mat4 mvp = VP; // no model
                // undo camera translation
                mvp = glm::translate(mvp, -cameraPosition); // todo use existing matrices instead?
                skybox_shader->activate();
                glUniformMatrix4fv(UNIFORM_MVP_LOC, 1, GL_FALSE, glm::value_ptr(mvp));
                glBindTextureUnit(SKYBOX_CUBE_SAMPLER, textureID);
                glBindVertexArray(vertexArrayObjectID);
                glDrawElements(GL_TRIANGLES, VAOIndexCount, GL_UNSIGNED_INT, nullptr);
            }
            break;
    }

    for(SceneNode* child : children) {
        child->render();
    }
}

void renderFrame(GLFWwindow* window) {
    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glViewport(0, 0, windowWidth, windowHeight);

    rootNode->render();
}
