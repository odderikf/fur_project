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
FurredGeometry* terrainNode;
TexturedGeometry* broadTerrainNode;
Skybox* skyBoxNode;
FurredGeometry* rickyFurNode;
TexturedGeometry* padNode;
PointLight* padLightNode;
PointLight* topLeftLightNode;
PointLight* topRightLightNode;
PointLight* sunNode; // todo dirlight
FlatGeometry* textNode;

CompositorNode *compositeNode;
GLuint semitransparent_pass_fb;
GLuint oit_color_buffer;
GLuint oit_accum_tex;
GLuint oit_reveal_tex;
GLuint oit_depth_buffer;

// These are heap allocated, because they should not be initialised at the start of the program
Gloom::Shader* opaque_lighting_shader;
Gloom::Shader* blending_lighting_shader;
Gloom::Shader* flat_geometry_shader;
Gloom::Shader* fur_shell_shader;
Gloom::Shader* fur_fin_shader;
Gloom::Shader* skybox_shader;
Gloom::Shader* compositing_shader;

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

const float debug_startTime = 0;
double realTime = debug_startTime;

glm::vec3 cameraPosition = glm::vec3(0, 2, -20);
glm::vec3 cameraRotation = glm::vec3(0, 0, 0);

glm::vec3 wind = glm::vec3(0,0,0);

bool skybox_demo = false;
bool fur_base_demo = false;
bool fur_shells_demo = false;
bool fur_fins_demo = false;
bool fur_shells_density_demo = false;
bool fur_shells_length_demo = false;

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

