#version 430 core

#define nlayers 10
#define nvertices 20 // 2*nlayers
layout(triangles) in;
layout(triangle_strip, max_vertices = nvertices) out;

in layout(location = 0) vec3 normal_in[3];
in layout(location = 1) vec2 textureCoordinates_in[3];
in layout(location = 3) vec3 tangent_in[3];

uniform layout(location = 1) mat3 normal_matrix;
uniform layout(location = 2) vec3 campos;
uniform layout(location = 3) mat4 MVP;
uniform layout(location = 4) mat4 model;
uniform layout(location = 8) float fur_strand_length;

layout(binding = 3) uniform sampler2D fur_texture;

out layout(location = 0) vec3 normal_out;
out layout(location = 1) vec2 textureCoordinates_out;
out layout(location = 2) vec3 world_pos_out;
out layout(location = 3) vec3 tangent_out;
out layout(location = 4) float alpha;

float rand(vec2 co) { return fract(sin(dot(co.xy, vec2(12.9898,78.233))) * 43758.5453); }
float dither(vec2 uv) { return (rand(uv)*2.0-1.0) / 256.0; }

void main(){
    int edges[6] = {0,1, 1,2, 2,0};

    vec4[3] view_space_pos;

    vec4 fur_texels[3];
    fur_texels[0] = texture(fur_texture, textureCoordinates_in[0]);
    fur_texels[1] = texture(fur_texture, textureCoordinates_in[1]);
    fur_texels[2] = texture(fur_texture, textureCoordinates_in[2]);

    mat3 TBNs[3];
    vec3 fur_dirs[3];
    for(int i = 0; i < 3; ++i){
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

    vec4 left;
    vec4 right;
    for (int e = 0; e < 6; e += 2){


        int ileft = edges[e];
        int iright = edges[e+1];
        left = gl_in[ileft].gl_Position;
        right = gl_in[iright].gl_Position;


        vec4 centre = left + right;
        centre /= 2.;
        vec4 view_space_centre = MVP * centre;
        vec4 world_space_centre = model * centre;

        // find whether this is a silhouette edge
        vec3 edge_normal = normal_in[ileft] +  normal_in[iright];
        edge_normal = normalize(edge_normal);
        vec3 world_normal = normal_matrix * edge_normal;
        world_normal = normalize(world_normal);
        vec3 to_camera = -campos - world_space_centre.xyz;
        vec3 to_camera_dir = normalize(to_camera);

        float camdot = dot(world_normal, to_camera_dir);
        float abscamdot = abs(camdot);
        float silhouette_tolerance = 0.6;
        if(abscamdot < silhouette_tolerance){
            float fadeout = (silhouette_tolerance-abscamdot);
            fadeout *= fadeout;

            float texture_strand_length = fur_texels[ileft].a + fur_texels[iright].a;
            texture_strand_length /= 2;

            // generate billboard
            float local_strand_length = texture_strand_length * fur_strand_length;
            vec4 displacement_dir = vec4(fur_dirs[ileft]+fur_dirs[iright], 0);
            displacement_dir = normalize(displacement_dir);
            vec4 positions[nvertices];
            // culling disabled so don't need to worry about winding order
            for(int i = 0; i < nlayers; ++i){
                float norm_i = float(i)/nlayers;
                float distance = local_strand_length * norm_i;
                alpha = 1. - norm_i*sqrt(norm_i);
                alpha *= fadeout; // fade in silhouette
                tangent_out = vec3(0); // not used;
                normal_out = to_camera_dir;

                textureCoordinates_out = vec2(0, norm_i);
                gl_Position = left + distance * displacement_dir;
                world_pos_out = (model*gl_Position).xyz;
                gl_Position = MVP * gl_Position;
                EmitVertex();

                textureCoordinates_out = vec2(1, norm_i);
                gl_Position = right + distance * displacement_dir;
                world_pos_out = (model*gl_Position).xyz;
                gl_Position = MVP * gl_Position;
                EmitVertex();
            }
            EndPrimitive();
        }
    }

}