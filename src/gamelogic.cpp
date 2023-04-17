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
GLuint semitransparent_pass_fb;
GLuint oit_accum_tex;
GLuint oit_reveal_tex;
GLuint oit_depth_buffer;

double ballRadius = 3.0f;

// These are heap allocated, because they should not be initialised at the start of the program
Gloom::Shader* lighting_shader;
Gloom::Shader* oit_lighting_shader;
Gloom::Shader* flat_geometry_shader;
Gloom::Shader* fur_shell_shader;
Gloom::Shader* fur_fin_shader;
Gloom::Shader* skybox_shader;
Gloom::Shader* composite_shader;

const glm::vec3 boxDimensions(1, 1, 1);
const glm::vec3 padDimensions(30, 3, 40);

// global so it can be multiplied in to make MVP locally
glm::mat4 VP;

GLint uniform_light_sources_position_loc[UNIFORM_POINT_LIGHT_SOURCES_LEN];
GLint uniform_light_sources_color_loc[UNIFORM_POINT_LIGHT_SOURCES_LEN];

GLint uniform_oit_light_sources_position_loc[UNIFORM_POINT_LIGHT_SOURCES_LEN];
GLint uniform_oit_light_sources_color_loc[UNIFORM_POINT_LIGHT_SOURCES_LEN];

GLint fur_shell_uniform_light_sources_position_loc[UNIFORM_POINT_LIGHT_SOURCES_LEN];
GLint fur_shell_uniform_light_sources_color_loc[UNIFORM_POINT_LIGHT_SOURCES_LEN];

