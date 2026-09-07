#version 440

layout(location = 0) in float vNorm;
layout(location = 1) in float vPerspective;
layout(location = 2) in float vLocalY;
layout(location = 3) in float vTopY;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 geometryParams;
    vec4 historyParams;
    vec4 perspectiveParams;
    vec4 xParams;
};

layout(binding = 3) uniform sampler2D paletteTexture;

void main()
{
    vec3 ridgeColor = texture(paletteTexture, vec2(clamp(vNorm, 0.0, 1.0), 0.5)).rgb;
    float fillDepth = clamp((vLocalY - vTopY) / max(geometryParams.y - vTopY, 1.0), 0.0, 1.0);
    float topShade = 0.30 * (1.0 - 0.45 * vPerspective);
    float shade = mix(topShade, 0.06, pow(fillDepth, 0.72));
    vec3 color = ridgeColor * shade;

    float distanceFromRidge = max(vLocalY - vTopY, 0.0);
    float ridgeWidth = mix(max(geometryParams.z, 1.0), 1.0, vPerspective);
    float glowWidth = max(geometryParams.w, ridgeWidth);
    float distanceFade = 1.0 - 0.72 * vPerspective;
    float glow = (1.0 - smoothstep(ridgeWidth, glowWidth, distanceFromRidge))
        * 0.45 * distanceFade;
    float ridge = (1.0 - smoothstep(0.0, ridgeWidth, distanceFromRidge)) * distanceFade;
    color = mix(color, ridgeColor, glow);
    color = mix(color, ridgeColor, ridge);

    fragColor = vec4(color, qt_Opacity);
}
