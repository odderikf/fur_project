#version 430 core

struct PointLightSource {
    vec3 position;
    vec3 color;
};

#define point_light_sources_len 4
in layout(location = 0) vec3 normal_in;
in layout(location = 1) vec2 textureCoordinates;
in layout(location = 2) vec3 world_pos;
in layout(location = 3) vec3 tangent_in;

uniform layout(location = 1) mat3 normal_matrix;
uniform layout(location = 2) vec3 campos;
uniform PointLightSource point_light_sources[point_light_sources_len];
uniform layout(location = 5) vec3 ball_position;
uniform layout(location = 6) bool enable_nmap;

layout(binding = 0) uniform sampler2D tex;
layout(binding = 1) uniform sampler2D normal_map;
layout(binding = 2) uniform sampler2D roughness_map;


layout (location = 0) out vec4 accumulation;
layout (location = 1) out float revealage;

float rand(vec2 co) { return fract(sin(dot(co.xy, vec2(12.9898,78.233))) * 43758.5453); }
float dither(vec2 uv) { return (rand(uv)*2.0-1.0) / 256.0; }

float attenuation(float distance){
    return 1 + 0.007*distance + 0.00013*distance*distance;
}
vec3 reject(vec3 from, vec3 onto) {
    return from - onto*dot(from, onto)/dot(onto, onto);
}

float sigmoid(float x){
    return 1 / (1 + exp(-10*(x-0.5)));
}

void main()
{
    vec4 color;
    // set defaults
    float mat_shine = 16;
    vec3 normal = normalize(normal_in);
    vec3 mat_diff = vec3(1.,1.,1.);
    vec3 mat_spec = vec3(1.,1.,1.);
    vec4 frag_color = vec4(1.,1.,1., 1.);
    // get the texture color
    frag_color = texture(tex, textureCoordinates);
    if (frag_color.a == 0) discard;
    // if this model uses normal maps
    if (enable_nmap) { // todo rename variable?

        // get the roughness, how unshiny it is
        float roughness = texture(roughness_map, textureCoordinates).x;
        mat_shine = (5.f/(roughness*roughness));

        // find transform from tangent-space to world-space
        vec3 tangent = normalize(tangent_in);
        vec3 bitangent = cross(normal, tangent);
        mat3 TBN = mat3(
            tangent,
            bitangent,
            normal
        );

        // find world-space normal from normal map in tangent-space
        normal = texture(normal_map, textureCoordinates).xyz * 2 - 1;
        normal = normalize(normal);
        normal = TBN * normal;
    }


    // find ball shadow data
    float ball_radius = 3.;
    vec3 ball_dir = ball_position - world_pos.xyz;
    float ball_dist = length(ball_dir);

    // inner and outer border for the softer shadow region,
    // shadows are softer if ball is far from shaded object
    float soft_shading_factor = 1 + 0.008 + 0.04 *log(ball_dist);
    float ball_soft_outer = ball_radius * soft_shading_factor;
    float ball_soft_inner = ball_radius / soft_shading_factor;

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

        // how far from centre of mass of the ball the light ray passes
        vec3 ball_reject = reject(ball_dir, light_dir);
        float ball_rejectance = length(ball_reject);
        // scale to be 0 at soft shadows inner edge, 1 at outer edge
        ball_rejectance = (ball_rejectance-ball_soft_inner)/(ball_soft_outer-ball_soft_inner);
        // sigmoid for better fade, and for soft-clipping the range to 0-1
        ball_rejectance = sigmoid(ball_rejectance);
        // if light is closer than ball, or the ball is in the opposite direction (angle more than 90)
        if (light_dist < ball_dist-ball_radius || dot(ball_dir, light_dir) < 0) {
            ball_rejectance = 1;
        }
        // todo ball no longer there
        ball_rejectance = 1;

        light_dir = normalize(light_dir);
        vec3 light_intensity = point_light_sources[i].color / attenuation(light_dist);
        intensity += light_ambiance*light_intensity; // make lights add local ambient

        vec3 cam_dir = normalize(campos - world_pos);

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
        intensity += ball_rejectance*(diffuse_term + 0.1*specular_term);
        reflective_intensity += ball_rejectance*0.9*specular_term;
    }

    intensity.r = min(1., intensity.r);
    intensity.g = min(1., intensity.g);
    intensity.b = min(1., intensity.b);
    color.a = frag_color.a;
    color.rgb = intensity * frag_color.xyz + reflective_intensity + dither(textureCoordinates);
    accumulation = color;

    // comments and formula from https://jcgt.org/published/0002/02/09/ presentation ppt
    float color_resist = 0.05; // Increase if low-coverage foreground transparents are affecting background transparent color.
    float range_adjust = 0.3; // Range adjustment to avoid saturating at the clamp bounds.
    float zrange = 1000; //Depth range over which significant ordering discrimination is required. Here, 500 camera space units. Decrease if high-opacity surfaces seem “too transparent”, increase if distant transparents are blending together too much.
    float ordering_strength = 32.0; // Ordering strength. Increase if background is showing through foreground too much.
    float clampmin = 1e-3; // Min and max values expected to accumulate without under/overflow.
    float clampmax = 3e4; // Here, tuned for 50 surfaces at float16 precision.
    float epsilon = 1e-5; // don't divide by zero
    float weight = pow(color.a, color_resist);
    weight *= clamp(range_adjust / (epsilon + pow(gl_FragCoord.z / zrange, ordering_strength)), clampmin, clampmax);

    accumulation.rgb *= color.a;
    accumulation *= weight;
    revealage = color.a;
}