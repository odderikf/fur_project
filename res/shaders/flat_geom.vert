#version 430 core

in layout(location = 0) vec3 position;
in layout(location = 2) vec2 uv_in;

uniform layout(location = 3) mat4 MVP;

out layout(location = 1) vec2 uv_out;

void main()
{
    uv_out = uv_in;
    gl_Position = MVP * vec4(position, 1.0f);
}
