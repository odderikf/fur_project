#include <iostream>
#include "glfont.hpp"

Mesh generateTextGeometryBuffer(std::string text, float characterHeightOverWidth, float totalTextWidth) {
    float characterWidth = totalTextWidth / float(text.length());
    float characterHeight = characterHeightOverWidth * characterWidth;

    unsigned int vertexCount = 4 * text.length();
    unsigned int indexCount = 6 * text.length();

    const float textureCharWidth = 29;
    const float textureTotalWidth = 3712;

    Mesh mesh;

    mesh.vertices.resize(vertexCount);
    mesh.indices.resize(indexCount);
    mesh.textureCoordinates.resize(vertexCount);

    for(unsigned int i = 0; i < text.length(); i++)
    {
        float baseXCoordinate = float(i) * characterWidth;
        float baseUCoordinate = text.at(i) * textureCharWidth; // todo prescale?

        mesh.vertices.at(4 * i + 0) = {baseXCoordinate, 0, 0};
        mesh.vertices.at(4 * i + 1) = {baseXCoordinate + characterWidth, 0, 0};
        mesh.vertices.at(4 * i + 2) = {baseXCoordinate + characterWidth, characterHeight, 0};
        mesh.vertices.at(4 * i + 3) = {baseXCoordinate, characterHeight, 0};

        mesh.textureCoordinates.at(4 * i + 0) = {baseUCoordinate / textureTotalWidth, 0};
        mesh.textureCoordinates.at(4 * i + 1) = {(baseUCoordinate + textureCharWidth) / textureTotalWidth, 0};
        mesh.textureCoordinates.at(4 * i + 2) = {(baseUCoordinate + textureCharWidth) / textureTotalWidth, 1};
        mesh.textureCoordinates.at(4 * i + 3) = {baseUCoordinate / textureTotalWidth, 1};

        mesh.indices.at(6 * i + 0) = 4 * i + 0;
        mesh.indices.at(6 * i + 1) = 4 * i + 1;
        mesh.indices.at(6 * i + 2) = 4 * i + 2;
        mesh.indices.at(6 * i + 3) = 4 * i + 0;
        mesh.indices.at(6 * i + 4) = 4 * i + 2;
        mesh.indices.at(6 * i + 5) = 4 * i + 3;
    }

    return mesh;
}