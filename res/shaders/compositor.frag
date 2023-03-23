#version 430 core

layout(binding = 0) uniform sampler2D first_pass;

in vec2 texcoord;

out vec4 color;

void main() {
    vec4 first_color = texture(first_pass, 0.5*texcoord.xy + 0.5);
    color = first_color;
}