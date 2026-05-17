#version 440

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 markerColor;
};

void main()
{
    vec2 p = vTexCoord * 2.0 - vec2(1.0);
    float d = length(p);
    float alpha = 1.0 - smoothstep(0.82, 1.0, d);
    float highlight = 1.0 - smoothstep(0.08, 0.78, distance(vTexCoord, vec2(0.34, 0.32)));
    vec3 color = markerColor.rgb + vec3(0.18) * highlight;
    float outAlpha = markerColor.a * alpha * qt_Opacity;
    fragColor = vec4(color * outAlpha, outAlpha);
}
