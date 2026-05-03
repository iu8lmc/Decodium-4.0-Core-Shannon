#version 440

layout(location = 0) in vec2 vertexCoord;
layout(location = 1) in vec2 texCoord;

layout(location = 0) out vec2 vTexCoord;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 wf_Params;
};

void main()
{
    vTexCoord = texCoord;
    gl_Position = qt_Matrix * vec4(vertexCoord, 0.0, 1.0);
}
