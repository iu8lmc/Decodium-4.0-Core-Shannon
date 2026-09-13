#version 440

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 spec_Params; // minDb, invRange, lineWidthNorm, showPeak
    vec4 spec_XParams; // xScale, xOffset, reserved, enableClamp
    vec4 spec_Background;
    vec4 spec_Fill;
    vec4 spec_Glow;
    vec4 spec_Trace;
    vec4 spec_Peak;
};

layout(binding = 1) uniform sampler2D spectrumTexture;
layout(binding = 2) uniform sampler2D peakTexture;

void main()
{
    float mappedX = vTexCoord.x * spec_XParams.x + spec_XParams.y;
    bool outside = spec_XParams.w > 0.5 && (mappedX < 0.0 || mappedX > 1.0);
    float sampleX = spec_XParams.w > 0.5 ? clamp(mappedX, 0.0, 1.0) : vTexCoord.x;
    if (outside) {
        fragColor = vec4(spec_Background.rgb, qt_Opacity);
        return;
    }

    float db = texture(spectrumTexture, vec2(sampleX, 0.5)).r;
    float norm = clamp((db - spec_Params.x) * spec_Params.y, 0.0, 1.0);
    float y = 1.0 - clamp(vTexCoord.y, 0.0, 1.0);
    float lineWidth = max(spec_Params.z, 0.0015);
    float glowWidth = lineWidth * 3.0;

    vec4 color = spec_Background;
    if (y <= norm)
        color = mix(spec_Background, spec_Fill, spec_Fill.a);
    if (abs(y - norm) <= glowWidth)
        color = mix(color, spec_Glow, spec_Glow.a);
    if (abs(y - norm) <= lineWidth)
        color = vec4(spec_Trace.rgb, 1.0);

    if (spec_Params.w > 0.5) {
        float peakDb = texture(peakTexture, vec2(sampleX, 0.5)).r;
        float peakNorm = clamp((peakDb - spec_Params.x) * spec_Params.y, 0.0, 1.0);
        if (abs(y - peakNorm) <= lineWidth)
            color = mix(color, spec_Peak, spec_Peak.a);
    }

    fragColor = vec4(color.rgb, qt_Opacity);
}
