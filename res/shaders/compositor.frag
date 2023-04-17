#version 430 core

layout(binding = 0) uniform sampler2D accum_tex;
layout(binding = 1) uniform sampler2D reveal_tex;

layout(location = 0) out vec4 color;

void main() {
    ivec2 coords = ivec2(gl_FragCoord.xy);
    float reveal = texelFetch(reveal_tex, coords, 0).r;
    if (reveal > 0.999) discard;
    vec4 accum = texelFetch(accum_tex, coords, 0);
    if (isinf(abs(accum.r)) || isinf(abs(accum.g)) || isinf(abs(accum.b))) {
        accum.rgb = vec3(accum.a);
    }
    float epsilon = 0.00001f;
    color.rgb = accum.rgb / max(accum.a, epsilon);
    color.a = 1-reveal;
}