#version 440

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 wf_Params; // paletteScaleX, paletteBiasX, ringWriteRow, ringRows
    vec4 wf_LevelParams; // blackThreshold, inverseUsableRange, gamma, rawLevelMode
    vec4 wf_XParams; // xScale, xBias, unused, gpuFrequencyMapMode
};

layout(binding = 1) uniform sampler2D intensityTexture;
layout(binding = 2) uniform sampler2D paletteTexture;
layout(binding = 3) uniform sampler2D rowParamsTexture;

void main()
{
    vec2 intensityCoord = vTexCoord;
    bool sourceOutside = false;
    if (wf_XParams.w > 0.5) {
        float sourceX = vTexCoord.x * wf_XParams.x + wf_XParams.y;
        sourceOutside = sourceX < 0.0 || sourceX > 1.0;
        intensityCoord.x = clamp(sourceX, 0.0, 1.0);
    }
    if (wf_Params.z >= 0.0 && wf_Params.w > 1.0) {
        float rows = wf_Params.w;
        float displayRow = floor(clamp(vTexCoord.y, 0.0, 0.999999) * rows);
        float sourceRow = mod(wf_Params.z - 1.0 - displayRow + rows * 2.0, rows);
        intensityCoord.y = (sourceRow + 0.5) / rows;
    }
    float level = 0.0;
    if (wf_LevelParams.w > 1.5) {
        float rawDb = sourceOutside ? -100000.0 : texture(intensityTexture, intensityCoord).r;
        float rowMinDb = texture(rowParamsTexture, vec2(0.25, intensityCoord.y)).r;
        float rowInvRange = max(texture(rowParamsTexture, vec2(0.75, intensityCoord.y)).r, 0.000001);
        level = sourceOutside ? 0.0 : clamp((rawDb - rowMinDb) * rowInvRange, 0.0, 1.0);
        float adjusted = clamp((level - wf_LevelParams.x) * wf_LevelParams.y, 0.0, 1.0);
        level = pow(adjusted, max(wf_LevelParams.z, 0.01));
    } else {
        level = sourceOutside ? 0.0 : clamp(texture(intensityTexture, intensityCoord).r, 0.0, 1.0);
    }
    if (wf_LevelParams.w > 0.5 && wf_LevelParams.w <= 1.5) {
        float adjusted = clamp((level - wf_LevelParams.x) * wf_LevelParams.y, 0.0, 1.0);
        level = pow(adjusted, max(wf_LevelParams.z, 0.01));
    }
    vec4 color = texture(paletteTexture, vec2(level * wf_Params.x + wf_Params.y, 0.5));
    fragColor = vec4(color.rgb, color.a * qt_Opacity);
}
