#include "DecodiumBridge.h"

void DecodiumBridge::resetLedStatus()
{
    m_ledCoherentAveraging = false;
    m_ledNeuralSync = false;
    m_ledTurboFeedback = false;
    m_coherentCount = 0;
    m_neuralScore = 0.0;
    m_turboIterations = 0;
    emit ledCoherentAveragingChanged();
    emit ledNeuralSyncChanged();
    emit ledTurboFeedbackChanged();
    emit coherentCountChanged();
    emit neuralScoreChanged();
    emit turboIterationsChanged();
}

// ---- Hooks chiamati dai DecodeWorker (via QMetaObject::invokeMethod / Qt::QueuedConnection)
// Restano on per la durata del periodo decode; resetLedStatus() li spegne a fine ciclo.
void DecodiumBridge::notifyCoherentAveraging(int signalsInCache)
{
    bool const on = signalsInCache > 0;
    if (m_ledCoherentAveraging != on) { m_ledCoherentAveraging = on; emit ledCoherentAveragingChanged(); }
    if (m_coherentCount != signalsInCache) { m_coherentCount = signalsInCache; emit coherentCountChanged(); }
}

void DecodiumBridge::notifyNeuralSyncHit(double score)
{
    // score atteso 0..1 (es. % decodi recuperati da OSD nel periodo).
    double const clamped = qBound(0.0, score, 1.0);
    bool const on = clamped > 0.0;
    if (m_ledNeuralSync != on)   { m_ledNeuralSync = on;   emit ledNeuralSyncChanged(); }
    if (qAbs(m_neuralScore - clamped) > 1e-4) { m_neuralScore = clamped; emit neuralScoreChanged(); }
}

void DecodiumBridge::notifyTurboIterations(int itersUsed)
{
    int const v = qMax(0, itersUsed);
    // LED on se il decoder ha lavorato oltre la soglia base (= recupero borderline).
    bool const on = v > 8;
    if (m_ledTurboFeedback != on)  { m_ledTurboFeedback = on; emit ledTurboFeedbackChanged(); }
    if (m_turboIterations != v)    { m_turboIterations = v;   emit turboIterationsChanged(); }
}