FurredGeometry::FurredGeometry(const std::string &objname) : TexturedGeometry(objname) {
    std::string filebase = "../res/textures/" + objname;
    furID = create_texture(filebase + "_fur.png");
    furNormalMapID = create_texture(filebase + "_fur_nrm.png");
    strandTextureID = create_texture(filebase + "_fur_str.png");
    furTurbulenceID = create_texture(filebase + "_fur_tur.png");
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

    // texture for opaque pass and color modulation
    glGenRenderbuffers(1, &oit_color_buffer);
    glBindRenderbuffer(GL_RENDERBUFFER, oit_color_buffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, oit_color_buffer);

    // texture for blended transparency color accumulation
    glGenTextures(1, &oit_accum_tex);
    glBindTexture(GL_TEXTURE_2D, oit_accum_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT,
                 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, oit_accum_tex, 0);

    // texture for blended real-alpha accumulation (as if using over)
    glGenTextures(1, &oit_reveal_tex);
    glBindTexture(GL_TEXTURE_2D, oit_reveal_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT,
                 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, oit_reveal_tex, 0);


    const GLenum dbs[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
    glDrawBuffers(3, dbs);

    // depth buffer
    glGenRenderbuffers(1, &oit_depth_buffer);
    glBindRenderbuffer(GL_RENDERBUFFER, oit_depth_buffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, oit_depth_buffer);


    // compile shaders

    // general shader (phong)
    opaque_lighting_shader = new Gloom::Shader();
    opaque_lighting_shader->makeBasicShader("../res/shaders/simple.vert", "../res/shaders/simple.frag");
    opaque_lighting_shader->activate();

    // oit pass shader (phong)
    blending_lighting_shader = new Gloom::Shader();
    blending_lighting_shader->makeBasicShader("../res/shaders/simple.vert", "../res/shaders/oit.frag");
    blending_lighting_shader->activate();

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
    compositing_shader = new Gloom::Shader();
    compositing_shader->attach("../res/shaders/compositor.vert");
    compositing_shader->attach("../res/shaders/compositor.frag");
    compositing_shader->link();
    compositing_shader->activate();


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
    terrainNode = new FurredGeometry("terrain");
    broadTerrainNode = new TexturedGeometry("broad_terrain");
    padNode  = new TexturedGeometry();
    rickyFurNode = new FurredGeometry("ricky");
    padLightNode = new PointLight();
    topLeftLightNode = new PointLight();
    topRightLightNode = new PointLight();
    sunNode = new PointLight();
    textNode = new FlatGeometry();
    compositeNode = new CompositorNode();

    // special case textures IDs
    textNode->textureID = create_texture("../res/textures/charmap.png");

    padNode->normalMapID = create_texture("../res/textures/paddle_nrm.png");
    padNode->textureID   = create_texture("../res/textures/paddle_col.png");
    padNode->roughnessID = create_texture("../res/textures/paddle_rgh.png");
    padNode->render_pass = SEMITRANSPARENT;

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

    // three colored lights, in what used to be the two corners and on the pad
    rootNode->children.push_back(topLeftLightNode);
    rootNode->children.push_back(topRightLightNode);
    padNode->children.push_back(padLightNode);

    // gen meshes

    skyBoxNode->vaoID  = boxVAO;
    skyBoxNode->vaoIndicesSize        = box.indices.size();

    padNode->vaoID  = padVAO;
    padNode->vaoIndicesSize        = pad.indices.size();

    textNode->vaoID = textVAO;
    textNode->vaoIndicesSize       = text_mesh.indices.size();

    pch1->vaoID = pch2->vaoID = pch3->vaoID = padNode->vaoID;
    pch1->vaoIndicesSize = pch2->vaoIndicesSize = pch3->vaoIndicesSize = padNode->vaoIndicesSize;

    Mesh compositeMesh;
    compositeMesh.vertices = {{-1,-1,0.5}, {1,-1,0.5}, {-1,1,0.5}, {1,1,0.5}};
    compositeMesh.indices = {0,1,2, 1,3,2};
    compositeNode->vaoID = generateBuffer(compositeMesh);
    compositeNode->vaoIndicesSize = 6;

    // light positions
    topLeftLightNode->position = { -85, 30, -120};
    topRightLightNode->position = { 85, 30, -120};
    padLightNode->position = {0,5,13};
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

    // one red, one green, one blue. These are also intensities, not normalized.
    topLeftLightNode->lightColor = {1., 0., 0.};
    topRightLightNode->lightColor = {0., 1., 0.};
    padLightNode->lightColor = {0., 0., 1.};

    sunNode->lightColor = {1, 1, 0.5};
    sunNode->lightColor *= 2000;

    // find uniform locations
    for (auto node : {topLeftLightNode, topRightLightNode, padLightNode, sunNode}) {
        std::string poslocname = fmt::format("{}[{}].{}", UNIFORM_POINT_LIGHT_SOURCES_NAME, node->lightID,
                                             UNIFORM_POINT_LIGHT_SOURCES_POSITION_NAME);
        uniform_light_sources_position_loc[node->lightID] = opaque_lighting_shader->getUniformFromName(poslocname);
        std::string collocname = fmt::format("{}[{}].{}", UNIFORM_POINT_LIGHT_SOURCES_NAME, node->lightID,
                                             UNIFORM_POINT_LIGHT_SOURCES_COLOR_NAME);
        uniform_light_sources_color_loc[node->lightID] = opaque_lighting_shader->getUniformFromName(collocname);
    }
    for (auto node : {topLeftLightNode, topRightLightNode, padLightNode, sunNode}) {
        std::string poslocname = fmt::format("{}[{}].{}", UNIFORM_POINT_LIGHT_SOURCES_NAME, node->lightID,
                                             UNIFORM_POINT_LIGHT_SOURCES_POSITION_NAME);
        uniform_oit_light_sources_position_loc[node->lightID] = blending_lighting_shader->getUniformFromName(poslocname);
        std::string collocname = fmt::format("{}[{}].{}", UNIFORM_POINT_LIGHT_SOURCES_NAME, node->lightID,
                                             UNIFORM_POINT_LIGHT_SOURCES_COLOR_NAME);
        uniform_oit_light_sources_color_loc[node->lightID] = blending_lighting_shader->getUniformFromName(collocname);
    }
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

    float timeDelta = getTimeDeltaSeconds();

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
    if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS)
    {
        skybox_demo = false;
        fur_base_demo = false;
        fur_shells_demo = false;
        fur_fins_demo = false;
        fur_shells_density_demo = false;
        fur_shells_length_demo = false;
    }
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
    {
        skybox_demo = true;
        fur_base_demo = false;
        fur_shells_demo = false;
        fur_fins_demo = false;
        fur_shells_density_demo = false;
        fur_shells_length_demo = false;
    }
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
    {
        skybox_demo = false;
        fur_base_demo = true;
        fur_shells_demo = false;
        fur_fins_demo = false;
        fur_shells_density_demo = false;
        fur_shells_length_demo = false;
    }
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
    {
        skybox_demo = false;
        fur_base_demo = false;
        fur_shells_demo = true;
        fur_fins_demo = false;
        fur_shells_density_demo = false;
        fur_shells_length_demo = false;
    }
    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS)
    {
        skybox_demo = false;
        fur_base_demo = false;
        fur_shells_demo = false;
        fur_fins_demo = true;
        fur_shells_density_demo = false;
        fur_shells_length_demo = false;
    }
    if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS)
    {
        skybox_demo = false;
        fur_base_demo = false;
        fur_shells_demo = false;
        fur_fins_demo = false;
        fur_shells_density_demo = true;
        fur_shells_length_demo = false;
    }
    if (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS)
    {
        skybox_demo = false;
        fur_base_demo = false;
        fur_shells_demo = false;
        fur_fins_demo = false;
        fur_shells_density_demo = false;
        fur_shells_length_demo = true;
    }
    realTime += timeDelta;

    wind = glm::vec3(
        0.1*glm::sin((2*realTime)),
        0,
        0
    );

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
//    rickyFurNode->rotation = {0, 0.1*realTime, 0.01*realTime};

    glm::vec3 previous_boxDimensions = {180, 90, 90};
    glm::vec3 previous_boxPosition = { 0, -10, -80 };;
    padNode->position  = {
            previous_boxPosition.x - (previous_boxDimensions.x/2) + (padDimensions.x/2) + (1 - padPositionX) * (previous_boxDimensions.x - padDimensions.x),
            100 + previous_boxPosition.y - (previous_boxDimensions.y/2) + (padDimensions.y/2),
            previous_boxPosition.z - (previous_boxDimensions.z/2) + (padDimensions.z/2) + (1 - padPositionZ) * (previous_boxDimensions.z - padDimensions.z)
    };

    rootNode->update(glm::identity<glm::mat4>());
    opaque_lighting_shader->activate();
    glUniform3fv(UNIFORM_CAMPOS_LOC, 1, glm::value_ptr(cameraPosition));
    blending_lighting_shader->activate();
    glUniform3fv(UNIFORM_CAMPOS_LOC, 1, glm::value_ptr(cameraPosition));
    fur_shell_shader->activate();
    glUniform3fv(UNIFORM_CAMPOS_LOC, 1, glm::value_ptr(cameraPosition));
    fur_fin_shader->activate();
    glUniform3fv(UNIFORM_CAMPOS_LOC, 1, glm::value_ptr(cameraPosition));

}

