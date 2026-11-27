#include "DecodiumBridgeTxFormat.h"
#include <QAudioSink>
#include <QBuffer>
#include <QCoreApplication>
#include <QDateTime>
#include <QList>
#include <QOperatingSystemVersion>
#include <QPointer>
#include <QString>
#include <QTimer>

void decodiumBridgeAudioLevelLog(const QString&);
void decodiumBridgeTxPrepareTimelineLog(const QString&);

static bool deferCoreAudioSinkDelete()
{
#if defined(Q_OS_MAC)
    return QOperatingSystemVersion::current()
        >= QOperatingSystemVersion(QOperatingSystemVersion::MacOS, 15);
#else
    return false;
#endif
}

static void deleteBufferLater(QBuffer *buffer, QObject *context)
{
    if (!buffer) {
        return;
    }
    buffer->close();
    if (!context) {
        delete buffer;
        return;
    }
    buffer->setParent(context);
    buffer->deleteLater();
}

#if defined(Q_OS_MAC)
static QList<QPointer<QAudioSink>>& parkedCoreAudioTxSinks()
{
    static QList<QPointer<QAudioSink>> sinks;
    return sinks;
}

static void parkCoreAudioTxSink(QAudioSink *sink, const QString& reason)
{
    if (!sink) {
        return;
    }
    // Do not let QObject parent teardown or a bounded-list cleanup destroy a
    // retired macOS QAudioSink. Qt/CoreAudio may crash in stopAudioUnit() while
    // removing the device-disconnect listener. Park it detached and let the OS
    // reclaim it on process exit.
    sink->setParent(nullptr);

    QList<QPointer<QAudioSink>>& sinks = parkedCoreAudioTxSinks();
    for (int i = sinks.size() - 1; i >= 0; --i) {
        if (!sinks.at(i)) {
            sinks.removeAt(i);
        }
    }

    sinks.append(QPointer<QAudioSink>(sink));
    if ((sinks.size() % 16) == 0) {
        decodiumBridgeAudioLevelLog(QStringLiteral("TX CoreAudio parked sinks retained to avoid Qt/CoreAudio stopAudioUnit crash: reason=%1 count=%2")
                      .arg(reason)
                      .arg(sinks.size()));
    }
}
#endif

// 1.0.217 — Lifetime parking allineato per platform.
// Mac: short grace window (workaround stopAudioUnit crash CoreAudio) without
// leaving parked TX sinks alive across multiple FT2 slots.
// Windows: 8s (oltre 1 slot FT2 7.5s ma sotto 2 slot, max ~2 sink parked
// contemporanei. Pre-1.0.217 era 30s hardcoded come Mac, ma con park
// abilitato su Windows da 1.0.216 si accumulava 4 sink/slot FT2 =
// memory leak temporaneo + WASAPI session pressure).
static constexpr int kRetireAudioSinkParkMs =
#if defined(Q_OS_MAC)
    6000;
#elif defined(Q_OS_WIN)
    8000;
#else
    30000;
#endif

