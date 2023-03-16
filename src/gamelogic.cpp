#include <chrono>
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm/vec3.hpp>
#include <iostream>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fmt/format.h>
#include "gamelogic.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

#include "utilities/imageLoader.hpp"
#include "utilities/glfont.h"
#include "utilities/timeutils.h"
#include "utilities/shapes.h"
#include "utilities/glutils.h"
#include "utilities/shader.hpp"


#include "shader_uniform_defines.hpp"
#include "window.hpp"


double padPositionX = 0;
double padPositionZ = 0;

SceneNode* rootNode;
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

const glm::vec3 boxDimensions(180, 90, 90);
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

GLuint create_texture(GLsizei width, GLsizei height,  std::vector<unsigned char> pixels) {
    GLuint tex_id = 0;
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    return tex_id;
}

Mesh loadObj(std::string filename){
    tinyobj::attrib_t attributes;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string error;
    std::string warning;
    tinyobj::LoadObj(&attributes, &shapes, &materials, &warning, &error, filename.c_str(), nullptr, false);

    Mesh m;
    if (shapes.size() > 1){
        std::cerr << "Unsupported obj format: more than one mesh in file" << std::endl;
        return m;
    }
    if (shapes.size() == 0){
        std::cerr << error << std::endl;
        return m;
    }
    const auto &shape = shapes.front();
    std::cout << "Loaded object " << shape.name << " from file " << filename << std::endl;
    const auto &tmesh = shape.mesh;
    for (const auto i : tmesh.indices){
        m.indices.push_back(m.indices.size());
        glm::vec3 pos;
        pos.x = attributes.vertices.at(3*i.vertex_index);
        pos.y = attributes.vertices.at(3*i.vertex_index+1);
        pos.z = attributes.vertices.at(3*i.vertex_index+2);
        m.vertices.push_back(pos);
        glm::vec3 normal;
        normal.x = attributes.normals.at(3*i.normal_index + 0);
        normal.y = attributes.normals.at(3*i.normal_index + 1);
        normal.z = attributes.normals.at(3*i.normal_index + 2);
        m.normals.push_back(normal);
        glm::vec2 uv;
        uv.x = attributes.texcoords.at(2*i.texcoord_index + 0);
        uv.y = attributes.texcoords.at(2*i.texcoord_index + 1);
        m.textureCoordinates.push_back(uv);

    }

    return m;
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

    // load textures
    PNGImage charmap = loadPNGFile("../res/textures/charmap.png");

    PNGImage paddle_normal_map    = loadPNGFile("../res/textures/paddle_nrm.png");
    PNGImage paddle_diffuse_map   = loadPNGFile("../res/textures/paddle_col.png");
    PNGImage paddle_roughness_map = loadPNGFile("../res/textures/paddle_rgh.png");

    PNGImage ricky_normal_map    = loadPNGFile("../res/textures/ricky_nrm.png");
    PNGImage ricky_fur_normal_map    = loadPNGFile("../res/textures/ricky_fur_nrm.png");
    PNGImage ricky_diffuse_map   = loadPNGFile("../res/textures/ricky_col.png");
    PNGImage ricky_roughness_map = loadPNGFile("../res/textures/ricky_rgh.png");
    PNGImage ricky_fur_texture = loadPNGFile("../res/textures/ricky_fur.png");
    PNGImage turbulence_texture = loadPNGFile("../res/textures/turbulence.png");

    // create textures
    GLuint charmap_id = create_texture(charmap.width, charmap.height, charmap.pixels);

    GLuint paddle_normal_id    = create_texture(paddle_normal_map.width,    paddle_normal_map.height,    paddle_normal_map.pixels);
    GLuint paddle_diffuse_id   = create_texture(paddle_diffuse_map.width,   paddle_diffuse_map.height,   paddle_diffuse_map.pixels);
    GLuint paddle_roughness_id = create_texture(paddle_roughness_map.width, paddle_roughness_map.height, paddle_roughness_map.pixels);

    GLuint ricky_normal_id    = create_texture(ricky_normal_map.width,    ricky_normal_map.height,    ricky_normal_map.pixels);
    GLuint ricky_fur_normal_id    = create_texture(ricky_fur_normal_map.width,    ricky_fur_normal_map.height,    ricky_fur_normal_map.pixels);
    GLuint ricky_diffuse_id   = create_texture(ricky_diffuse_map.width,   ricky_diffuse_map.height,   ricky_diffuse_map.pixels);
    GLuint ricky_roughness_id = create_texture(ricky_roughness_map.width, ricky_roughness_map.height, ricky_roughness_map.pixels);
    GLuint ricky_fur_texture_id = create_texture(ricky_fur_texture.width, ricky_fur_texture.height, ricky_fur_texture.pixels);

    GLuint turbulence_id = create_texture(turbulence_texture.width, turbulence_texture.height, turbulence_texture.pixels);

    // gen meshes
    Mesh pad = cube(padDimensions, glm::vec2(30, 40), true);
    Mesh box = cube(boxDimensions, glm::vec2(90), true, true);
    Mesh sphere = generateSphere(1.0, 40, 40);
    Mesh ricky = loadObj("../res/models/ricky.obj");

    const float textwidth = 400;
    const float textratio = 1.34482759;
    Mesh text_mesh = generateTextGeometryBuffer("Click to start", textratio, textwidth);


    // Fill buffers
    unsigned int boxVAO  = generateBuffer(box);
    unsigned int padVAO  = generateBuffer(pad);
    unsigned int textVAO = generateBuffer(text_mesh);
    unsigned int rickyVAO = generateBuffer(ricky);

    // Construct scene
    rootNode = createSceneNode();
    boxNode  = createSceneNode();
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

    rootNode->children.push_back(padNode);
    rootNode->children.push_back(rickyNode);
    rickyNode->children.push_back(rickyFurNode);

    // transparency nodes
    rootNode->children.push_back(textNode);

    int which_lights = 1;

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

    boxNode->nodeType = NORMAL_MAPPED_GEOMETRY;
    padNode->nodeType = NORMAL_MAPPED_GEOMETRY;
    rickyNode->nodeType = NORMAL_MAPPED_GEOMETRY;
    rickyFurNode->nodeType = FUR_GEOMETRY;

    topLeftLightNode->position = { -85, 30, -120};
    topRightLightNode->position = { 85, 30, -120};
    padLightNode->position = {0,5,13};
    whiteLightNode->position = {0,20,-20};

    textNode->position = { DEFAULT_WINDOW_WIDTH/2 - textwidth/2, DEFAULT_WINDOW_HEIGHT/2, 0};

    rickyNode->position = {-50, 0, -90};

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

    realTime += timeDelta;

    glm::mat4 projection = glm::perspective(glm::radians(80.0f), float(DEFAULT_WINDOW_WIDTH) / float(DEFAULT_WINDOW_HEIGHT), 0.1f, 350.f);

    // Some math to make the camera move in a nice way
    glm::vec3 cameraPosition = glm::vec3(0, 2, -20);
    glm::mat4 cameraTransform = glm::translate(-cameraPosition);

    VP = projection * cameraTransform;

    // Move and rotate various SceneNodes
    boxNode->position = { 0, -10, -80 };

    rickyNode->rotation = {0, 0.1*realTime, 0.01*realTime};

    padNode->position  = {
        boxNode->position.x - (boxDimensions.x/2) + (padDimensions.x/2) + (1 - padPositionX) * (boxDimensions.x - padDimensions.x),
        boxNode->position.y - (boxDimensions.y/2) + (padDimensions.y/2),
        boxNode->position.z - (boxDimensions.z/2) + (padDimensions.z/2) + (1 - padPositionZ) * (boxDimensions.z - padDimensions.z)
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
    }

    for(SceneNode* child : node->children) {
        updateNodeTransformations(child, node->modelTF);
    }
}

