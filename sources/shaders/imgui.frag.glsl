R"(
    #version 440 core

    layout(location = 0)in vec4 v_Color;
    layout(location = 1)in vec2 v_UV;

    layout(location = 0) out vec4 f_Color;

    layout(binding = 1) uniform sampler2D u_Texture;

    void main()
    {
        f_Color = v_Color * texture(u_Texture, v_UV);
    }
)"