void PointLight::update(glm::mat4 transformationThusFar) {
    // find total position in graph by model matrix
    glm::vec4 lightpos = modelTF * glm::vec4(0, 0, 0, 1);

    opaque_lighting_shader->activate();
    glUniform3fv(uniform_light_sources_position_loc[lightID], 1, glm::value_ptr(lightpos));
    glUniform3fv(uniform_light_sources_color_loc[lightID], 1, glm::value_ptr(lightColor));
    blending_lighting_shader->activate();
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

    opaque_lighting_shader->activate();
    glUniform3fv(uniform_light_sources_position_loc[lightID], 1, glm::value_ptr(lightpos));
    glUniform3fv(uniform_light_sources_color_loc[lightID], 1, glm::value_ptr(lightColor));
    blending_lighting_shader->activate();
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
    if(render_pass == pass && vaoID != -1) {
        glm::mat4 mvp = VP * modelTF;
        glm::mat3 normal_matrix = glm::transpose(glm::inverse(modelTF));
        compositing_shader->activate();

        glBindTextureUnit(ACCUMULATION_SAMPLER, oit_accum_tex);
        glBindTextureUnit(REVEALAGE_SAMPLER, oit_reveal_tex);

        glBindVertexArray(vaoID);
        glDrawElements(GL_TRIANGLES, vaoIndicesSize, GL_UNSIGNED_INT, nullptr);
    }
}

void SceneNode::render(render_type pass) {
    for(SceneNode* child : children) {
        child->render(pass);
    }
}

