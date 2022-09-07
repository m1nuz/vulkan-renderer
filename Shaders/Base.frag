#version 450

layout(location = 0, index = 0) out vec4 fragcolor;

layout(location = 0) in VS_out {
    vec3 color;
}
fs_in;

void main() {
    fragcolor = vec4(fs_in.color, 1.0);
}