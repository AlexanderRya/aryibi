#version 460

layout (location = 0) in vec2 TexCoords;

layout (set = 1, binding = 0) uniform sampler2D tile;

void main() {
    if (texture(tile, TexCoords).a == 0) {
        gl_FragDepth = 1;
        return;
    }

    gl_FragDepth = gl_FragCoord.z;
}