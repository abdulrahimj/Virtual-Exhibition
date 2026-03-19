#version 430 core

in vec3 fragPos;
in vec3 fragNormal;
in vec2 fragTexCoord;

uniform sampler2D u_texture;
uniform vec3 u_lightPos;
uniform vec3 u_viewPos;
uniform vec3 u_lightColor;
uniform vec3 u_objectColor;
uniform bool u_useTexture;
uniform float u_ambientStrength;

out vec4 FragColor;

void main() {
    // Ambient
    vec3 ambient = u_ambientStrength * u_lightColor;

    // Diffuse
    vec3 norm = normalize(fragNormal);
    vec3 lightDir = normalize(u_lightPos - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * u_lightColor;

    // Specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(u_viewPos - fragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * u_lightColor;

    vec3 baseColor;
    if (u_useTexture) {
        baseColor = texture(u_texture, fragTexCoord).rgb;
    } else {
        baseColor = u_objectColor;
    }

    vec3 result = (ambient + diffuse + specular) * baseColor;
    FragColor = vec4(result, 1.0);
}