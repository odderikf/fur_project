#version 430 core

#define nlayers 20
#define nvertices 60 // 3*nlayers
layout(triangles) in;
layout(triangle_strip, max_vertices = nvertices) out;

in layout(location = 0) vec3 normal_in[3];
in layout(location = 1) vec2 textureCoordinates_in[3];
in layout(location = 3) vec3 tangent_in[3];

uniform layout(location = 1) mat3 normal_matrix;
uniform layout(location = 3) mat4 MVP;
uniform layout(location = 4) mat4 model;

layout(binding = 3) uniform sampler2D fur_texture;

out layout(location = 0) vec3 normal_out;
out layout(location = 1) vec2 textureCoordinates_out;
out layout(location = 2) vec3 world_pos_out;
out layout(location = 3) vec3 tangent_out;
out layout(location = 4) float alpha;

float rand(vec2 co) { return fract(sin(dot(co.xy, vec2(12.9898,78.233))) * 43758.5453); }
float dither(vec2 uv) { return (rand(uv)*2.0-1.0) / 256.0; }

void main(){
    float fur_strand_length = 4;
    vec4 fur_texels[3];
    fur_texels[0] = texture(fur_texture, textureCoordinates_in[0]);
    fur_texels[1] = texture(fur_texture, textureCoordinates_in[1]);
    fur_texels[2] = texture(fur_texture, textureCoordinates_in[2]);

    if (fur_texels[0].a < 0.02 && fur_texels[1].a < 0.02 && fur_texels[2].a < 0.02) {
        return; // fur too short to bother
    }

    vec3 normals_out[3];
    normals_out[0] = normalize(normal_matrix * normal_in[0]);
    normals_out[1] = normalize(normal_matrix * normal_in[1]);
    normals_out[2] = normalize(normal_matrix * normal_in[2]);

    vec3 tangents_out[3];
    tangents_out[0] = normalize(normal_matrix * tangent_in[0]);
    tangents_out[1] = normalize(normal_matrix * tangent_in[1]);
    tangents_out[2] = normalize(normal_matrix * tangent_in[2]);

    // translate tangent-space fur direction to model-space
    mat3 TBNs[3];
    TBNs[0] = mat3(
        tangent_in[0],
        cross(normal_in[0], tangent_in[0]),
        normal_in[0]
    );
    TBNs[1] = mat3(
        tangent_in[1],
        cross(normal_in[1], tangent_in[1]),
        normal_in[1]
    );
    TBNs[2] = mat3(
        tangent_in[2],
        cross(normal_in[2], tangent_in[2]),
        normal_in[2]
    );

    vec3 fur_dirs[3];
    // blue: normal to surface, G: up, R: right
    fur_dirs[0] = fur_texels[0].xyz * 2 - 1; // tangent space lookup
    fur_dirs[0] = normalize(TBNs[0] * fur_dirs[0]); // transform to model space
    fur_dirs[1] = fur_texels[1].xyz * 2 - 1;
    fur_dirs[1] = normalize(TBNs[1] * fur_dirs[1]);
    fur_dirs[2] = fur_texels[2].xyz * 2 - 1;
    fur_dirs[2] = normalize(TBNs[2] * fur_dirs[2]);

    for(int i = 0; i < nlayers; ++i){
        float norm_i = float(i)/nlayers;
        float distance = fur_strand_length * norm_i;

        for(int j = 0; j < 3; ++j){
            normal_out = normals_out[j];
            tangent_out = tangents_out[j];
            textureCoordinates_out = textureCoordinates_in[j];

            vec4 displacement = fur_texels[j].a * distance * vec4(fur_dirs[j], 0);

            gl_Position = gl_in[j].gl_Position + displacement;
            world_pos_out = (model*gl_Position).xyz;
            gl_Position = MVP * gl_Position;
            alpha = 1. - norm_i*sqrt(norm_i);

            EmitVertex();
        }
        EndPrimitive();
    }
}