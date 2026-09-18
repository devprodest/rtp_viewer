#version 330 core
uniform sampler2D tex;
uniform int effective_bits;
in vec2 TexCoord;
out vec4 FragColor;

void main()
{
    float val = texture(tex, TexCoord).r;
    FragColor = vec4(val, val, val, 1.0);
}
