static int legacyDepthBitsToModernDepth(int legacyBits)
{
    int depth = legacyBits & 0x7;
    if (depth <= 0) {
        depth = 3;
    }
    if (legacyBits & 0xF0) {
        depth = 4;
    }
    return qBound(1, depth, 4);
}
int decodiumBridgeLegacyDepthBitsToModernDepth(int bits) { return legacyDepthBitsToModernDepth(bits); }


static QString legacyPaletteNameForUiIndex(int index)
{
    switch (index) {
    case 1: return QStringLiteral("Raptor");
    case 2: return QStringLiteral("Gray1");
    case 3: return QStringLiteral("Blue1");
    case 4: return QStringLiteral("Sunburst");
    case 5: return QStringLiteral("Linrad");
    case 0:
    default:
        return QStringLiteral("Default");
    }
}
QString decodiumBridgeLegacyPaletteNameForUiIndex(int value) { return legacyPaletteNameForUiIndex(value); }