static void retireAudioSink(QAudioSink *sink, QBuffer *buffer, const QString& reason)
{
    if (!sink) {
        deleteBufferLater(buffer, QCoreApplication::instance());
        return;
    }

    sink->disconnect();

    if (deferCoreAudioSinkDelete()) {
#if defined(Q_OS_MAC)
        if (sink->state() != QAudio::StoppedState) {
            sink->setVolume(0.0f);
        }
        deleteBufferLater(buffer, QCoreApplication::instance());
        parkCoreAudioTxSink(sink, reason);
        decodiumBridgeAudioLevelLog(QStringLiteral("TX CoreAudio sink parked bounded: reason=%1 cap=12").arg(reason));
        decodiumBridgeTxPrepareTimelineLog(QStringLiteral("[TX-TL] park_entry reason=%1 lifetime_ms=bounded_mac cap=12").arg(reason));
        return;
#endif
        QObject *context = QCoreApplication::instance();
        if (!context) {
            if (sink->state() != QAudio::StoppedState) {
                sink->stop();
            }
            delete sink;
            if (buffer) {
                buffer->close();
                delete buffer;
            }
            return;
        }
        QPointer<QAudioSink> sinkGuard(sink);
        QPointer<QBuffer> bufferGuard(buffer);
        sink->setParent(nullptr);
        if (buffer) {
            buffer->setParent(nullptr);
        }

        // 1.0.225 — setVolume(0) solo se sink non e' gia' StoppedState
        // (Qt6 logga warning "QAudioSink::setVolume() called on stopped
        // sink" che attraversa il qt-message-handler -> log sync flush).
        if (sink->state() != QAudio::StoppedState) {
            sink->setVolume(0.0f);
        }
        if (buffer) {
            buffer->close();
        }

        decodiumBridgeAudioLevelLog(QStringLiteral("TX audio sink parked: reason=%1 lifetimeMs=%2")
                      .arg(reason).arg(kRetireAudioSinkParkMs));
        // 1.0.218 — [TX-TL] park entry timestamp per misurare lo span tra
        // park e delayed-delete release. Aiuta a distinguere stutter da
        // sink-create dal stutter da park-delete-overlap.
        qint64 const parkEntryMs = QDateTime::currentMSecsSinceEpoch();
        decodiumBridgeTxPrepareTimelineLog(QStringLiteral("[TX-TL] park_entry reason=%1 lifetime_ms=%2")
                      .arg(reason).arg(kRetireAudioSinkParkMs));
        QTimer::singleShot(kRetireAudioSinkParkMs, context, [sinkGuard, bufferGuard, reason, parkEntryMs]() {
            if (sinkGuard) {
                sinkGuard->disconnect();
                if (sinkGuard->state() == QAudio::StoppedState) {
#if defined(Q_OS_MAC)
                    qint64 const parkSpanMs = QDateTime::currentMSecsSinceEpoch() - parkEntryMs;
                    decodiumBridgeAudioLevelLog(QStringLiteral("TX CoreAudio sink left parked stopped after lifetime: reason=%1 park_span_ms=%2").arg(reason).arg(parkSpanMs));
                    decodiumBridgeTxPrepareTimelineLog(QStringLiteral("[TX-TL] park_release reason=%1 span_ms=%2 state=parked_mac_stopped_no_delete").arg(reason).arg(parkSpanMs));
                    return;
#else
                    sinkGuard->deleteLater();
                    if (bufferGuard) {
                        bufferGuard->deleteLater();
                    }
                    qint64 const parkSpanMs = QDateTime::currentMSecsSinceEpoch() - parkEntryMs;
                    decodiumBridgeAudioLevelLog(QStringLiteral("TX audio sink deferred delete released: reason=%1 park_span_ms=%2").arg(reason).arg(parkSpanMs));
                    decodiumBridgeTxPrepareTimelineLog(QStringLiteral("[TX-TL] park_release reason=%1 span_ms=%2 state=stopped").arg(reason).arg(parkSpanMs));
                    return;
#endif
                }
                // Dopo la safe-window forziamo stop+delete solo sui backend
                // dove e' stabile. Su macOS/Qt 6.11 il cleanup differito puo'
                // toccare socket notifier CoreAudio da un QThread diverso
                // (warning QSocketNotifier + crash), quindi il sink resta
                // parcheggiato e muto finche' non viene reclamato a exit.
#if defined(Q_OS_WIN)
                QString const parkedState = audioStateToString(sinkGuard->state());
                sinkGuard->stop();
                sinkGuard->deleteLater();
                if (bufferGuard) {
                    bufferGuard->deleteLater();
                }
                qint64 const parkSpanMsForcedWin = QDateTime::currentMSecsSinceEpoch() - parkEntryMs;
                decodiumBridgeAudioLevelLog(QStringLiteral("TX audio sink parked stop+delete after lifetime: reason=%1 state=%2")
                              .arg(reason)
                              .arg(parkedState));
                decodiumBridgeTxPrepareTimelineLog(QStringLiteral("[TX-TL] park_release reason=%1 span_ms=%2 state=forced_stop_win").arg(reason).arg(parkSpanMsForcedWin));
                return;
#elif defined(Q_OS_MAC)
                QString const parkedState = audioStateToString(sinkGuard->state());
                QString const parkedError = audioErrorToString(sinkGuard->error());
                qint64 const parkSpanMsForcedMac = QDateTime::currentMSecsSinceEpoch() - parkEntryMs;
                decodiumBridgeAudioLevelLog(QStringLiteral("TX CoreAudio sink left parked after lifetime: reason=%1 state=%2 err=%3")
                              .arg(reason)
                              .arg(parkedState)
                              .arg(parkedError));
                decodiumBridgeTxPrepareTimelineLog(QStringLiteral("[TX-TL] park_release reason=%1 span_ms=%2 state=parked_mac_no_stop").arg(reason).arg(parkSpanMsForcedMac));
                return;
#else
                decodiumBridgeAudioLevelLog(QStringLiteral("TX CoreAudio sink parked to avoid Qt stopAudioUnit crash: reason=%1 state=%2 err=%3")
                              .arg(reason)
                              .arg(audioStateToString(sinkGuard->state()))
                              .arg(audioErrorToString(sinkGuard->error())));
                return;
#endif
            }
            if (bufferGuard) {
                bufferGuard->deleteLater();
            }
        });
        return;
    }

    if (sink->state() != QAudio::StoppedState) {
        sink->stop();
    }
    delete sink;
    if (buffer) {
        buffer->close();
        delete buffer;
    }
}

void decodiumBridgeRetireAudioSink(QAudioSink* sink, QBuffer* buffer, const QString& reason) { retireAudioSink(sink, buffer, reason); }


