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
#include "scenegraph.hpp"


double padPositionX = 0;
double padPositionZ = 0;

SceneNode* rootNode;
FurLayer* terrainNode;
TexturedGeometry* broadTerrainNode;
Skybox* skyBoxNode;
FurLayer* rickyFurNode;
TexturedGeometry* padNode;
PointLight* padLightNode;
PointLight* topLeftLightNode;
PointLight* topRightLightNode;
PointLight* trio0LightNode;
PointLight* trio1LightNode;
PointLight* trio2LightNode;
PointLight* whiteLightNode;
PointLight* sunNode; // todo dirlight
FlatGeometry* textNode;

CompositorNode *compositeNode;
GLuint first_pass_fb;
GLuint first_pass_tex;

double ballRadius = 3.0f;

// These are heap allocated, because they should not be initialised at the start of the program
Gloom::Shader* lighting_shader;
Gloom::Shader* flat_geometry_shader;
Gloom::Shader* fur_shader;
Gloom::Shader* skybox_shader;
Gloom::Shader* composite_shader;

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
// todo
//    glfwSetCursorPos(window, windowWidth / 2, windowHeight / 2);
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

TexturedGeometry::TexturedGeometry(const std::string &objname) : Geometry(objname) {
    std::string filebase = "../res/textures/" + objname;
    textureID = create_texture(filebase + "_col.png");
    normalMapID = create_texture(filebase + "_nrm.png");
    roughnessID = create_texture(filebase + "_rgh.png");
}

FurLayer::FurLayer(const std::string &objname) : TexturedGeometry(objname) {
    std::string filebase = "../res/textures/" + objname;
    furID = create_texture(filebase + "_fur.png");
    furNormalMapID = create_texture(filebase + "_fur_nrm.png");
}

