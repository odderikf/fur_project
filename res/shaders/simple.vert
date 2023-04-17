#version 430 core

in layout(location = 0) vec3 position;
in layout(location = 1) vec3 normal_in;
in layout(location = 2) vec2 uv_in;
in layout(location = 3) vec3 tangent_in;

uniform layout(location = 1) mat3 normal_matrix;
uniform layout(location = 3) mat4 MVP;
uniform layout(location = 4) mat4 model;

out layout(location = 0) vec3 normal_out;
out layout(location = 1) vec2 uv_out;
out layout(location = 2) vec3 world_pos;
out layout(location = 3) vec3 tangent_out;

void main()
{
    normal_out = normal_matrix * normal_in;
    normal_out = normalize(normal_out);
    tangent_out = normal_matrix * tangent_in;
    tangent_out = normalize(tangent_out);
    uv_out = uv_in;
    gl_Position = MVP * vec4(position, 1.0f);
    world_pos = (model * vec4(position, 1.0f)).xyz;
}