void renderNode(SceneNode* node) {

    switch(node->nodeType) {
        case NORMAL_MAPPED_GEOMETRY:
            if(node->vertexArrayObjectID != -1) {
                glm::mat4 mvp = VP * node->modelTF;
                glm::mat3 normal_matrix = glm::transpose(glm::inverse(node->modelTF));
                lighting_shader->activate();
                glUniform1i(UNIFORM_ENABLE_NMAP_LOC, 1);
                glUniformMatrix4fv(UNIFORM_MVP_LOC, 1, GL_FALSE, glm::value_ptr(mvp));
                glUniformMatrix4fv(UNIFORM_MODEL_LOC, 1, GL_FALSE, glm::value_ptr(node->modelTF));
                glUniformMatrix3fv(UNIFORM_NORMAL_MATRIX_LOC, 1, GL_FALSE, glm::value_ptr(normal_matrix));
                glBindTextureUnit(SIMPLE_NORMAL_SAMPLER, node->normalMapID);
                glBindTextureUnit(SIMPLE_TEXTURE_SAMPLER, node->textureID);
                glBindTextureUnit(SIMPLE_ROUGHNESS_SAMPLER, node->roughnessID);

                glBindVertexArray(node->vertexArrayObjectID);
                glDrawElements(GL_TRIANGLES, node->VAOIndexCount, GL_UNSIGNED_INT, nullptr);
            }
            break;

    case FUR_GEOMETRY:
        if(node->vertexArrayObjectID != -1) {
            glm::mat4 mvp = VP * node->modelTF;
            glm::mat3 normal_matrix = glm::transpose(glm::inverse(node->modelTF));
            fur_shader->activate();
            glUniformMatrix4fv(UNIFORM_MVP_LOC, 1, GL_FALSE, glm::value_ptr(mvp));
            glUniformMatrix4fv(UNIFORM_MODEL_LOC, 1, GL_FALSE, glm::value_ptr(node->modelTF));
            glUniformMatrix3fv(UNIFORM_NORMAL_MATRIX_LOC, 1, GL_FALSE, glm::value_ptr(normal_matrix));
            glBindTextureUnit(SIMPLE_TEXTURE_SAMPLER, node->textureID);
            glBindTextureUnit(SIMPLE_NORMAL_SAMPLER, node->normalMapID);
            glBindTextureUnit(SIMPLE_ROUGHNESS_SAMPLER, node->roughnessID);
            glBindTextureUnit(FUR_FUR_SAMPLER, node->furID);
            glBindTextureUnit(FUR_TURBULENCE_SAMPLER, turbulenceID);

            glBindVertexArray(node->vertexArrayObjectID);
            glDrawElements(GL_TRIANGLES, node->VAOIndexCount, GL_UNSIGNED_INT, nullptr);
        }
            break;

        case GEOMETRY:
            if(node->vertexArrayObjectID != -1) {
                glm::mat4 mvp = VP * node->modelTF;
                glm::mat3 normal_matrix = glm::transpose(glm::inverse(node->modelTF));
                lighting_shader->activate();
                glUniform1i(UNIFORM_ENABLE_NMAP_LOC, 0);
                glUniformMatrix4fv(UNIFORM_MVP_LOC, 1, GL_FALSE, glm::value_ptr(mvp));
                glUniformMatrix4fv(UNIFORM_MODEL_LOC, 1, GL_FALSE, glm::value_ptr(node->modelTF));
                glUniformMatrix3fv(UNIFORM_NORMAL_MATRIX_LOC, 1, GL_FALSE, glm::value_ptr(normal_matrix));

                glBindVertexArray(node->vertexArrayObjectID);
                glDrawElements(GL_TRIANGLES, node->VAOIndexCount, GL_UNSIGNED_INT, nullptr);
            }
            break;
        case POINT_LIGHT: break;
        case SPOT_LIGHT: break;
        case FLAT_GEOMETRY:
            if(node->vertexArrayObjectID != -1) {
                flat_geometry_shader->activate();
                glm::mat4 ortho = glm::ortho(0.f, (float)DEFAULT_WINDOW_WIDTH, 0.f, (float)DEFAULT_WINDOW_HEIGHT, -1.f, 1.f);
                ortho = ortho * node->modelTF;
                glUniformMatrix4fv(UNIFORM_MVP_LOC, 1, GL_FALSE, glm::value_ptr(ortho));

                glBindTextureUnit(TEX_TEXT_SAMPLER, node->textureID);
                glBindVertexArray(node->vertexArrayObjectID);
                glDrawElements(GL_TRIANGLES, node->VAOIndexCount, GL_UNSIGNED_INT, nullptr);
            }
            break;
    }

    for(SceneNode* child : node->children) {
        renderNode(child);
    }
}

void renderFrame(GLFWwindow* window) {
    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glViewport(0, 0, windowWidth, windowHeight);

    renderNode(rootNode);
}
