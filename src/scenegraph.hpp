#pragma once

#include <glm/glm.hpp>

#include <vector>

enum SceneNodeType {
	GEOMETRY, POINT_LIGHT, SPOT_LIGHT, FLAT_GEOMETRY, NORMAL_MAPPED_GEOMETRY, FUR_GEOMETRY
};

struct SceneNode {
	SceneNode() {
		position = glm::vec3(0, 0, 0);
		rotation = glm::vec3(0, 0, 0);
		scale = glm::vec3(1, 1, 1);

        referencePoint = glm::vec3(0, 0, 0);
        vertexArrayObjectID = -1;
        VAOIndexCount = 0;

        lightID = -1;
        textureID = -1;
        normalMapID = -1;
        roughnessID = -1;
        furID = -1;
        lightColor = glm::vec3(0.6, 0.6, 0.6);

        nodeType = GEOMETRY;

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

	// The ID of the VAO containing the "appearance" of this SceneNode.
	int vertexArrayObjectID;
	unsigned int VAOIndexCount;

    // index in light arrays, if nodeType is POINT_LIGHT / SPOT_LIGHT.
    int lightID;
    glm::vec3 lightColor;

    int textureID;
    int roughnessID;
    int normalMapID;
    int furID;

	// Node type is used to determine how to handle the contents of a node
	SceneNodeType nodeType;
};

SceneNode* createSceneNode();
void addChild(SceneNode* parent, SceneNode* child);
void printNode(SceneNode* node);
int totalChildren(SceneNode* parent);

// For more details, see SceneGraph.cpp.