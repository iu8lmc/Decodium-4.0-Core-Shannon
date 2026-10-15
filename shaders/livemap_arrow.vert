#version 440

layout(location = 0) in vec4 endpoints;
layout(location = 1) in vec4 arrowMeta;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 arrowColor;
    vec4 viewParams;
    vec4 rectParams;
    vec4 animParams;
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

vec2 greatCirclePoint(float t)
{
    vec3 a = lonLatToVector(endpoints.x, endpoints.y);
    vec3 b = lonLatToVector(endpoints.z, endpoints.w);
    float dotAB = clamp(dot(a, b), -1.0, 1.0);
    float omega = acos(dotAB);
    float sinOmega = sin(omega);
    vec3 p;
    if (abs(sinOmega) < 0.000001) {
        p = mix(a, b, t);
    } else {
        float wa = sin((1.0 - t) * omega) / sinOmega;
        float wb = sin(t * omega) / sinOmega;
        p = a * wa + b * wb;
    }
    return vectorToLonLat(p);
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
    float baseProgress = fract(animParams.x + arrowMeta.x);
    float txProgress = fract(animParams.y);
    float progress = mix(baseProgress, txProgress, step(0.5, arrowMeta.w));

    vec2 tip = projectLonLat(greatCirclePoint(progress));
    vec2 tail = projectLonLat(greatCirclePoint(max(0.0, progress - 0.022)));
    vec2 direction = tip - tail;
    float len = length(direction);
    vec2 u = len > 0.001 ? direction / len : vec2(1.0, 0.0);
    vec2 n = vec2(-u.y, u.x);
    vec2 xy = tip + u * arrowMeta.y + n * arrowMeta.z;

    gl_Position = qt_Matrix * vec4(xy, 0.0, 1.0);
}
