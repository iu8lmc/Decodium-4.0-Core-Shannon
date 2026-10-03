#version 440

layout(location = 0) in vec2 markerLonLat;
layout(location = 1) in vec4 markerOffsetTex;

layout(location = 0) out vec2 vTexCoord;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 markerColor;
    vec4 viewParams;
    vec4 rectParams;
};

float wrapLonDelta(float lon)
{
    return mod(lon + 540.0, 360.0) - 180.0;
}

vec2 projectLonLat(vec2 lonLat)
{
    float dx = wrapLonDelta(lonLat.x - viewParams.x);
    float x = rectParams.x + (dx + viewParams.z * 0.5) / viewParams.z * rectParams.z;
    float y = rectParams.y + (viewParams.y + viewParams.w * 0.5 - lonLat.y) / viewParams.w * rectParams.w;
    return vec2(x, y);
}

void main()
{
    vTexCoord = markerOffsetTex.zw;
    vec2 xy = projectLonLat(markerLonLat) + markerOffsetTex.xy;
    gl_Position = qt_Matrix * vec4(xy, 0.0, 1.0);
}
