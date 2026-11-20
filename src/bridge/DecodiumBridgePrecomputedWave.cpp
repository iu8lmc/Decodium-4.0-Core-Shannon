#include "commons.h"
#include "widgets/FoxWaveGuard.hpp"
#include <QReadWriteLock>
#include <QString>
#include <QVector>
#include <algorithm>

static void storeBridgePrecomputedTxWave(QString const& mode, QVector<float> const& wave)
{
    int constexpr maxSamples {FOXCOM_WAVE_SIZE};
    int const copySamples = qMin(maxSamples, wave.size());
    foxcom_ensure_wave(&foxcom_);
    QWriteLocker waveLock {&fox_wave_lock()};
    if (copySamples > 0) {
        std::copy_n(wave.constBegin(), copySamples, foxcom_.wave);
        std::fill(foxcom_.wave + copySamples, foxcom_.wave + maxSamples, 0.0f);
        precomputed_tx_wave_mode() = mode;
        precomputed_tx_wave_buffer() = wave.mid(0, copySamples);
    } else if (precomputed_tx_wave_mode() == mode) {
        precomputed_tx_wave_mode().clear();
        precomputed_tx_wave_buffer().clear();
        std::fill(foxcom_.wave, foxcom_.wave + maxSamples, 0.0f);
    }
}

void decodiumBridgeStorePrecomputedTxWave(QString const& mode, QVector<float> const& wave) { storeBridgePrecomputedTxWave(mode, wave); }