GLint fur_fin_uniform_light_sources_position_loc[UNIFORM_POINT_LIGHT_SOURCES_LEN];
GLint fur_fin_uniform_light_sources_color_loc[UNIFORM_POINT_LIGHT_SOURCES_LEN];

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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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
    strandTextureID = create_texture(filebase + "_fur_str.png");
}
void GLAPIENTRY
MessageCallback( GLenum source,
                 GLenum type,
                 GLuint id,
                 GLenum severity,
                 GLsizei length,
                 const GLchar* message,
                 const void* userParam )
{
    std::cerr << "GL CALLBACK: " <<
    "source: " << source <<
    " type:" << type <<
    " id: " << id <<
    " severity: " << severity <<
    " length: " << length <<
    " message: " << message << std::endl;

}
void initialize_game(GLFWwindow* window) {
#ifdef __DEBUG__
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(MessageCallback, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_OTHER , GL_DONT_CARE, 0, nullptr, false);
#endif
    // gl settings
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glfwSwapBuffers(window);

    // set up first pass frame buffer
    glGenFramebuffers(1, &semitransparent_pass_fb);
    glBindFramebuffer(GL_FRAMEBUFFER, semitransparent_pass_fb);

    // texture for blended transparency color accumulation
    glGenTextures(1, &oit_accum_tex);
    glBindTexture(GL_TEXTURE_2D, oit_accum_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT,
                 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); todo remove?
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, oit_accum_tex, 0);

    // texture for blended real-alpha accumulation (as if using over)
    glGenTextures(1, &oit_reveal_tex);
    glBindTexture(GL_TEXTURE_2D, oit_reveal_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT,
                 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, oit_reveal_tex, 0);


    const GLenum dbs[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, dbs);

    // depth buffer
    glGenRenderbuffers(1, &oit_depth_buffer);
    glBindRenderbuffer(GL_RENDERBUFFER, oit_depth_buffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, oit_depth_buffer);

    // its own clear color
    glClearColor(1.f, 0.0f, 1.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    glfwSetCursorPosCallback(window, mouseCallback);

    // general shader (phong)
    lighting_shader = new Gloom::Shader();
    lighting_shader->makeBasicShader("../res/shaders/simple.vert", "../res/shaders/simple.frag");
    lighting_shader->activate();

    // oit pass shader (phong)
    oit_lighting_shader = new Gloom::Shader();
    oit_lighting_shader->makeBasicShader("../res/shaders/simple.vert", "../res/shaders/oit.frag");
    oit_lighting_shader->activate();

    // text / UI / 2d shader
    flat_geometry_shader = new Gloom::Shader();
    flat_geometry_shader->makeBasicShader("../res/shaders/flat_geom.vert", "../res/shaders/flat_geom.frag");
    flat_geometry_shader->activate();

    // Fur shell geometry shader
    fur_shell_shader = new Gloom::Shader();
    fur_shell_shader->attach("../res/shaders/fur.vert");
    fur_shell_shader->attach("../res/shaders/fur_shell.frag");
    fur_shell_shader->attach("../res/shaders/fur_shell.geom");
    fur_shell_shader->link();
    fur_shell_shader->activate();

    // Fur fin geometry shader
    fur_fin_shader = new Gloom::Shader();
    fur_fin_shader->attach("../res/shaders/fur.vert");
    fur_fin_shader->attach("../res/shaders/fur_fin.frag");
    fur_fin_shader->attach("../res/shaders/fur_fin.geom");
    fur_fin_shader->link();
    fur_fin_shader->activate();

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
    padNode->render_pass = SEMITRANSPARENT;

    turbulenceID = create_texture("../res/textures/turbulence.png");

    skyBoxNode->textureID = create_cubemap("../res/textures/skybox/");

    rootNode->children.push_back(skyBoxNode);

    rootNode->children.push_back(sunNode);

    rootNode->children.push_back(padNode);
    rootNode->children.push_back(rickyFurNode);

    rootNode->children.push_back(terrainNode);
    terrainNode->children.push_back(broadTerrainNode);

    // transparency nodes
    rootNode->children.push_back(textNode);

    TexturedGeometry *pch1 = new TexturedGeometry();
    TexturedGeometry *pch2 = new TexturedGeometry();
    TexturedGeometry *pch3 = new TexturedGeometry();
    pch1->textureID = pch2->textureID = pch3->textureID = padNode->textureID;
    pch1->roughnessID = pch2->roughnessID = pch3->roughnessID = padNode->roughnessID;
    pch1->normalMapID = pch2->normalMapID = pch3->normalMapID = padNode->normalMapID;
    pch1->position = pch2->position = pch3->position = {0, -5, 0};
    pch1->rotation = pch2->rotation = pch3->rotation = {1, 1, 0};
    pch1->render_pass = pch2->render_pass = pch3->render_pass = padNode->render_pass;
    padNode->children.push_back(pch1);
    pch1->children.push_back(pch2);
    pch2->children.push_back(pch3);

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

    pch1->vertexArrayObjectID = pch2->vertexArrayObjectID = pch3->vertexArrayObjectID = padNode->vertexArrayObjectID;
    pch1->VAOIndexCount = pch2->VAOIndexCount = pch3->VAOIndexCount = padNode->VAOIndexCount;

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

    terrainNode->strand_length = 15;
    rickyFurNode->strand_length = 1;


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
    sunNode->lightColor *= 2000;

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
    for (auto node : {topLeftLightNode, topRightLightNode, padLightNode, sunNode}) {
        std::string poslocname = fmt::format("{}[{}].{}", UNIFORM_POINT_LIGHT_SOURCES_NAME, node->lightID,
                                             UNIFORM_POINT_LIGHT_SOURCES_POSITION_NAME);
        uniform_oit_light_sources_position_loc[node->lightID] = oit_lighting_shader->getUniformFromName(poslocname);
        std::string collocname = fmt::format("{}[{}].{}", UNIFORM_POINT_LIGHT_SOURCES_NAME, node->lightID,
                                             UNIFORM_POINT_LIGHT_SOURCES_COLOR_NAME);
        uniform_oit_light_sources_color_loc[node->lightID] = oit_lighting_shader->getUniformFromName(collocname);
    }
    // do it again for the fur shader
    for (auto node : {topLeftLightNode, topRightLightNode, padLightNode, sunNode}) {
        std::string poslocname = fmt::format("{}[{}].{}", UNIFORM_POINT_LIGHT_SOURCES_NAME, node->lightID,
                                             UNIFORM_POINT_LIGHT_SOURCES_POSITION_NAME);
        fur_shell_uniform_light_sources_position_loc[node->lightID] = fur_shell_shader->getUniformFromName(poslocname);
        std::string collocname = fmt::format("{}[{}].{}", UNIFORM_POINT_LIGHT_SOURCES_NAME, node->lightID,
                                             UNIFORM_POINT_LIGHT_SOURCES_COLOR_NAME);
        fur_shell_uniform_light_sources_color_loc[node->lightID] = fur_shell_shader->getUniformFromName(collocname);
    }
    for (auto node : {topLeftLightNode, topRightLightNode, padLightNode, sunNode}) {
        std::string poslocname = fmt::format("{}[{}].{}", UNIFORM_POINT_LIGHT_SOURCES_NAME, node->lightID,
                                             UNIFORM_POINT_LIGHT_SOURCES_POSITION_NAME);
        fur_fin_uniform_light_sources_position_loc[node->lightID] = fur_fin_shader->getUniformFromName(poslocname);
        std::string collocname = fmt::format("{}[{}].{}", UNIFORM_POINT_LIGHT_SOURCES_NAME, node->lightID,
                                             UNIFORM_POINT_LIGHT_SOURCES_COLOR_NAME);
        fur_fin_uniform_light_sources_color_loc[node->lightID] = fur_fin_shader->getUniformFromName(collocname);
    }

    getTimeDeltaSeconds();

    std::cout << fmt::format("Initialized scene with {} SceneNodes.", totalChildren(rootNode)) << std::endl;

    std::cout << "Ready. Click to start!" << std::endl;
    cameraPosition = glm::vec3(0, 0, 0);
    cameraRotation = glm::vec3(0, 0, 0);
}

