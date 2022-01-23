R"(
    #version 440 core
    
    layout(location = 0) in vec2 a_Pos;
    layout(location = 1) in vec2 a_UV;
    layout(location = 2) in vec4 a_Color;

    layout(location = 0)out vec4 v_Color;
    layout(location = 1)out vec2 v_UV;

    layout(binding = 0) uniform Transform{ 
        vec2 u_Scale; 
        vec2 u_Translate; 
    };

    void main()
    {
        v_Color = a_Color;
        v_UV = a_UV;
        gl_Position = vec4(a_Pos * u_Scale + u_Translate, 0, 1);
    }
)"