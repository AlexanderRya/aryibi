#version 460

layout (location = 0) in vec3 iPos;
layout (location = 1) in vec2 iTexCoords;

layout (location = 0) out VS_OUT {
    vec3 FragPos;
    vec2 TexCoords;
    vec4 FragPosLightSpace;
} vs_out;

layout (set = 0, binding = 0) uniform UniformData {
    mat4 projection;
    mat4 view;
};

layout (set = 0, binding = 1) buffer readonly Transforms {
    mat4[] transforms;
};

layout (set = 0, binding = 2) buffer readonly LightSpaceMats {
    mat4[] light_mats;
};

layout (push_constant) uniform Constants {
    uint transform_index;
    uint light_mat_index;
};

void main() {
    mat4 model = transforms[transform_index];
    vs_out.FragPos = vec3(model * vec4(iPos, 1.0));
    vs_out.TexCoords = iTexCoords;
    vs_out.FragPosLightSpace = light_mats[light_mat_index] * vec4(vs_out.FragPos, 1.0);
    gl_Position = projection * view * vec4(vs_out.FragPos, 1.0);
}