void updateFrame(GLFWwindow* window) {
//    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); todo

    float timeDelta = getTimeDeltaSeconds();

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
    oit_lighting_shader->activate();
    glUniform3fv(UNIFORM_CAMPOS_LOC, 1, glm::value_ptr(cameraPosition));
    fur_shell_shader->activate();
    glUniform3fv(UNIFORM_CAMPOS_LOC, 1, glm::value_ptr(cameraPosition));
    fur_fin_shader->activate();
    glUniform3fv(UNIFORM_CAMPOS_LOC, 1, glm::value_ptr(cameraPosition));

}

void PointLight::update(glm::mat4 transformationThusFar) {
    // find total position in graph by model matrix
    glm::vec4 lightpos = modelTF * glm::vec4(0, 0, 0, 1);

    lighting_shader->activate();
    glUniform3fv(uniform_light_sources_position_loc[lightID], 1, glm::value_ptr(lightpos));
    glUniform3fv(uniform_light_sources_color_loc[lightID], 1, glm::value_ptr(lightColor));
    oit_lighting_shader->activate();
    glUniform3fv(uniform_oit_light_sources_position_loc[lightID], 1, glm::value_ptr(lightpos));
    glUniform3fv(uniform_oit_light_sources_color_loc[lightID], 1, glm::value_ptr(lightColor));
    fur_shell_shader->activate();
    glUniform3fv(fur_shell_uniform_light_sources_position_loc[lightID], 1, glm::value_ptr(lightpos));
    glUniform3fv(fur_shell_uniform_light_sources_color_loc[lightID], 1, glm::value_ptr(lightColor));
    fur_fin_shader->activate();
    glUniform3fv(fur_fin_uniform_light_sources_position_loc[lightID], 1, glm::value_ptr(lightpos));
    glUniform3fv(fur_fin_uniform_light_sources_color_loc[lightID], 1, glm::value_ptr(lightColor));
    SceneNode::update(transformationThusFar);
}

