#version 330 core
layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec3 vNormal;
uniform mat4 uMVP;
uniform vec4 uColor;
out vec3 fNormal;
out vec4 fColor;
void main() {
    gl_Position = uMVP * vec4(vPosition, 1.0);
    fNormal = vNormal;
    fColor = uColor;
}
