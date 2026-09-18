#version 330 core
uniform sampler2D tex;
in vec2 TexCoord;
out vec4 FragColor;

// Преобразование YCbCr -> RGB (BT.601)
void main()
{
    vec3 ycbcr = texture(tex, TexCoord).rgb;
    float Y  = ycbcr.r;
    float Cb = ycbcr.g - 0.5;
    float Cr = ycbcr.b - 0.5;
    float R = Y + 1.402 * Cr;
    float G = Y - 0.344136 * Cb - 0.714136 * Cr;
    float B = Y + 1.772 * Cb;
    FragColor = vec4(R, G, B, 1.0);
}
