#include "DecodiumCatShare.h"
#include "DecodiumTransceiverManager.h"

namespace {
DecodiumCatShare::Hooks rigHooks(DecodiumTransceiverManager* manager)
{
    QPointer<DecodiumTransceiverManager> rig(manager);
    DecodiumCatShare::Hooks hooks;
    hooks.state = [rig]() {
        DecodiumCatShare::State state;
        if (!rig) return state;
        state.connected = rig->connected();
        state.frequency = rig->frequency();
        state.txFrequency = rig->txFrequency();
        state.mode = rig->mode();
        state.ptt = rig->pttActive();
        state.split = rig->split();
        state.strengthValid = rig->strengthValid();
        state.strengthDb = rig->strengthDb();
        state.swr = rig->swr();
        state.alcValid = rig->alcValid();
        state.alc = rig->alc();
        state.powerWatts = rig->powerWatts();
        return state;
    };
    hooks.frequency = [rig](double hz, bool tx) {
        if (!rig) return;
        if (tx) rig->setRigTxFrequency(hz);
        else rig->setRigFrequency(hz);
    };
    hooks.mode = [rig](const QString& mode) { if (rig) rig->setRigMode(mode); };
    hooks.ptt = [rig](bool on) { if (rig) rig->setRigPtt(on); };
    hooks.morseAvailable = [rig]() { return rig && rig->rigMorseAvailable(); };
    hooks.morseTextLimit = [rig]() { return rig ? rig->rigMorseTextLimit() : 0; };
    hooks.sendMorse = [rig](const QString& text, DecodiumCatShare::Completion done) {
        if (!rig || !rig->rigMorseAvailable()) { done(-11); return; }
        // Zero means preserve the radio's actual speed, read/set separately
        // through KEYSPD. sendRigMorse also verifies CW mode on the worker.
        if (!rig->sendRigMorse(text, 0, [done](const QString& error) {
            done(error.isEmpty() ? 0 : -6);
        })) done(-6);
    };
    hooks.morseSpeed = [rig](bool write, int wpm, DecodiumCatShare::SpeedCompletion done) {
        if (rig) rig->requestRigMorseSpeed(write, wpm, std::move(done));
        else done(-6, 0);
    };
    hooks.stopMorse = [rig](DecodiumCatShare::Completion done) {
        if (rig) rig->stopRigMorse(std::move(done));
        else done(-6);
    };
    return hooks;
}
}

DecodiumCatShare::DecodiumCatShare(DecodiumTransceiverManager* rig, QObject* parent)
    : DecodiumCatShare(rigHooks(rig), parent)
{
    if (!rig) return;
    connect(rig, &DecodiumTransceiverManager::morseStopRequested,
            this, &DecodiumCatShare::cancelMorse);
    connect(rig, &DecodiumTransceiverManager::connectedChanged, this, [this, rig]() {
        if (!rig->connected()) cancelMorse();
    });
    connect(rig, &QObject::destroyed, this, &DecodiumCatShare::cancelMorse);
}
