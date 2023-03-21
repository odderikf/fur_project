#pragma once

#include <glm/glm.hpp>

#include <vector>
#include <glad/glad.h>
#include <string>

class SceneNode {
public:
	SceneNode() {
		position = glm::vec3(0, 0, 0);
		rotation = glm::vec3(0, 0, 0);
		scale = glm::vec3(1, 1, 1);

        referencePoint = glm::vec3(0, 0, 0);
        furID = 0;

	}

	// A list of all children that belong to this node.
	// For instance, in case of the scene graph of a human body shown in the assignment text, the "Upper Torso" node would contain the "Left Arm", "Right Arm", "Head" and "Lower Torso" nodes in its list of children.
	std::vector<SceneNode*> children;
	
	// The node's position and rotation relative to its parent
	glm::vec3 position;
	glm::vec3 rotation;
	glm::vec3 scale;

	// A transformation matrix representing the transformation of the node's location relative to its parent. This matrix is updated every frame.
	glm::mat4 modelTF;

	// The location of the node's reference point
	glm::vec3 referencePoint;

    GLuint furID;

    virtual void render();

    virtual void update(glm::mat4 transformationThusFar);
};

class Geometry : public SceneNode {
public:
    GLuint textureID = 0;
    int vertexArrayObjectID = -1;
    GLsizei VAOIndexCount = 0;
    Geometry() : SceneNode() {}
    explicit Geometry(const std::string &objname);
    void render() override;
};

class TexturedGeometry : public Geometry {
public:

    GLuint roughnessID = 0;
    GLuint normalMapID = 0;
    TexturedGeometry() : Geometry() {}
    explicit TexturedGeometry(const std::string &objname);
    void render() override;
};

class Skybox : public Geometry {
public:
    Skybox() : Geometry() {}
    explicit Skybox(const std::string &objname) : Geometry(objname) {};
    void render() override;
};

class FurLayer : public TexturedGeometry {
public:
    FurLayer() : TexturedGeometry() {}
    explicit FurLayer(const std::string &objname) : TexturedGeometry(objname) {};
    void render() override;
};

class FlatGeometry : public Geometry {
public:
    FlatGeometry() : Geometry() {}
    explicit FlatGeometry(const std::string &objname) : Geometry(objname) {};
    void render() override;
};

class LightNode : public SceneNode {
public:
    // index in light arrays, if nodeType is POINT_LIGHT / SPOT_LIGHT.
    int lightID = 0;
    glm::vec3 lightColor = glm::vec3(0.6, 0.6, 0.6);

};

class PointLight : public LightNode {
public:
    void update(glm::mat4 transformationThusFar) override;
};
class DirLight : public LightNode {
public:
    void update(glm::mat4 transformationThusFar) override;
};

SceneNode* createSceneNode();
void addChild(SceneNode* parent, SceneNode* child);
void printNode(SceneNode* node);
int totalChildren(SceneNode* parent);

// For more details, see SceneGraph.cpp.