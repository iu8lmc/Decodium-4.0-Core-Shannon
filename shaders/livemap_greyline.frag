#version 440

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 sunParams; // subSolarLonDeg, subSolarLatDeg, enabled, maxAlpha
    vec4 viewParams; // centerLonDeg, centerLatDeg, spanLonDeg, spanLatDeg
};

const float PI = 3.14159265358979323846;

float radians1(float deg)
{
    return deg * PI / 180.0;
}

void main()
{
    if (sunParams.z < 0.5) {
        fragColor = vec4(0.0);
        return;
    }

    float lon = viewParams.x - viewParams.z * 0.5 + vTexCoord.x * viewParams.z;
    float lat = viewParams.y + viewParams.w * 0.5 - vTexCoord.y * viewParams.w;
    float subLon = sunParams.x;
    float subLat = sunParams.y;

    float latRad = radians1(lat);
    float subLatRad = radians1(subLat);
    float dLon = radians1(lon - subLon);

    float altitude = sin(subLatRad) * sin(latRad)
        + cos(subLatRad) * cos(latRad) * cos(dLon);

    float night = smoothstep(0.06, -0.06, altitude);
    float deepNight = smoothstep(-0.08, -0.28, altitude);
    float alpha = min(sunParams.w, (night * 0.46 + deepNight * 0.16) * sunParams.w);

    fragColor = vec4(0.0, 0.02, 0.12, alpha * qt_Opacity);
}
