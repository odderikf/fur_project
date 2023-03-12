#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>
#include <iostream>

class Mesh {
public:
    Mesh::Mesh(const std::string &filename);

public:
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;
    std::vector<GLsizei> indices;
};
