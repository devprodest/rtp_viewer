#version 330 core
uniform sampler2D tex;
in vec2 TexCoord;
out vec4 FragColor;

void main()
{
    float gray = texture(tex, TexCoord).r;
    FragColor = vec4(gray, gray, gray, 1.0);
}
