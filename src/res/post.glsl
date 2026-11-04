#version 330 core
uniform sampler2D tex;
uniform int saturation_enabled;
uniform int gradient_enabled;
uniform int auto_levels_enabled;
uniform vec2 texel_size;
uniform vec2 image_origin;
uniform vec2 image_size;
uniform vec3 level_min;
uniform vec3 level_max;
out vec4 FragColor;

float luma(vec3 c)
{
    return dot(c, vec3(0.299, 0.587, 0.114));
}

vec3 apply_levels(vec3 color)
{
    if (auto_levels_enabled == 0)
    {
        return color;
    }
    vec3 inv_range = 1.0 / max(level_max - level_min, vec3(1e-6));
    return clamp((color - level_min) * inv_range, 0.0, 1.0);
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
    c.rgb  = apply_levels(c.rgb);

    if (gradient_enabled != 0)
    {
        float nw = luma(apply_levels(texture(tex, uv + vec2(-texel_size.x, -texel_size.y)).rgb));
        float n  = luma(apply_levels(texture(tex, uv + vec2( 0.0,          -texel_size.y)).rgb));
        float ne = luma(apply_levels(texture(tex, uv + vec2( texel_size.x, -texel_size.y)).rgb));
        float w  = luma(apply_levels(texture(tex, uv + vec2(-texel_size.x,  0.0)).rgb));
        float e  = luma(apply_levels(texture(tex, uv + vec2( texel_size.x,  0.0)).rgb));
        float sw = luma(apply_levels(texture(tex, uv + vec2(-texel_size.x,  texel_size.y)).rgb));
        float s  = luma(apply_levels(texture(tex, uv + vec2( 0.0,           texel_size.y)).rgb));
        float se = luma(apply_levels(texture(tex, uv + vec2( texel_size.x,  texel_size.y)).rgb));

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