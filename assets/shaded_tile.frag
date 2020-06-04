#version 430 core
#define MAX_DIRECTIONAL_LIGHTS 5
#define MAX_POINT_LIGHTS 20

in vec2 TexCoords;

uniform sampler2D tile;// Name hardcoded in renderer_impl_x.cpp. TODO: Add constexpr variable in separate file
uniform sampler2D shadow;// Name hardcoded in renderer_impl_x.cpp. TODO: Add constexpr variable in separate file

layout(std140) struct DirectionalLight {
/// Color RGB is color, alpha is light intensity
/// lightAtlasPos XY is atlas pos, Z is tile size
                            // base // aligned
    vec4 color;             // 16   // 0
    mat4 lightSpaceMatrix;  // 64   // 16
    vec3 lightAtlasPos;     // 16   // 80
                                    // 96
};

layout(std140) struct PointLight {
/// Color RGB is color, alpha is light intensity
/// lightAtlasPos XY is atlas pos, Z is tile size
                            // base // aligned
    vec4 color;             // 16   // 0
    float radius;           // 4    // 16
    mat4 lightSpaceMatrix;  // 64   // 32
    vec3 lightAtlasPos;     // 16   // 96
                                    // 112
};

layout(std140, binding = 5) uniform Lights {
                                                                // base // aligned
    DirectionalLight directionalLights[MAX_DIRECTIONAL_LIGHTS]; // 480  // 0
    uint directionalLightCount;                                 // 4    // 480
    PointLight pointLights[MAX_POINT_LIGHTS];                   // 2240 // 496
    uint pointLightCount;                                       // 4    // 2736
                                                                        // 2752
} lights;

in VS_OUT {
    vec3 FragPos;
    vec2 TexCoords;
} fs_in;

out vec4 FragColor;

float ShadowCalculation(vec2 lightAtlasPos, float lightAtlasSize, vec4 fragPosLightSpace)
{
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
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadow, projCoords2D + vec2(x, y) * texelSize).r;
            f_shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    f_shadow /= 9.0;
    return f_shadow;
}

void main() {

    float f_shadow = 1.0;
    for (int directional_i = 0; directional_i < lights.directionalLightCount; ++directional_i) {
        vec4 FragPosLightSpace = lights.directionalLights[directional_i].lightSpaceMatrix * vec4(fs_in.FragPos, 1.0);
        f_shadow -= 1.0 - ShadowCalculation(lights.directionalLights[directional_i].lightAtlasPos.xy, lights.directionalLights[directional_i].lightAtlasPos.z, FragPosLightSpace);
    }
    for (int point_i = 0; point_i < lights.pointLightCount; ++point_i) {
        vec4 FragPosLightSpace = lights.pointLights[point_i].lightSpaceMatrix * vec4(fs_in.FragPos, 1.0);
        f_shadow -= 1.0 - ShadowCalculation(lights.pointLights[point_i].lightAtlasPos.xy, lights.pointLights[point_i].lightAtlasPos.z, FragPosLightSpace);
    }
    f_shadow = max(f_shadow, 0.0);
    float shadow_force = 0.2;
    FragColor = texture(tile, fs_in.TexCoords).rgba - vec4(vec3(f_shadow), 0) * shadow_force;
    if (texture(tile, fs_in.TexCoords).a == 0) { gl_FragDepth = 99999; return; }

    gl_FragDepth = gl_FragCoord.z;
}