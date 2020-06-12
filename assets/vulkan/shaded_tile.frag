#version 460
#define MAX_DIRECTIONAL_LIGHTS 5
#define MAX_POINT_LIGHTS 20

struct DirectionalLight {
    vec4 color;
    mat4 lightSpaceMatrix;
    vec3 lightAtlasPos;
};

struct PointLight {
    vec4 color;
    mat4 lightSpaceMatrix;
    vec3 lightAtlasPos;
    float radius;
};

layout (location = 0) in VS_OUT {
    vec3 FragPos;
    vec2 TexCoords;
} vs_out;

layout (location = 0) out vec4 FragColor;

layout (set = 1, binding = 0) uniform sampler2D shadow;
layout (set = 1, binding = 1) uniform sampler2D palette; // Unused

layout (set = 2, binding = 0) uniform sampler2D tile;

layout (std140, set = 3, binding = 0) uniform Lights {
    DirectionalLight directionalLights[MAX_DIRECTIONAL_LIGHTS];
    float _pad0;
    PointLight pointLights[MAX_POINT_LIGHTS];
    vec3 ambientLightColor;
    uint pointLightCount;
    uint directionalLightCount;
    vec3 _pad1;
} lights;

float ShadowCalculation(vec2 lightAtlasPos, float lightAtlasSize, vec4 fragPosLightSpace) {
    // perform perspective divide (not really neccesary for ortho projection, but whatever)
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    vec2 projCoords2D = lightAtlasPos + projCoords.xy * lightAtlasSize;
    float currentDepth = projCoords.z;
    float bias = 0.0005;

    if (currentDepth == 0) return 0.0;

    float f_shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadow, 0);
    for (int x = -2; x <= 2; ++x)
    {
        for (int y = -2; y <= 2; ++y)
        {
            float pcfDepth = texture(shadow, projCoords2D + vec2(x, y) * texelSize * 5.0).r;
            f_shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    f_shadow /= 25.0;
    return f_shadow;
}

void main() {
    vec3 light = lights.ambientLightColor;
    for (int directional_i = 0; directional_i < lights.directionalLightCount; ++directional_i) {
        vec4 FragPosLightSpace = lights.directionalLights[directional_i].lightSpaceMatrix * vec4(vs_out.FragPos, 1.0);
        vec3 light_forward = normalize(lights.directionalLights[directional_i].lightSpaceMatrix[2].xyz);
        // Assume our normal is always facing the camera
        vec3 this_normal = vec3(0, 0, 1);
        float light_strength = dot(this_normal, -light_forward) * lights.directionalLights[directional_i].color.a;
        light_strength = max(light_strength, 0.0);
        light += light_strength * lights.directionalLights[directional_i].color.rgb *
        (1.0 - ShadowCalculation(lights.directionalLights[directional_i].lightAtlasPos.xy,
        lights.directionalLights[directional_i].lightAtlasPos.z, FragPosLightSpace));
    }
    for (int point_i = 0; point_i < lights.pointLightCount; ++point_i) {
        vec4 FragPosLightSpace = lights.pointLights[point_i].lightSpaceMatrix * vec4(vs_out.FragPos, 1.0);
        vec3 light_pos = lights.pointLights[point_i].lightSpaceMatrix[3].xyz;
        vec3 light_dir_vec = normalize(light_pos - vs_out.FragPos);
        // Assume our normal is always facing the camera
        vec3 this_normal = vec3(0, 0, 1);
        float light_strength = dot(this_normal, light_dir_vec) * lights.pointLights[point_i].color.a;
        light_strength *= min(1.0, (lights.pointLights[point_i].radius - distance(light_pos, vs_out.FragPos)) / lights.pointLights[point_i].radius);
        light_strength = max(light_strength, 0.0);
        light += light_strength * lights.pointLights[point_i].color.rgb *
        (1.0 - ShadowCalculation(lights.pointLights[point_i].lightAtlasPos.xy,
        lights.pointLights[point_i].lightAtlasPos.z, FragPosLightSpace));
    }
    FragColor = texture(tile, vs_out.TexCoords).rgba * vec4(light, 1.0);
    if (texture(tile, vs_out.TexCoords).a == 0) { gl_FragDepth = 1.0; return; }

    gl_FragDepth = gl_FragCoord.z;
}