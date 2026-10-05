#version 330 core
uniform sampler2D tex;
uniform int effective_bits;
in vec2 TexCoord;
out vec4 FragColor;

void main()
{
    float val = texture(tex, TexCoord).r;
	float maxv = exp2(float(effective_bits)) - 1.0;
	val = clamp(val * (65535.0 / maxv), 0.0, 1.0);
    FragColor = vec4(val, val, val, 1.0);
}
