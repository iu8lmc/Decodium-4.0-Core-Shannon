quint32 stableSstvStreamId(QStringView identity)
{
    // Stable FNV-1a over UTF-16 code units. This runs only on owner-thread
    // source transitions; the audio callback receives the resulting scalar.
    quint32 value = 2'166'136'261U;
    for (QChar character : identity) {
        const quint16 codeUnit = character.unicode();
        value ^= static_cast<quint8>(codeUnit & 0xffU);
        value *= 16'777'619U;
        value ^= static_cast<quint8>((codeUnit >> 8U) & 0xffU);
        value *= 16'777'619U;
    }
    return value == 0U ? 1U : value;
}

