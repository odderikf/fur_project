#version 430 core

in layout(location = 1) vec2 uv;

layout(binding = 0) uniform sampler2D tex;

layout(location = 0) out vec4 color;

void main()
{
    color = texture(tex, uv);
}