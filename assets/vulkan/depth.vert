#version 460
layout (location = 0) in vec3 iPos;
layout (location = 1) in vec2 iTexCoords;

layout (location = 0) out vec2 TexCoords;

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
    TexCoords = iTexCoords;
    gl_Position = light_mats[light_mat_index] * model * vec4(iPos, 1.0);
}