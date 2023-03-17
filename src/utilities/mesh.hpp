#pragma once

#include <vector>
#include <string>
#include <glm/glm.hpp>

class Mesh {
public:
    explicit Mesh(const std::string &filename);
    Mesh() = default;
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> textureCoordinates;

    std::vector<unsigned int> indices;
};