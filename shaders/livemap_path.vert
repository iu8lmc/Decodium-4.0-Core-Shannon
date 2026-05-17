#version 440

layout(location = 0) in vec4 endpoints;
layout(location = 1) in float pathT;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 pathColor;
    vec4 viewParams;
    vec4 rectParams;
};

const float PI = 3.14159265358979323846;

float radians1(float deg)
{
    return deg * PI / 180.0;
}

float degrees1(float rad)
{
    return rad * 180.0 / PI;
}

float wrapLonDelta(float lon)
{
    return mod(lon + 540.0, 360.0) - 180.0;
}

vec3 lonLatToVector(float lonDeg, float latDeg)
{
    float lon = radians1(lonDeg);
    float lat = radians1(latDeg);
    float cosLat = cos(lat);
    return vec3(cosLat * cos(lon), cosLat * sin(lon), sin(lat));
}

vec2 vectorToLonLat(vec3 v)
{
    vec3 n = normalize(v);
    return vec2(degrees1(atan(n.y, n.x)), degrees1(asin(clamp(n.z, -1.0, 1.0))));
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
    vec3 a = lonLatToVector(endpoints.x, endpoints.y);
    vec3 b = lonLatToVector(endpoints.z, endpoints.w);
    float dotAB = clamp(dot(a, b), -1.0, 1.0);
    float omega = acos(dotAB);
    float sinOmega = sin(omega);
    vec3 p;
    if (abs(sinOmega) < 0.000001) {
        p = mix(a, b, pathT);
    } else {
        float wa = sin((1.0 - pathT) * omega) / sinOmega;
        float wb = sin(pathT * omega) / sinOmega;
        p = a * wa + b * wb;
    }

    vec2 xy = projectLonLat(vectorToLonLat(p));
    gl_Position = qt_Matrix * vec4(xy, 0.0, 1.0);
}
