#version 430 core

in layout(location = 0) vec3 position;
in layout(location = 1) vec3 normal_in;
in layout(location = 2) vec2 uv_in;
in layout(location = 3) vec3 tangent_in;

uniform layout(location = 3) mat4 MVP;

out layout(location = 0) vec3 uv_out;


void main()
{
    uv_out = -position;
    gl_Position = MVP * vec4(position, 1.0f);
    gl_Position = gl_Position.xyww;
}
