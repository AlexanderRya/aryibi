#version 460

layout (location = 0) in VS_OUT {
    vec3 FragPos;
    vec2 TexCoords;
} vs_out;

layout (set = 1, binding = 0) uniform sampler2D tile;

layout (location = 0) out vec4 FragColor;

void main() {
    FragColor = texture(tile, vs_out.TexCoords).rgba;
    if (texture(tile, vs_out.TexCoords).a == 0) { gl_FragDepth = 1; return; }

    gl_FragDepth = gl_FragCoord.z;
}