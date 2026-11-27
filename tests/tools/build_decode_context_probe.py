#!/usr/bin/env python3
"""Build temporary callback instrumentation; restore source even on build failure.

Run profile_mode_cycles.py --probe-late-callbacks afterwards. This build is lab-only;
rebuild decodium_qml after testing to remove the hook from the executable.
"""
from pathlib import Path
import subprocess
ROOT = Path(__file__).resolve().parents[2]
source = ROOT / 'src/bridge/DecodiumBridgeSstv.cpp'
original = source.read_bytes()
anchor = b'QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'
assert original.count(anchor) == 1
hook = r'''
    if (QCoreApplication::arguments().contains(QStringLiteral("--test-mode"))
        && values.value(QStringLiteral("__contextProbe")).toBool()) {
        m_filtersBypassed = true;
        auto containsMessage = [this](const QString& message) {
            for (const QVariant& item : std::as_const(m_decodeList))
                if (item.toMap().value(QStringLiteral("message")).toString() == message) return true;
            return false;
        };
        QVariantList cases;
        for (const QString& mode : {QStringLiteral("Q65"), QStringLiteral("WSPR"), QStringLiteral("JT65")}) {
            setMode(mode);
            quint64 const oldSerial = ++m_decodeSerial;
            setMode(QStringLiteral("FT4"));
            setMode(mode);
            auto deliver = [this, &mode](quint64 serial, const QString& call) {
                if (mode == QStringLiteral("WSPR")) {
                    onWsprDecodeReady(serial, {QStringLiteral("2256 -21 -0.3 14.097090 0 ") + call + QStringLiteral(" FN31 37")}, {}, 0);
                } else if (mode == QStringLiteral("JT65")) {
                    QString const row = QStringLiteral("1200") + QStringLiteral("-10").rightJustified(4)
                        + QStringLiteral("0.1").rightJustified(5) + QStringLiteral("1500").rightJustified(5)
                        + QStringLiteral(" #  ") + (QStringLiteral("CQ ") + call + QStringLiteral(" FN31")).leftJustified(22);
                    onLegacyJtDecodeReady(serial, {row});
                } else {
                    onQ65DecodeReady(serial, {QStringLiteral("120000 -10 0.1 1500 :  CQ ") + call + QStringLiteral(" FN31")});
                }
            };
            QTemporaryFile temporary;
            bool const tempCreated = mode != QStringLiteral("WSPR") || temporary.open();
            if (mode == QStringLiteral("WSPR") && tempCreated) {
                temporary.close();
                m_wsprDecodeTempFiles.insert(oldSerial, temporary.fileName());
            }
            deliver(oldSerial, QStringLiteral("W1XYZ"));
            bool const tempCleaned = mode != QStringLiteral("WSPR")
                || (tempCreated && !QFile::exists(temporary.fileName()) && !m_wsprDecodeTempFiles.contains(oldSerial));
            bool const oldAccepted = containsMessage(mode == QStringLiteral("WSPR") ? QStringLiteral("W1XYZ FN31 37") : QStringLiteral("CQ W1XYZ FN31"));
            quint64 const freshSerial = ++m_decodeSerial;
            // Time-sync FT callbacks already require mode metadata. WSPR/JT permit
            // expired metadata; preserve their slow current-session completions.
            if (mode == QStringLiteral("Q65")) {
                m_decodeModeBySerial.insert(freshSerial, m_mode);
                m_decodeSessionBySerial.insert(freshSerial, m_decodeSessionId);
            }
            deliver(freshSerial, QStringLiteral("K1ABC"));
            bool const freshAccepted = containsMessage(mode == QStringLiteral("WSPR") ? QStringLiteral("K1ABC FN31 37") : QStringLiteral("CQ K1ABC FN31"));
            cases.append(QVariantMap{{"mode", mode}, {"oldAccepted", oldAccepted}, {"freshAccepted", freshAccepted}, {"tempCleaned", tempCleaned}});
        }
        return {{"probeExecuted", true}, {"cases", cases}, {"tx", m_transmitting}, {"cat", m_catConnected}};
    }
'''.encode()
try:
    source.write_bytes(b'#include <QTemporaryFile>\n' + original.replace(anchor, anchor + hook, 1))
    subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '4'],
                   cwd=ROOT, check=True)
finally:
    source.write_bytes(original)
