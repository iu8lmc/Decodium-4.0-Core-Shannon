#include "DecodiumBridge.h"
#include "Network/RemoteCommandServer.hpp"
#include <QDateTime>

void decodiumBridgeDecodeFlowLog(const QString&);

void DecodiumBridge::onLegacyWaterfallRow(QByteArray const& rowLevels,
                                          int startFrequencyHz,
                                          int spanHz,
                                          int rxFrequencyHz,
                                          int txFrequencyHz,
                                          QString const& mode)
{
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    if (nowMs - m_lastLegacyWaterfallDiagLogMs >= 2000) {
        m_lastLegacyWaterfallDiagLogMs = nowMs;
        int minLevel = 255;
        int maxLevel = 0;
        int nonZero = 0;
        quint64 sum = 0;
        for (unsigned char level : rowLevels) {
            minLevel = qMin(minLevel, static_cast<int>(level));
            maxLevel = qMax(maxLevel, static_cast<int>(level));
            sum += level;
            if (level != 0) {
                ++nonZero;
            }
        }
        double const mean = rowLevels.isEmpty()
            ? 0.0
            : static_cast<double>(sum) / static_cast<double>(rowLevels.size());
        decodiumBridgeDecodeFlowLog(QStringLiteral(
            "[LegacyAudio][WATERFALL] bins=%1 min=%2 max=%3 mean=%4 nonzero=%5 mode=%6")
                      .arg(rowLevels.size())
                      .arg(rowLevels.isEmpty() ? 0 : minLevel)
                      .arg(maxLevel)
                      .arg(mean, 0, 'f', 1)
                      .arg(nonZero)
                      .arg(mode));
    }
    bool const modernPanadapterFresh =
        !m_lastPanadapterData.isEmpty()
        && m_lastPanadapterFrameMs > 0
        && (nowMs - m_lastPanadapterFrameMs) < 2500;

    if (!modernPanadapterFresh
        && m_remoteServer
        && m_remoteServer->isRunning()
        && m_remoteServer->waterfallEnabled()
        && !rowLevels.isEmpty()
        && spanHz > 0)
        {
          m_remoteServer->publishWaterfallRow(rowLevels,
                                             startFrequencyHz,
                                             spanHz,
                                             rxFrequencyHz,
                                             txFrequencyHz,
                                             mode);
        }

    if (!usingLegacyBackendForRx() || rowLevels.isEmpty() || spanHz <= 0)
        {
          return;
        }

    if ((useModernSpectrumFeedWithLegacy() || m_legacyPcmSpectrumFeed)
        && !m_lastPanadapterData.isEmpty())
        {
          return;
        }

    QVector<float> dbValues;
    dbValues.reserve(rowLevels.size());

    for (char level : rowLevels)
        {
          float const value = static_cast<float> (static_cast<unsigned char> (level));
          float const norm = value / 254.0f;
          // Legacy waterfall rows are post-gain intensity levels, not real dB.
          // Map them into the same visual dB span used by the QML panadapter
          // so SmartSDR-style auto-range and labels stay coherent.
          dbValues.push_back (-130.0f + norm * 90.0f);
        }

    m_lastPanadapterFrameMs = QDateTime::currentMSecsSinceEpoch();
    emit panadapterDataReady (dbValues,
                              -130.0f,
                              -40.0f,
                              static_cast<float> (startFrequencyHz),
                              static_cast<float> (startFrequencyHz + spanHz));
}
