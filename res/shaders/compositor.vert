#version 430 core

in layout(location = 0) vec3 position;
in layout(location = 1) vec3 normal_in;
in layout(location = 2) vec2 textureCoordinates_in;
in layout(location = 3) vec3 tangent_in;

uniform layout(location = 1) mat3 normal_matrix;
uniform layout(location = 3) mat4 MVP;
uniform layout(location = 4) mat4 model;

out vec2 texcoord;

void main()
{
    gl_Position = vec4(position, 1.0f);
    texcoord = position.xy;
}
