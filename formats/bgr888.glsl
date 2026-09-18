#version 330 core
uniform sampler2D tex;
in vec2 TexCoord;
out vec4 FragColor;

void main()
{
    vec3 c = texture(tex, TexCoord).rgb;
    FragColor = vec4(c.b, c.g, c.r, 1.0);
}
