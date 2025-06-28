#version 460 core

in vec2 TexCoords;

out vec4 FragColor;

uniform sampler2D sceneTexture;

void main()
{
    FragColor = vec4(texture(sceneTexture, TexCoords).rgb, 1.0f);
}