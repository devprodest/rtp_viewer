#version 330 core
uniform sampler2D tex;
uniform int image_width;
uniform int image_height;
in vec2 TexCoord;
out vec4 FragColor;

// Bayer GRBG:
//   G R G R ...
//   B G B G ...
void main()
{
    ivec2 maxp = ivec2(image_width - 1, image_height - 1);
    ivec2 p    = ivec2(floor(TexCoord * vec2(image_width, image_height)));

    float C  = texelFetch(tex, p, 0).r;
    float N  = texelFetch(tex, clamp(p + ivec2( 0, -1), ivec2(0), maxp), 0).r;
    float S  = texelFetch(tex, clamp(p + ivec2( 0,  1), ivec2(0), maxp), 0).r;
    float E  = texelFetch(tex, clamp(p + ivec2( 1,  0), ivec2(0), maxp), 0).r;
    float W  = texelFetch(tex, clamp(p + ivec2(-1,  0), ivec2(0), maxp), 0).r;
    float NE = texelFetch(tex, clamp(p + ivec2( 1, -1), ivec2(0), maxp), 0).r;
    float NW = texelFetch(tex, clamp(p + ivec2(-1, -1), ivec2(0), maxp), 0).r;
    float SE = texelFetch(tex, clamp(p + ivec2( 1,  1), ivec2(0), maxp), 0).r;
    float SW = texelFetch(tex, clamp(p + ivec2(-1,  1), ivec2(0), maxp), 0).r;

    bool even_x = (p.x & 1) == 0;
    bool even_y = (p.y & 1) == 0;

    vec3 rgb;
    if (even_x && even_y)        // G-позиция: C=G, N/S=R, E/W=B
    {
        rgb = vec3((N + S) * 0.5, C, (E + W) * 0.5);
    }
    else if (!even_x && even_y)  // R-позиция: C=R, NSEW=G, диагонали=B
    {
        rgb = vec3(C, (N + S + E + W) * 0.25, (NE + NW + SE + SW) * 0.25);
    }
    else if (even_x && !even_y)  // B-позиция: C=B, NSEW=G, диагонали=R
    {
        rgb = vec3((NE + NW + SE + SW) * 0.25, (N + S + E + W) * 0.25, C);
    }
    else                         // G-позиция: C=G, N/S=B, E/W=R
    {
        rgb = vec3((E + W) * 0.5, C, (N + S) * 0.5);
    }

    FragColor = vec4(rgb, 1.0);
}