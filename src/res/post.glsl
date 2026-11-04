#version 330 core
uniform sampler2D tex;
uniform int saturation_enabled;
uniform int gradient_enabled;
uniform vec2 texel_size;
uniform vec2 image_origin;   // bottom-left угла изображения в gl-координатах окна
uniform vec2 image_size;     // размер изображения в пикселях окна
out vec4 FragColor;

float luma(vec3 c)
{
    return dot(c, vec3(0.299, 0.587, 0.114));
}

void main()
{
    vec2 uv = (gl_FragCoord.xy - image_origin) / image_size;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
    {
        discard;
    }
    uv.y = 1.0 - uv.y;

    vec4 c = texture(tex, uv);

    if (gradient_enabled != 0)
    {
        float nw = luma(texture(tex, uv + vec2(-texel_size.x, -texel_size.y)).rgb);
        float n  = luma(texture(tex, uv + vec2( 0.0,          -texel_size.y)).rgb);
        float ne = luma(texture(tex, uv + vec2( texel_size.x, -texel_size.y)).rgb);
        float w  = luma(texture(tex, uv + vec2(-texel_size.x,  0.0)).rgb);
        float e  = luma(texture(tex, uv + vec2( texel_size.x,  0.0)).rgb);
        float sw = luma(texture(tex, uv + vec2(-texel_size.x,  texel_size.y)).rgb);
        float s  = luma(texture(tex, uv + vec2( 0.0,           texel_size.y)).rgb);
        float se = luma(texture(tex, uv + vec2( texel_size.x,  texel_size.y)).rgb);

        float gx = (ne + 2.0 * e + se) - (nw + 2.0 * w + sw);
        float gy = (sw + 2.0 * s + se) - (nw + 2.0 * n + ne);
        float mag = sqrt(gx * gx + gy * gy);
        mag = clamp(mag, 0.0, 1.0);
        c = vec4(mag, mag, mag, 1.0);
    }

    if (saturation_enabled != 0)
    {
        bool under = (c.r <= 0.0) && (c.g <= 0.0) && (c.b <= 0.0);
        bool over  = (c.r >= 1.0) && (c.g >= 1.0) && (c.b >= 1.0);
        if (under)      c = vec4(0.0, 0.0, 1.0, 1.0);
        else if (over)  c = vec4(1.0, 0.0, 0.0, 1.0);
    }

    FragColor = c;
}
