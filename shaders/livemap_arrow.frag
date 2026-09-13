#version 440

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 arrowColor;
    vec4 viewParams;
    vec4 rectParams;
    vec4 animParams;
};

void main()
{
    fragColor = vec4(arrowColor.rgb, arrowColor.a * qt_Opacity);
}
