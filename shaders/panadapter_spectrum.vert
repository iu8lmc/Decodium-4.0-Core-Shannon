#version 440

layout(location = 0) in vec2 vertexCoord;
layout(location = 1) in vec2 texCoord;

layout(location = 0) out vec2 vTexCoord;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 spec_Params;
    vec4 spec_XParams;
    vec4 spec_Background;
    vec4 spec_Fill;
    vec4 spec_Glow;
    vec4 spec_Trace;
    vec4 spec_Peak;
};

void main()
{
    vTexCoord = texCoord;
    gl_Position = qt_Matrix * vec4(vertexCoord, 0.0, 1.0);
}
