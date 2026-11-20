QString decodiumBridgeTxPrepareAudioFormat(const QAudioFormat& format) { return audioFormatToString(format); }
bool decodiumBridgeTxPrepareSameFormat(const QAudioFormat& a, const QAudioFormat& b) { return sameAudioFormat(a, b); }
QAudioFormat decodiumBridgeTxPrepareChooseFormat(const QAudioDevice& device) { return chooseTxAudioFormat(device); }
QByteArray decodiumBridgeTxPreparePcm(const QVector<float>& wave, const QAudioFormat& format, int channel) { return buildTxPcmBuffer(wave, format, boundedAudioChannel(channel)); }
