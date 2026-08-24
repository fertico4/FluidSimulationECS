#version 430 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aInstancePos;

uniform mat4 u_Projection;      // Send camera matrix through C++
uniform mat4 u_View;

out vec2 vLocalPos;

void main() {
    vLocalPos = aPos;

    vec4 viewPos = u_View * vec4(aInstancePos, 1.f);

    float particleScale = 0.25;
    viewPos.xy += aPos * particleScale;
    gl_Position = u_Projection * viewPos;
}