#version 330 core
uniform sampler2D tex;
uniform int saturation_enabled;
in vec2 TexCoord;
out vec4 FragColor;

void main()
{
    vec4 c = texture(tex, TexCoord);

    if (saturation_enabled != 0)
    {
        bool under = (c.r <= 0.0) && (c.g <= 0.0) && (c.b <= 0.0);
        bool over  = (c.r >= 1.0) && (c.g >= 1.0) && (c.b >= 1.0);
        if (under)
        {
            c = vec4(0.0, 0.0, 1.0, 1.0);
        }
        else if (over)
        {
            c = vec4(1.0, 0.0, 0.0, 1.0);
        }
    }

    FragColor = c;
}