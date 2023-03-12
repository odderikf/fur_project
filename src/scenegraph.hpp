#pragma once

#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "utils/mesh.hpp"

class SceneNode {
protected:
    glm::mat4 modelTF;
    glm::vec3 referencePoint;
public:
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale = {1,1,1};
    std::vector<SceneNode*> children;

    virtual void render();

    virtual void updateTFs(glm::mat4 cumulative_tf);
};


class DrawableNode : public SceneNode {
public:
    GLuint vao;
    GLsizei vao_index_count;

    void render() override;
};

class GeometryNode : public DrawableNode {
public:
    void render() override;
    void register_vao(const Mesh &mesh);
};

class LightNode : public SceneNode {
    unsigned lightID;
    glm::vec3 lightColor;
};

class DirLightNode : public LightNode {
    glm::vec3 direction;
};

class PointLightNode : public LightNode {
    glm::vec3 location;
    void updateTFs(glm::mat4 cumulative_tf) override;
};