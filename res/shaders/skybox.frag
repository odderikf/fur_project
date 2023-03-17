#version 430 core

struct PointLightSource {
    vec3 position;
    vec3 color;
};

#define point_light_sources_len 3
in layout(location = 0) vec3 normal_in;
in layout(location = 1) vec2 textureCoordinates;
in layout(location = 2) vec3 world_pos;
in layout(location = 3) vec3 tangent_in;

uniform layout(location = 1) mat3 normal_matrix;
uniform layout(location = 2) vec3 campos;
uniform PointLightSource point_light_sources[point_light_sources_len];
uniform layout(location = 5) vec3 ball_position;
bool enable_nmap = false;

layout(binding = 0) uniform sampler2D tex;
layout(binding = 1) uniform sampler2D normal_map;
layout(binding = 2) uniform sampler2D roughness_map;
layout(binding = 3) uniform samplerCube skybox;

out vec4 color;

void main()
{
    color.rgba = texture(skybox, -world_pos);
}