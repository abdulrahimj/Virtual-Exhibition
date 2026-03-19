#version 430 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;

out vec3 fragPos;
out vec3 fragNormal;
out vec2 fragTexCoord;

void main() {
    vec4 worldPos = u_model * vec4(aPos, 1.0);
    fragPos = worldPos.xyz;
    fragNormal = mat3(transpose(inverse(u_model))) * aNormal;
    fragTexCoord = aTexCoord;
    gl_Position = u_proj * u_view * worldPos;
}