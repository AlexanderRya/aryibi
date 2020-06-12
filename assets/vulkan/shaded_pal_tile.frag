#version 460

layout (location = 0) in VS_OUT {
    vec3 FragPos;
    vec2 TexCoords;
    vec4 FragPosLightSpace;
} vs_out;

layout (location = 0) out vec4 FragColor;

layout (set = 1, binding = 0) uniform sampler2D shadow;
layout (set = 1, binding = 1) uniform sampler2D palette;

layout (set = 2, binding = 0) uniform sampler2D tile;

float ShadowCalculation(vec4 fragPosLightSpace) {
    // perform perspective divide (not really neccesary for ortho projection, but whatever)
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    // projCoords = vec3(projCoords.x, 1.0 - projCoords.y, projCoords.z);
    float closestDepth = texture(shadow, projCoords.xy).r;
    float currentDepth = projCoords.z;
    float bias = 0.0005;

    if (currentDepth == 0) return 0.0;

    float f_shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadow, 0);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadow, projCoords.xy + vec2(x, y) * texelSize).r;
            f_shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    f_shadow /= 9.0;
    return f_shadow;
}

void main() {
    float f_shadow = ShadowCalculation(vs_out.FragPosLightSpace);
    vec4 original_color = texture(tile, vs_out.TexCoords);

    // 0 is transparent
    if (original_color.r == 0) FragColor = vec4(0);
    else FragColor = texelFetch(palette, max(ivec2(original_color.rg * 255.0 - vec2(f_shadow + 1, 0)), ivec2(0,0)), 0);
    if (original_color.a == 0) { gl_FragDepth = 1.0; return; }

    gl_FragDepth = gl_FragCoord.z;
}