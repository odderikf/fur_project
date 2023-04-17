#version 430 core

struct PointLightSource {
    vec3 position;
    vec3 color;
};

#define point_light_sources_len 4
in layout(location = 0) vec3 normal_in;
in layout(location = 1) vec2 uv_in;
in layout(location = 2) vec3 world_pos;
in layout(location = 3) vec3 tangent_in;
in layout(location = 4) float layer_dist;
in layout(location = 5) float alpha;


uniform layout(location = 1) mat3 normal_matrix;
uniform layout(location = 2) vec3 camera_pos;
uniform PointLightSource point_light_sources[point_light_sources_len];

layout(binding = 0) uniform sampler2D tex;
layout(binding = 1) uniform sampler2D normal_map;
layout(binding = 2) uniform sampler2D roughness_map;
layout(binding = 4) uniform sampler2D turbulence;

layout (location = 0) out vec4 modulation;
layout (location = 1) out vec4 accumulation;
layout (location = 2) out float revealage;

float rand(vec2 co) { return fract(sin(dot(co.xy, vec2(12.9898,78.233))) * 43758.5453); }
float dither(vec2 uv) { return (rand(uv)*2.0-1.0) / 256.0; }

float attenuation(float distance){
    return 1 + 0.007*distance + 0.00013*distance*distance;
}

void main()
{
    vec4 color;
    vec3 mat_diff = vec3(1.,1.,1.);
    vec3 mat_spec = vec3(1.,1.,1.);
    vec3 normal = normalize(normal_in);

    // get the texture color.
    vec4 frag_color = texture(tex, uv_in);
    // Find strand point visibility, turbulence texture gives the fur strands.
    color.a = frag_color.a * alpha;

    // hardcoded roughness location in texture
    vec2 fin_roughness_uv = vec2(0.1,0.1);
    float roughness = texture(roughness_map, fin_roughness_uv).x;
    float mat_shine = (5.f/(roughness*roughness));

    // base ambient intensity
    vec3 ambient_intensity = vec3(0.25, 0.25, 0.35);
    // how much of a lightsource's power to add as ambient
    float light_ambiance = 0.75;
    // sums of lighting and reflective lighting
    vec3 intensity = ambient_intensity;
    vec3 reflective_intensity = vec3(0);

    // per pointlight lighting
    for (int i = 0; i < point_light_sources_len; ++i){
        vec3 light_dir = point_light_sources[i].position - world_pos.xyz;
        float light_dist = length(light_dir);

        light_dir = normalize(light_dir);
        vec3 light_intensity = point_light_sources[i].color / attenuation(light_dist);
        intensity += light_ambiance*light_intensity; // make lights add local ambient

        vec3 cam_dir = normalize(camera_pos - world_pos);

        vec3 diff_intensity = light_intensity;
        vec3 spec_intensity = light_intensity;

        // phong model math
        float lambertian = max(0., dot(light_dir, normal));
        vec3 diffuse_term = mat_diff * lambertian * diff_intensity;

        vec3 reflected = reflect(-light_dir, normal);
        float spec_dot = max(0, dot(reflected, cam_dir));
        spec_dot = pow(spec_dot, mat_shine);
        vec3 specular_term = mat_spec * spec_dot * spec_intensity;

        // add a little specular to colored intensity,
        // add most of it to reflective intensity
        // this lets black objects be shiny, f.ex.
        intensity += (diffuse_term + 0.1*specular_term);
        reflective_intensity += 0.9*specular_term;
    }

    intensity.r = min(1., intensity.r);
    intensity.g = min(1., intensity.g);
    intensity.b = min(1., intensity.b);
    float root_darkening = (0.9 + 0.2*layer_dist*layer_dist);
    color.rgb = root_darkening * intensity * frag_color.rgb + reflective_intensity + dither(uv_in);

    accumulation = color;

    float weight = 4e3*pow(1-gl_FragCoord.z, 3.);
    weight = clamp(weight, 3e-4, 1.);

    accumulation.rgb *= color.a;
    accumulation *= weight;
    revealage = color.a;
    modulation.rgb = accumulation.a * (1 - frag_color.rgb);
    modulation.a = color.a;
}