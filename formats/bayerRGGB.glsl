#version 330 core
uniform sampler2D tex;
uniform int image_width;
uniform int image_height;
in vec2 TexCoord;
out vec4 FragColor;

void main()
{
    vec2 ts = 1.0 / vec2(image_width, image_height);
    vec2 c = TexCoord;
    float c00 = texture(tex, c + vec2(-0.5, -0.5) * ts).r;
    float c10 = texture(tex, c + vec2( 0.5, -0.5) * ts).r;
    float c01 = texture(tex, c + vec2(-0.5,  0.5) * ts).r;
    float c11 = texture(tex, c + vec2( 0.5,  0.5) * ts).r;
    float R = c00;
    float G = (c10 + c01) * 0.5;
    float B = c11;
    FragColor = vec4(R, G, B, 1.0);
}