void DirLight::update(glm::mat4 transformationThusFar) {
    // find total position in graph by model matrix
    glm::vec4 lightpos = modelTF * glm::vec4(0, 0, 0, 1);

    lighting_shader->activate();
    glUniform3fv(uniform_light_sources_position_loc[lightID], 1, glm::value_ptr(lightpos));
    glUniform3fv(uniform_light_sources_color_loc[lightID], 1, glm::value_ptr(lightColor));
    oit_lighting_shader->activate();
    glUniform3fv(uniform_oit_light_sources_position_loc[lightID], 1, glm::value_ptr(lightpos));
    glUniform3fv(uniform_oit_light_sources_color_loc[lightID], 1, glm::value_ptr(lightColor));
    fur_shell_shader->activate();
    glUniform3fv(fur_shell_uniform_light_sources_position_loc[lightID], 1, glm::value_ptr(lightpos));
    glUniform3fv(fur_shell_uniform_light_sources_color_loc[lightID], 1, glm::value_ptr(lightColor));
    fur_fin_shader->activate();
    glUniform3fv(fur_shell_uniform_light_sources_position_loc[lightID], 1, glm::value_ptr(lightpos));
    glUniform3fv(fur_shell_uniform_light_sources_color_loc[lightID], 1, glm::value_ptr(lightColor));
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

void CompositorNode::render(render_type pass) {
    if(render_pass == pass && vertexArrayObjectID != -1) {
        glm::mat4 mvp = VP * modelTF;
        glm::mat3 normal_matrix = glm::transpose(glm::inverse(modelTF));
        composite_shader->activate();

        glBindTextureUnit(ACCUMULATION_SAMPLER, oit_accum_tex);
        glBindTextureUnit(REVEALANCE_SAMPLER, oit_reveal_tex);

        glBindVertexArray(vertexArrayObjectID);
        glDrawElements(GL_TRIANGLES, VAOIndexCount, GL_UNSIGNED_INT, nullptr);
    }
}

void SceneNode::render(render_type pass) {
    for(SceneNode* child : children) {
        child->render(pass);
    }
}

void Skybox::render(render_type pass) {
    if(render_pass == pass && vertexArrayObjectID != -1) {
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
        child->render(pass);
    }
}

void Geometry::render(render_type pass) {
    if(render_pass == pass && vertexArrayObjectID != -1) {
        glm::mat4 mvp = VP * modelTF;
        glm::mat3 normal_matrix = glm::transpose(glm::inverse(modelTF));
        if(render_pass == SEMITRANSPARENT) oit_lighting_shader->activate();
        else lighting_shader->activate();
        glUniform1i(UNIFORM_ENABLE_NMAP_LOC, 0);
        glUniformMatrix4fv(UNIFORM_MVP_LOC, 1, GL_FALSE, glm::value_ptr(mvp));
        glUniformMatrix4fv(UNIFORM_MODEL_LOC, 1, GL_FALSE, glm::value_ptr(modelTF));
        glUniformMatrix3fv(UNIFORM_NORMAL_MATRIX_LOC, 1, GL_FALSE, glm::value_ptr(normal_matrix));

        glBindVertexArray(vertexArrayObjectID);
        glDrawElements(GL_TRIANGLES, VAOIndexCount, GL_UNSIGNED_INT, nullptr);
    }
    SceneNode::render(pass);
}


void FurLayer::render(render_type pass) {
    if(vertexArrayObjectID != -1) {
        if (render_pass == pass){
            // draw shells of fur volume
            glm::mat4 mvp = VP * modelTF;
            glm::mat3 normal_matrix = glm::transpose(glm::inverse(modelTF));
            fur_shell_shader->activate();
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

            // draw silhouette fins
            // these should be a little longer to match length and  stick out a little,
            // so the texture has a little room at the top
            float fin_strand_length_fac = 1.2;
            glDisable(GL_CULL_FACE);
            fur_fin_shader->activate();
            glUniformMatrix4fv(UNIFORM_MVP_LOC, 1, GL_FALSE, glm::value_ptr(mvp));
            glUniformMatrix4fv(UNIFORM_MODEL_LOC, 1, GL_FALSE, glm::value_ptr(modelTF));
            glUniformMatrix3fv(UNIFORM_NORMAL_MATRIX_LOC, 1, GL_FALSE, glm::value_ptr(normal_matrix));
            glUniform1f(UNIFORM_FUR_LENGTH_LOC, fin_strand_length_fac*strand_length);

            glBindTextureUnit(SIMPLE_TEXTURE_SAMPLER, strandTextureID);
            glBindTextureUnit(FUR_FUR_SAMPLER, furID);

            glDrawElements(GL_TRIANGLES, VAOIndexCount, GL_UNSIGNED_INT, nullptr);

            glEnable(GL_CULL_FACE);

        } else if (pass == OPAQUE) {
            // draw base in opaque pass
            TexturedGeometry::render(OPAQUE);
        }
    }
    for(SceneNode* child : children) {
        child->render(pass);
    }
}

void TexturedGeometry::render(render_type pass) {
    if(render_pass == pass && vertexArrayObjectID != -1) {
        glm::mat4 mvp = VP * modelTF;
        glm::mat3 normal_matrix = glm::transpose(glm::inverse(modelTF));
        if(render_pass == SEMITRANSPARENT) oit_lighting_shader->activate();
        else lighting_shader->activate();
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
        child->render(pass);
    }
}

void FlatGeometry::render(render_type pass) {
    if(render_pass == pass && vertexArrayObjectID != -1) {
        flat_geometry_shader->activate();
        glm::mat4 ortho = glm::ortho(0.f, (float)DEFAULT_WINDOW_WIDTH, 0.f, (float)DEFAULT_WINDOW_HEIGHT, -1.f, 1.f);
        ortho = ortho * modelTF;
        glUniformMatrix4fv(UNIFORM_MVP_LOC, 1, GL_FALSE, glm::value_ptr(ortho));

        glBindTextureUnit(TEX_TEXT_SAMPLER, textureID);
        glBindVertexArray(vertexArrayObjectID);
        glDrawElements(GL_TRIANGLES, VAOIndexCount, GL_UNSIGNED_INT, nullptr);
    }
    for(SceneNode* child : children) {
        child->render(pass);
    }
}

void renderFrame(GLFWwindow* window) {
    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glViewport(0, 0, windowWidth, windowHeight);

    // draw opaque
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    rootNode->render(OPAQUE);

    // draw blended transparent
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, semitransparent_pass_fb);

    GLenum bufs[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};

    glDepthMask(GL_FALSE);
    glm::vec4 accum_clear(0.,0.,0.,0.);
    glClearBufferfv(GL_COLOR, 0, glm::value_ptr(accum_clear));
    glBlendFunci(0, GL_ONE, GL_ONE);

    glm::vec4 reveal_clear(1.,0.,0.,0.);
    glClearBufferfv(GL_COLOR, 1, glm::value_ptr(reveal_clear));
    glBlendFunci(1, GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
    glBlendEquation(GL_FUNC_ADD);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBlitFramebuffer(0,0,DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT,
                      0,0,DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT,
                      GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glDrawBuffers(2, bufs);
    rootNode->render(SEMITRANSPARENT);


    // composite transparent onto opaque
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDepthFunc(GL_ALWAYS);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, oit_accum_tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, oit_reveal_tex);
    compositeNode->render(OPAQUE);

    // add UI
//    rootNode->render(UI);
}
