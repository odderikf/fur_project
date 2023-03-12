//
// Created by odder on 10/03/2023.
//

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>


#include "mesh.hpp"

Mesh::Mesh(const std::string &filename) {
    tinyobj::attrib_t attributes;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warning;
    std::string error;
    tinyobj::LoadObj(&attributes, &shapes, &materials, &warning, &error, filename.c_str(), nullptr, false);

    if (shapes.size() > 1){
        std::cerr << "Unsupported obj format: more than one mesh in file" << std::endl;
    }
    if (shapes.empty()){
        std::cerr << error << std::endl;
    }
    if (! warning.empty()) std::cerr << warning << std::endl;
    if (! error.empty()) std::cerr << error << std::endl;

    const auto &shape = shapes.front();
    std::cout << "Loaded object " << shape.name << " from file " << filename << std::endl;
    const auto &tmesh = shape.mesh;
    for (const auto i : tmesh.indices){
        indices.push_back(indices.size());
        glm::vec3 pos;
        pos.x = attributes.vertices.at(3*i.vertex_index);
        pos.y = attributes.vertices.at(3*i.vertex_index+1);
        pos.z = attributes.vertices.at(3*i.vertex_index+2);
        vertices.push_back(pos);
        glm::vec3 normal;
        normal.x = attributes.normals.at(3*i.normal_index + 0);
        normal.y = attributes.normals.at(3*i.normal_index + 1);
        normal.z = attributes.normals.at(3*i.normal_index + 2);
        normals.push_back(normal);
        glm::vec2 uv;
        uv.x = attributes.texcoords.at(2*i.texcoord_index + 0);
        uv.y = attributes.texcoords.at(2*i.texcoord_index + 1);
        uvs.push_back(uv);

    }
}