void Skybox::render(render_type pass) {
    if(render_pass == pass && vaoID != -1) {
        glm::mat4 mvp = VP; // no model
        // undo camera translation
        mvp = glm::translate(mvp, -cameraPosition);
        skybox_shader->activate();
        glUniformMatrix4fv(UNIFORM_MVP_LOC, 1, GL_FALSE, glm::value_ptr(mvp));
        glBindTextureUnit(SKYBOX_CUBE_SAMPLER, textureID);
        glBindVertexArray(vaoID);
        glDrawElements(GL_TRIANGLES, vaoIndicesSize, GL_UNSIGNED_INT, nullptr);
    }
    for(SceneNode* child : children) {
        child->render(pass);
    }
}

void Geometry::render(render_type pass) {
    if(render_pass == pass && vaoID != -1) {
        glm::mat4 mvp = VP * modelTF;
        glm::mat3 normal_matrix = glm::transpose(glm::inverse(modelTF));
        if(render_pass == SEMITRANSPARENT) blending_lighting_shader->activate();
        else opaque_lighting_shader->activate();
        glUniform1i(UNIFORM_ENABLE_NMAP_LOC, 0);
        glUniformMatrix4fv(UNIFORM_MVP_LOC, 1, GL_FALSE, glm::value_ptr(mvp));
        glUniformMatrix4fv(UNIFORM_MODEL_LOC, 1, GL_FALSE, glm::value_ptr(modelTF));
        glUniformMatrix3fv(UNIFORM_NORMAL_MATRIX_LOC, 1, GL_FALSE, glm::value_ptr(normal_matrix));

        glBindVertexArray(vaoID);
        glDrawElements(GL_TRIANGLES, vaoIndicesSize, GL_UNSIGNED_INT, nullptr);
    }
    SceneNode::render(pass);
}