void initialize_game(GLFWwindow* window) {

    // gl settings
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);

    glClearColor(0.f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glfwSwapBuffers(window);

    // set up first pass frame buffer
    glGenFramebuffers(1, &first_pass_fb);
    glBindFramebuffer(GL_FRAMEBUFFER, first_pass_fb);

    // texture output
    glGenTextures(1, &first_pass_tex);
    glBindTexture(GL_TEXTURE_2D, first_pass_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT,
                                0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, first_pass_tex, 0);

    // depth buffer
    unsigned int rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

    // its own clear color
    glClearColor(1.f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    glfwSetCursorPosCallback(window, mouseCallback);

    // general shader (phong)
    lighting_shader = new Gloom::Shader();
    lighting_shader->makeBasicShader("../res/shaders/simple.vert", "../res/shaders/simple.frag");
    lighting_shader->activate();

    // text / UI / 2d shader
    flat_geometry_shader = new Gloom::Shader();
    flat_geometry_shader->makeBasicShader("../res/shaders/flat_geom.vert", "../res/shaders/flat_geom.frag");
    flat_geometry_shader->activate();

    // Fur shell geometry shader todo fins?
    fur_shader = new Gloom::Shader();
    fur_shader->attach("../res/shaders/fur.vert");
    fur_shader->attach("../res/shaders/fur.frag");
    fur_shader->attach("../res/shaders/fur.geom");
    fur_shader->link();
    fur_shader->activate();

    // shader for position invariant skybox
    skybox_shader = new Gloom::Shader();
    skybox_shader->attach("../res/shaders/skybox.vert");
    skybox_shader->attach("../res/shaders/skybox.frag");
    skybox_shader->link();
    skybox_shader->activate();

    // shader that composits framebuffers to screen
    composite_shader = new Gloom::Shader();
    composite_shader->attach("../res/shaders/compositor.vert");
    composite_shader->attach("../res/shaders/compositor.frag");
    composite_shader->link();
    composite_shader->activate();

    // gen meshes
    Mesh pad = cube(padDimensions, glm::vec2(30, 40), true);
    Mesh box = cube(boxDimensions, glm::vec2(90), true, true);
    Mesh sphere = generateSphere(1.0, 40, 40);

    const float textwidth = 400;
    const float textratio = 1.34482759f;


    // Generated meshes
    Mesh text_mesh = generateTextGeometryBuffer("Click to start", textratio, textwidth);
    unsigned int boxVAO  = generateBuffer(box);
    unsigned int padVAO  = generateBuffer(pad);
    unsigned int textVAO = generateBuffer(text_mesh);

    // Construct scene
    rootNode = new SceneNode();
    skyBoxNode  = new Skybox();
    terrainNode = new FurLayer("terrain");
    broadTerrainNode = new TexturedGeometry("broad_terrain");
    padNode  = new TexturedGeometry();
    rickyFurNode = new FurLayer("ricky");
    padLightNode = new PointLight();
    topLeftLightNode = new PointLight();
    topRightLightNode = new PointLight();
    trio0LightNode = new PointLight();
    trio1LightNode = new PointLight();
    trio2LightNode = new PointLight();
    whiteLightNode = new PointLight();
    sunNode = new PointLight();
    textNode = new FlatGeometry();
    compositeNode = new CompositorNode();

    // special case textures IDs
    textNode->textureID = create_texture("../res/textures/charmap.png");

    padNode->normalMapID = create_texture("../res/textures/paddle_nrm.png");
    padNode->textureID   = create_texture("../res/textures/paddle_col.png");
    padNode->roughnessID = create_texture("../res/textures/paddle_rgh.png");

    turbulenceID = create_texture("../res/textures/turbulence.png");

    skyBoxNode->textureID = create_cubemap("../res/textures/skybox/");

    rootNode->children.push_back(skyBoxNode);

    rootNode->children.push_back(sunNode);

    rootNode->children.push_back(terrainNode);
    terrainNode->children.push_back(broadTerrainNode);
    rootNode->children.push_back(padNode);
    rootNode->children.push_back(rickyFurNode);

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

    // gen meshes

    skyBoxNode->vertexArrayObjectID  = boxVAO;
    skyBoxNode->VAOIndexCount        = box.indices.size();

    padNode->vertexArrayObjectID  = padVAO;
    padNode->VAOIndexCount        = pad.indices.size();

    textNode->vertexArrayObjectID = textVAO;
    textNode->VAOIndexCount       = text_mesh.indices.size();

    Mesh compositeMesh;
    compositeMesh.vertices = {{-1,-1,0.5}, {1,-1,0.5}, {-1,1,0.5}, {1,1,0.5}};
    compositeMesh.indices = {0,1,2, 1,3,2};
    compositeNode->vertexArrayObjectID = generateBuffer(compositeMesh);
    compositeNode->VAOIndexCount = 6;

    // light positions
    topLeftLightNode->position = { -85, 30, -120};
    topRightLightNode->position = { 85, 30, -120};
    padLightNode->position = {0,5,13};
    whiteLightNode->position = {0,20,-20};
    sunNode->position = {0, 5000, 0};

    // geometry positions
    textNode->position = { DEFAULT_WINDOW_WIDTH/2 - textwidth/2, DEFAULT_WINDOW_HEIGHT/2, 0};

    rickyFurNode->position = {-50, 20, -90};

    skyBoxNode->position = {0, 0, 0 };

    terrainNode->position = {0, 0, 0};
    broadTerrainNode->position = {0, 40, 0};

    terrainNode->strand_length = 25;


    // uniform array index IDs
    topLeftLightNode->lightID = 0;
    topRightLightNode->lightID = 1;
    padLightNode->lightID = 2;
    sunNode->lightID = 3;

    whiteLightNode->lightID = 0;

    trio0LightNode->lightID = 0;
    trio1LightNode->lightID = 1;
    trio2LightNode->lightID = 2;

    // one red, one green, one blue. These are also intensities, not normalized.
    topLeftLightNode->lightColor = {1., 0., 0.};
    topRightLightNode->lightColor = {0., 1., 0.};
    padLightNode->lightColor = {0., 0., 1.};

    sunNode->lightColor = {1, 1, 0.5};
    sunNode->lightColor *= 1000;

    whiteLightNode->lightColor = {0.9, 0.9, 0.9};

    trio0LightNode->lightColor = {1., 0., 0.};
    trio1LightNode->lightColor = {0., 1., 0.};
    trio2LightNode->lightColor = {0., 0., 1.};


    // find uniform locations
    for (auto node : {topLeftLightNode, topRightLightNode, padLightNode, sunNode}) {
        std::string poslocname = fmt::format("{}[{}].{}", UNIFORM_POINT_LIGHT_SOURCES_NAME, node->lightID,
                                             UNIFORM_POINT_LIGHT_SOURCES_POSITION_NAME);
        uniform_light_sources_position_loc[node->lightID] = lighting_shader->getUniformFromName(poslocname);
        std::string collocname = fmt::format("{}[{}].{}", UNIFORM_POINT_LIGHT_SOURCES_NAME, node->lightID,
                                             UNIFORM_POINT_LIGHT_SOURCES_COLOR_NAME);
        uniform_light_sources_color_loc[node->lightID] = lighting_shader->getUniformFromName(collocname);
    }
    // do it again for the fur shader
    for (auto node : {topLeftLightNode, topRightLightNode, padLightNode, sunNode}) {
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
    cameraPosition = glm::vec3(0, -20, -20);
    cameraRotation = glm::vec3(0, 0, 0);
}

void updateFrame(GLFWwindow* window) {
//    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); todo

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
        4000.f
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
    rickyFurNode->rotation = {0, 0.1*realTime, 0.01*realTime};

    glm::vec3 previous_boxDimensions = {180, 90, 90};
    glm::vec3 previous_boxPosition = { 0, -10, -80 };;
    padNode->position  = {
            previous_boxPosition.x - (previous_boxDimensions.x/2) + (padDimensions.x/2) + (1 - padPositionX) * (previous_boxDimensions.x - padDimensions.x),
            100 + previous_boxPosition.y - (previous_boxDimensions.y/2) + (padDimensions.y/2),
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

    rootNode->update(glm::identity<glm::mat4>());
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

void PointLight::update(glm::mat4 transformationThusFar) {
    // find total position in graph by model matrix
    lighting_shader->activate();
    glm::vec4 lightpos = modelTF * glm::vec4(0, 0, 0, 1);
    glUniform3fv(uniform_light_sources_position_loc[lightID], 1, glm::value_ptr(lightpos));
    glUniform3fv(uniform_light_sources_color_loc[lightID], 1, glm::value_ptr(lightColor));
    fur_shader->activate();
    glUniform3fv(fur_uniform_light_sources_position_loc[lightID], 1, glm::value_ptr(lightpos));
    glUniform3fv(fur_uniform_light_sources_color_loc[lightID], 1, glm::value_ptr(lightColor));
    SceneNode::update(transformationThusFar);
}

void DirLight::update(glm::mat4 transformationThusFar) {
    // find total position in graph by model matrix
    lighting_shader->activate();
    glm::vec4 lightpos = modelTF * glm::vec4(0, 0, 0, 1);
    glUniform3fv(uniform_light_sources_position_loc[lightID], 1, glm::value_ptr(lightpos));
    glUniform3fv(uniform_light_sources_color_loc[lightID], 1, glm::value_ptr(lightColor));
    fur_shader->activate();
    glUniform3fv(fur_uniform_light_sources_position_loc[lightID], 1, glm::value_ptr(lightpos));
    glUniform3fv(fur_uniform_light_sources_color_loc[lightID], 1, glm::value_ptr(lightColor));
    SceneNode::update(transformationThusFar);
}
void SceneNode::update(glm::mat4 transformationThusFar) {
    glm::mat4 transformationMatrix =
              glm::translate(position)
            * glm::translate(referencePoint)
            * glm::rotate(rotation.y, glm::vec3(0,1,0))
            * glm::rotate(rotation.x, glm::vec3(1,0,0))
            * glm::rotate(rotation.z, glm::vec3(0,0,1))
            * glm::scale(scale)
            * glm::translate(-referencePoint);

    modelTF = transformationThusFar * transformationMatrix;

    for(SceneNode* child : children) {
        child->update( modelTF);
    }
}

void CompositorNode::render() {
    if(vertexArrayObjectID != -1) {
        glm::mat4 mvp = VP * modelTF;
        glm::mat3 normal_matrix = glm::transpose(glm::inverse(modelTF));
        composite_shader->activate();
        glUniform1i(UNIFORM_ENABLE_NMAP_LOC, 0);
        glUniformMatrix4fv(UNIFORM_MVP_LOC, 1, GL_FALSE, glm::value_ptr(mvp));
        glUniformMatrix4fv(UNIFORM_MODEL_LOC, 1, GL_FALSE, glm::value_ptr(modelTF));
        glUniformMatrix3fv(UNIFORM_NORMAL_MATRIX_LOC, 1, GL_FALSE, glm::value_ptr(normal_matrix));

        glBindTextureUnit(0, first_pass_tex); // todo define

        glBindVertexArray(vertexArrayObjectID);
        glDrawElements(GL_TRIANGLES, VAOIndexCount, GL_UNSIGNED_INT, nullptr);
    }
}

void SceneNode::render() {
    for(SceneNode* child : children) {
        child->render();
    }
}

void Skybox::render() {
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
    for(SceneNode* child : children) {
        child->render();
    }
}

void Geometry::render() {
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
    SceneNode::render();
}


void FurLayer::render() {
    TexturedGeometry::render();
    if(vertexArrayObjectID != -1) {
        glm::mat4 mvp = VP * modelTF;
        glm::mat3 normal_matrix = glm::transpose(glm::inverse(modelTF));
        fur_shader->activate();
        glUniformMatrix4fv(UNIFORM_MVP_LOC, 1, GL_FALSE, glm::value_ptr(mvp));
        glUniformMatrix4fv(UNIFORM_MODEL_LOC, 1, GL_FALSE, glm::value_ptr(modelTF));
        glUniformMatrix3fv(UNIFORM_NORMAL_MATRIX_LOC, 1, GL_FALSE, glm::value_ptr(normal_matrix));
        glUniform1f(UNIFORM_FUR_LENGTH_LOC, strand_length);
        glBindTextureUnit(SIMPLE_TEXTURE_SAMPLER, textureID);
        glBindTextureUnit(SIMPLE_NORMAL_SAMPLER, furNormalMapID);
        glBindTextureUnit(SIMPLE_ROUGHNESS_SAMPLER, roughnessID);
        glBindTextureUnit(FUR_FUR_SAMPLER, furID);
        glBindTextureUnit(FUR_TURBULENCE_SAMPLER, turbulenceID);

        glBindVertexArray(vertexArrayObjectID);
        glDrawElements(GL_TRIANGLES, VAOIndexCount, GL_UNSIGNED_INT, nullptr);
    }
    for(SceneNode* child : children) {
        child->render();
    }
}

void TexturedGeometry::render() {
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
    for(SceneNode* child : children) {
        child->render();
    }
}

void FlatGeometry::render() {
    if(vertexArrayObjectID != -1) {
        flat_geometry_shader->activate();
        glm::mat4 ortho = glm::ortho(0.f, (float)DEFAULT_WINDOW_WIDTH, 0.f, (float)DEFAULT_WINDOW_HEIGHT, -1.f, 1.f);
        ortho = ortho * modelTF;
        glUniformMatrix4fv(UNIFORM_MVP_LOC, 1, GL_FALSE, glm::value_ptr(ortho));

        glBindTextureUnit(TEX_TEXT_SAMPLER, textureID);
        glBindVertexArray(vertexArrayObjectID);
        glDrawElements(GL_TRIANGLES, VAOIndexCount, GL_UNSIGNED_INT, nullptr);
    }
    for(SceneNode* child : children) {
        child->render();
    }
}

void renderFrame(GLFWwindow* window) {
    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glViewport(0, 0, windowWidth, windowHeight);

    glEnable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, first_pass_fb);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    rootNode->render();

    glDisable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    compositeNode->render();
}
