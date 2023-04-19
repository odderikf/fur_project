#version 430 core

#define nlayers 20
#define nvertices 60 // 3*nlayers
layout(triangles) in;
layout(triangle_strip, max_vertices = nvertices) out;

in layout(location = 0) vec3 normal_in[3];
in layout(location = 1) vec2 uv_in[3];
in layout(location = 3) vec3 tangent_in[3];

uniform layout(location = 1) mat3 normal_matrix;
uniform layout(location = 3) mat4 MVP;
uniform layout(location = 4) mat4 model;
uniform layout(location = 7) vec3 wind;
uniform layout(location = 8) float fur_strand_length;
uniform layout(location = 9) float shells;

layout(binding = 3) uniform sampler2D fur_texture;

out layout(location = 0) vec3 normal_out;
out layout(location = 1) vec2 uv_out;
out layout(location = 2) vec3 world_pos_out;
out layout(location = 3) vec3 tangent_out;
out layout(location = 4) float layer_dist;

void main(){
    vec4 fur_texels[3];
    fur_texels[0] = texture(fur_texture, uv_in[0]);
    fur_texels[1] = texture(fur_texture, uv_in[1]);
    fur_texels[2] = texture(fur_texture, uv_in[2]);

    if (fur_texels[0].a < 0.02 && fur_texels[1].a < 0.02 && fur_texels[2].a < 0.02) {
        return; // fur too short to bother
    }

    vec3 normals_out[3];
    vec3 tangents_out[3];
    mat3 TBNs[3];
    vec3 fur_dirs[3];
    for(int i = 0; i < 3; ++i){
        normals_out[i] = normalize(normal_matrix * normal_in[i]);
        tangents_out[i] = normalize(normal_matrix * tangent_in[i]);
        // translate tangent-space fur direction to model-space
        TBNs[i] = mat3(
            tangent_in[i],
            cross(normal_in[i], tangent_in[i]),
            normal_in[i]
        );
        // blue: normal to surface, G: up, R: right
        fur_dirs[i] = fur_texels[i].xyz * 2 - 1; // tangent space lookup
        fur_dirs[i] = TBNs[i] * fur_dirs[i]; // transform to model space
        fur_dirs[i] += normal_matrix * wind; // sway
        fur_dirs[i] = normalize(fur_dirs[i]);
    }

    // possible optimization: dynamically reduce shellcount.
    // currently shelved because it affects blending
    /*
    float camdist0 = length(gl_in[0].gl_Position.xyz - campos);
    float camdist1 = length(gl_in[1].gl_Position.xyz - campos);
    float camdist2 = length(gl_in[2].gl_Position.xyz - campos);
    float camdist = min(min(camdist0, camdist1), camdist2);
    int stepsize = 1;// + int(camdist/(100*fur_strand_length));
    */

    for(int i = 0; i < nlayers*shells; i = ++i){
        float norm_i = float(i)/nlayers;
        float distance = fur_strand_length * norm_i;

        for(int j = 0; j < 3; ++j){
            normal_out = normals_out[j];
            tangent_out = tangents_out[j];
            uv_out = uv_in[j];

            vec4 displacement = fur_texels[j].a * distance * vec4(fur_dirs[j], 0);

            gl_Position = gl_in[j].gl_Position + displacement;
            world_pos_out = (model*gl_Position).xyz;
            gl_Position = MVP * gl_Position;
            layer_dist = norm_i;

            EmitVertex();
        }
        EndPrimitive();
    }
}