void FurredGeometry::render(render_type pass) {
    if(vaoID != -1) {
        if (render_pass == pass){
            glm::mat4 mvp = VP * modelTF;
            glm::mat3 normal_matrix = glm::transpose(glm::inverse(modelTF));

            // draw shells of fur volume

            fur_shell_shader->activate();
            glUniformMatrix4fv(UNIFORM_MVP_LOC, 1, GL_FALSE, glm::value_ptr(mvp));
            glUniformMatrix4fv(UNIFORM_MODEL_LOC, 1, GL_FALSE, glm::value_ptr(modelTF));
            glUniformMatrix3fv(UNIFORM_NORMAL_MATRIX_LOC, 1, GL_FALSE, glm::value_ptr(normal_matrix));
            if(fur_shells_density_demo) {
                float timefac = std::fmod(realTime/3,1.);
                glUniform1f(9, timefac);
            } else {
                glUniform1f(9, 1.001);
            }
            if(fur_shells_length_demo) {
                float timefac = std::fmod(realTime/3,2.);
                glUniform1f(UNIFORM_FUR_LENGTH_LOC, timefac*strand_length);
            } else {
                glUniform1f(UNIFORM_FUR_LENGTH_LOC, strand_length);
            }
            glUniform3fv(UNIFORM_WIND_LOC, 1, glm::value_ptr(wind));

            glBindTextureUnit(SIMPLE_TEXTURE_SAMPLER, textureID);
            glBindTextureUnit(SIMPLE_NORMAL_SAMPLER, furNormalMapID);
            glBindTextureUnit(SIMPLE_ROUGHNESS_SAMPLER, roughnessID);
            glBindTextureUnit(FUR_FUR_SAMPLER, furID);
            glBindTextureUnit(FUR_TURBULENCE_SAMPLER, furTurbulenceID);

            glBindVertexArray(vaoID);
            if(fur_fins_demo){

            } else {
                glDrawElements(GL_TRIANGLES, vaoIndicesSize, GL_UNSIGNED_INT, nullptr);
            }
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
            glUniform3fv(UNIFORM_WIND_LOC, 1, glm::value_ptr(wind));

            glBindTextureUnit(SIMPLE_TEXTURE_SAMPLER, strandTextureID);
            glBindTextureUnit(FUR_FUR_SAMPLER, furID);

            if(fur_shells_demo || fur_shells_density_demo || fur_shells_length_demo){

            } else {
                glDrawElements(GL_TRIANGLES, vaoIndicesSize, GL_UNSIGNED_INT, nullptr);
            }
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
    if(render_pass == pass && vaoID != -1) {
        glm::mat4 mvp = VP * modelTF;
        glm::mat3 normal_matrix = glm::transpose(glm::inverse(modelTF));
        if(render_pass == SEMITRANSPARENT) blending_lighting_shader->activate();
        else opaque_lighting_shader->activate();
        glUniform1i(UNIFORM_ENABLE_NMAP_LOC, 1);
        glUniformMatrix4fv(UNIFORM_MVP_LOC, 1, GL_FALSE, glm::value_ptr(mvp));
        glUniformMatrix4fv(UNIFORM_MODEL_LOC, 1, GL_FALSE, glm::value_ptr(modelTF));
        glUniformMatrix3fv(UNIFORM_NORMAL_MATRIX_LOC, 1, GL_FALSE, glm::value_ptr(normal_matrix));
        glBindTextureUnit(SIMPLE_NORMAL_SAMPLER, normalMapID);
        glBindTextureUnit(SIMPLE_TEXTURE_SAMPLER, textureID);
        glBindTextureUnit(SIMPLE_ROUGHNESS_SAMPLER, roughnessID);

        glBindVertexArray(vaoID);
        glDrawElements(GL_TRIANGLES, vaoIndicesSize, GL_UNSIGNED_INT, nullptr);
    }
    for(SceneNode* child : children) {
        child->render(pass);
    }
}

void FlatGeometry::render(render_type pass) {
    if(render_pass == pass && vaoID != -1) {
        flat_geometry_shader->activate();
        glm::mat4 ortho = glm::ortho(0.f, (float)DEFAULT_WINDOW_WIDTH, 0.f, (float)DEFAULT_WINDOW_HEIGHT, -1.f, 1.f);
        ortho = ortho * modelTF;
        glUniformMatrix4fv(UNIFORM_MVP_LOC, 1, GL_FALSE, glm::value_ptr(ortho));

        glBindTextureUnit(TEX_TEXT_SAMPLER, textureID);
        glBindVertexArray(vaoID);
        glDrawElements(GL_TRIANGLES, vaoIndicesSize, GL_UNSIGNED_INT, nullptr);
    }
    for(SceneNode* child : children) {
        child->render(pass);
    }
}

void renderFrame(GLFWwindow* window) {
    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glViewport(0, 0, windowWidth, windowHeight);

    // clear fb
    glBindFramebuffer(GL_FRAMEBUFFER, semitransparent_pass_fb);

    GLenum bufs[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
    glDrawBuffers(3, bufs);

    glm::vec4 color_clear(1., 1., 1., 1.);
    glClearBufferfv(GL_COLOR, 0, glm::value_ptr(color_clear));

    glm::vec4 accum_clear(0.,0.,0.,0.);
    glClearBufferfv(GL_COLOR, 1, glm::value_ptr(accum_clear));

    glm::vec4 reveal_clear(1.,0.,0.,0.);
    glClearBufferfv(GL_COLOR, 2, glm::value_ptr(reveal_clear));

    glClear(GL_DEPTH_BUFFER_BIT);

    // draw the base opaque scene, write depth, use depth
    glBlendFunci(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    if(skybox_demo){
        skyBoxNode->render(OPAQUE);
    } else {
        rootNode->render(OPAQUE);
    }


    // draw blended transparent objects
    // Have the base color of objects filter the colors behind them
    glBlendFunci(0, GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
    // accumulated blended lit color, blend weighting happens in shader
    glBlendFunci(1, GL_ONE, GL_ONE);
    // remaining transparency of blend,
    // white stencil slowly subtracted black where opaque background becomes hid
    glBlendFunci(2, GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
    // transparent things do not occlude, so do not write to depth
    glDepthMask(GL_FALSE);

    glDrawBuffers(3, bufs);
    if (skybox_demo || fur_base_demo){

    } else {
        rootNode->render(SEMITRANSPARENT);
    }


    // composite transparent onto opaque
    // This just composits buffers, so depth is irrelevant
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_ALWAYS);

    // use other layers as textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, oit_accum_tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, oit_reveal_tex);
    // compose them on top of opaque color
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if(skybox_demo){

    } else {
        compositeNode->render(OPAQUE);
    }


    // copy onto screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, semitransparent_pass_fb);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glBlitFramebuffer(0,0,DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT,
                      0,0,DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // add UI
//    rootNode->render(UI);
}
