#version 440

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 wf_Params; // paletteScaleX, paletteBiasX, paletteY, intensityScaleY
};

layout(binding = 1) uniform sampler2D intensityTexture;
layout(binding = 2) uniform sampler2D paletteTexture;

void main()
{
    float level = clamp(texture(intensityTexture, vTexCoord).r, 0.0, 1.0);
    vec4 color = texture(paletteTexture, vec2(level * wf_Params.x + wf_Params.y, 0.5));
    fragColor = vec4(color.rgb, color.a * qt_Opacity);
}
