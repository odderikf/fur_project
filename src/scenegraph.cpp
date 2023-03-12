//
// Created by odder on 10/03/2023.
//
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>

#include "shader_uniform_defines.h"
#include "game_logic.hpp"
#include "scenegraph.hpp"

void SceneNode::render() {
    for (auto c : children) c->render();
}

void SceneNode::updateTFs(glm::mat4 cumulative_tf) {
    glm::mat4 tf =
            glm::translate(position)
            * glm::translate(referencePoint)
            * glm::rotate(rotation.y, glm::vec3(0,1,0))
            * glm::rotate(rotation.x, glm::vec3(1,0,0))
            * glm::rotate(rotation.z, glm::vec3(0,0,1))
            * glm::scale(scale)
            * glm::translate(-referencePoint);
    modelTF = cumulative_tf * tf;
    for (auto c : children) c->updateTFs(modelTF);
}

void DrawableNode::render() {
    SceneNode::render();
}

void GeometryNode::render() {
    if (vao != -1){
        glm::mat4 mvp = VP * modelTF;
        glm::mat3 normal_matrix = glm::transpose(glm::inverse(modelTF));
        lighting_shader.activate();
//        glUniform1i(UNIFORM_ENABLE_NMAP_LOC, 0);
        glUniformMatrix4fv(UNIFORM_MVP_LOC, 1, GL_FALSE, glm::value_ptr(mvp));
//        glUniformMatrix4fv(UNIFORM_MODEL_LOC, 1, GL_FALSE, glm::value_ptr(modelTF));
//        glUniformMatrix3fv(UNIFORM_NORMAL_MATRIX_LOC, 1, GL_FALSE, glm::value_ptr(normal_matrix));

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, vao_index_count, GL_UNSIGNED_INT, nullptr);
    }
    DrawableNode::render();
}

template <class T>
unsigned int generateAttribute(int id, int elementsPerEntry, std::vector<T> data, bool normalize) {
    unsigned int bufferID;
    glGenBuffers(1, &bufferID);
    glBindBuffer(GL_ARRAY_BUFFER, bufferID);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(T), data.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(id, elementsPerEntry, GL_FLOAT, normalize ? GL_TRUE : GL_FALSE, sizeof(T), 0);
    glEnableVertexAttribArray(id);
    return bufferID;
}

void GeometryNode::register_vao(const Mesh &mesh) {
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    generateAttribute(0, 3, mesh.vertices, false);
    generateAttribute(1, 3, mesh.normals, true);
    if (mesh.uvs.size() > 0) {
        generateAttribute(2, 2, mesh.uvs, false);
    }

    std::vector<glm::vec3> tangents(mesh.normals.size());

    for (int i = 0; i < mesh.normals.size(); i += 3){
        // http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-13-normal-mapping/
        // Shortcuts for vertices
        const glm::vec3 & v0 = mesh.vertices[i+0];
        const glm::vec3 & v1 = mesh.vertices[i+1];
        const glm::vec3 & v2 = mesh.vertices[i+2];

        // Shortcuts for UVs
        const glm::vec2 & uv0 = mesh.uvs[i+0];
        const glm::vec2 & uv1 = mesh.uvs[i+1];
        const glm::vec2 & uv2 = mesh.uvs[i+2];

        // Edges of the triangle : position delta
        glm::vec3 deltaPos1 = v1-v0;
        glm::vec3 deltaPos2 = v2-v0;

        // UV delta
        glm::vec2 deltaUV1 = uv1-uv0;
        glm::vec2 deltaUV2 = uv2-uv0;

        float r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
        glm::vec3 tangent = (deltaPos1 * deltaUV2.y   - deltaPos2 * deltaUV1.y)*r;
        tangents.at(i) = tangents.at(i+1) = tangents.at(i+2) = tangent;
    }

    generateAttribute(3, 3, tangents, false);

    unsigned int indexBufferID;
    glGenBuffers(1, &indexBufferID);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(unsigned int), mesh.indices.data(), GL_STATIC_DRAW);

    vao_index_count = mesh.indices.size();
}

void PointLightNode::updateTFs(glm::mat4 cumulative_tf) {
    SceneNode::updateTFs(cumulative_tf);
    // find total position in graph by model matrix
    lighting_shader.activate();
    glm::vec4 lightpos = modelTF * glm::vec4(0, 0, 0, 1);
    // todo:
//    glUniform3fv(uniform_light_sources_position_loc[node->lightID], 1, glm::value_ptr(lightpos));
//    glUniform3fv(uniform_light_sources_color_loc[node->lightID], 1, glm::value_ptr(node->lightColor));
//    fur_shader->activate();
//    glUniform3fv(fur_uniform_light_sources_position_loc[node->lightID], 1, glm::value_ptr(lightpos));
//    glUniform3fv(fur_uniform_light_sources_color_loc[node->lightID], 1, glm::value_ptr(node->lightColor));
}
