#include "scenegraph.hpp"
#include <iostream>
#include <utilities/mesh.hpp>
#include <utilities/glutils.hpp>

SceneNode* createSceneNode() {
	return new SceneNode();
}

// Add a child node to its parent's list of children
void addChild(SceneNode* parent, SceneNode* child) {
	parent->children.push_back(child);
}

int totalChildren(SceneNode* parent) {
	int count = parent->children.size();
	for (SceneNode* child : parent->children) {
		count += totalChildren(child);
	}
	return count;
}


Geometry::Geometry(const std::string &objname) : SceneNode() {
    Mesh m("../res/models/" + objname + ".obj");
    unsigned int terrainVAO = generateBuffer(m);
    vaoID = terrainVAO;
    vaoIndicesSize = m.indices.size();

}