#version 430 core

struct PointLightSource {
    vec3 position;
    vec3 color;
};

in layout(location = 0) vec3 uv;

layout(binding = 0) uniform samplerCube skybox;

layout(location = 0) out vec4 color;

void main()
{
    color.rgba = texture(skybox, uv);
}