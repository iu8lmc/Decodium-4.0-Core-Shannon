#version 440

layout(location = 0) in vec2 vertexCoord;
// x = trace depth (0 newest, 1 oldest), y = 0 ridge / 1 panel bottom
layout(location = 1) in vec2 traceCoord;

layout(location = 0) out float vNorm;
layout(location = 1) out float vPerspective;
layout(location = 2) out float vLocalY;
layout(location = 3) out float vTopY;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 geometryParams;    // panel width/height, ridge width, glow width
    vec4 historyParams;     // next write row, row count, trace count, floor depth dB
    vec4 perspectiveParams; // far Y ratio, X shrink, ridge height ratio, exponent
    vec4 xParams;           // source X scale/bias, clamp flag, minimum 3D dB span
};

layout(binding = 1) uniform sampler2D historyTexture;
layout(binding = 2) uniform sampler2D rowParamsTexture;

float median3(float a, float b, float c)
{
    return max(min(a, b), min(max(a, b), c));
}

float rowCoord(float row, float rows)
{
    return (mod(row + rows * 4.0, rows) + 0.5) / rows;
}

void main()
{
    float width = max(geometryParams.x, 1.0);
    float height = max(geometryParams.y, 1.0);
    float rows = max(historyParams.y, 1.0);
    float traces = max(historyParams.z, 2.0);
    float depth = clamp(traceCoord.x, 0.0, 1.0);
    float traceOffset = floor(depth * (traces - 1.0) + 0.5);
    float row = mod(historyParams.x - 1.0 - traceOffset + rows * 4.0, rows);

    float displayX = clamp(vertexCoord.x / width, 0.0, 1.0);
    float sourceX = displayX * xParams.x + xParams.y;
    bool outside = xParams.z > 0.5 && (sourceX < 0.0 || sourceX > 1.0);
    sourceX = clamp(sourceX, 0.0, 1.0);

    float rowY = rowCoord(row, rows);
    float db0 = texture(historyTexture, vec2(sourceX, rowY)).r;
    float db1 = texture(historyTexture, vec2(sourceX, rowCoord(row - 1.0, rows))).r;
    float db2 = texture(historyTexture, vec2(sourceX, rowCoord(row + 1.0, rows))).r;
    float db = median3(db0, db1, db2);

    float rowMinDb = texture(rowParamsTexture, vec2(0.25, rowY)).r;
    // L'altezza si misura sopra il fondo della riga, su quanti dB i segnali
    // occupano davvero (xParams.w, misurato dalla CPU). Normalizzare
    // sull'intera finestra dei colori appiattiva tutto quando la soglia di
    // rumore automatica la ancora al rumore e la porta 80 dB piu' su.
    float floorDb = rowMinDb + historyParams.w;
    float floorRange = max(xParams.w, 1.0);
    float rawNorm = outside ? 0.0 : clamp((db - floorDb) / floorRange, 0.0, 1.0);
    float norm = rawNorm * rawNorm * (3.0 - 2.0 * rawNorm);

    float exponent = max(perspectiveParams.w, 0.1);
    float perspective = 1.0 - pow(max(0.0, 1.0 - depth), exponent);
    float shrink = clamp(perspectiveParams.y, 0.0, 0.8) * perspective;
    float projectedX = width * shrink * 0.5 + vertexCoord.x * (1.0 - shrink);
    float nearY = height - 1.0;
    float farY = height * perspectiveParams.x;
    float bandHeight = max(nearY - farY, 1.0);
    float baseY = nearY - perspective * bandHeight;
    float ridgeHeight = bandHeight * perspectiveParams.z * (1.0 - 0.55 * perspective);
    float topY = baseY - norm * ridgeHeight;
    float localY = mix(topY, height, clamp(traceCoord.y, 0.0, 1.0));

    vNorm = norm;
    vPerspective = perspective;
    vLocalY = localY;
    vTopY = topY;
    gl_Position = qt_Matrix * vec4(projectedX, localY, 0.0, 1.0);
}
