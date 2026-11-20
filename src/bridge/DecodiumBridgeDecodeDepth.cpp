#include "DecodiumBridge.h"
#include "DecodiumProfileSettings.h"
#include <QSettings>

static int modernDepthToLegacyBits(int depth, bool avgDecodeEnabled, bool deepSearchEnabled)
{
    int bits = qBound(1, depth, 3);
    if (avgDecodeEnabled) {
        bits |= 0x10;
    }
    if (deepSearchEnabled) {
        bits |= 0x20;
    }
    return bits;
}

int DecodiumBridge::nfa() const { return m_nfa; }
void DecodiumBridge::setNfa(int v) { if (m_nfa!=v) { m_nfa=v; emit nfaChanged(); } }
int DecodiumBridge::nfb() const { return m_nfb; }
void DecodiumBridge::setNfb(int v) { if (m_nfb!=v) { m_nfb=v; emit nfbChanged(); } }
int DecodiumBridge::ndepth() const { return m_ndepth; }
void DecodiumBridge::setNdepth(int v)
{
    int const depth = qBound(1, v, 4);
    if (m_ndepth == depth) {
        return;
    }

    m_ndepth = depth;
    int const legacyBits = legacyCompatibleDecodeDepthBits();

    QSettings s(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(s);
    s.setValue(QStringLiteral("ndepth"), m_ndepth);
    s.setValue(QStringLiteral("NDepth"), legacyBits);
    s.setValue(QStringLiteral("decodeDepthMigratedFromLegacy"), true);
    s.sync();
    syncSettingToLegacyIni(QStringLiteral("NDepth"), legacyBits);
    syncSettingToLegacyIni(QStringLiteral("ndepth"), m_ndepth);

    emit ndepthChanged();
    emit settingValueChanged(QStringLiteral("ndepth"), m_ndepth);
    emit settingValueChanged(QStringLiteral("NDepth"), legacyBits);
}
bool DecodiumBridge::singleDecode() const { return m_singleDecode; }
void DecodiumBridge::setSingleDecode(bool v)
{
    if (m_singleDecode == v) {
        return;
    }

    m_singleDecode = v;

    QSettings s(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(s);
    s.setValue(QStringLiteral("SingleDecode"), m_singleDecode);
    s.sync();
    syncSettingToLegacyIni(QStringLiteral("SingleDecode"), m_singleDecode);

    emit singleDecodeChanged();
    emit settingValueChanged(QStringLiteral("SingleDecode"), m_singleDecode);
}
int DecodiumBridge::effectiveFt4ThreadLimit() const
{
    int const baseLimit = effectiveFtThreadLimit();
    if (ft4LatencyGuardActive() || ft4AdaptiveCpuLimitActive()) {
        return qMin(baseLimit, 4);
    }
    return baseLimit;
}

int DecodiumBridge::effectiveFt4DecodeDepth(int requestedDepth) const
{
    int depth = qBound(1, requestedDepth, 4);
    bool const canUseDepth4 = m_deepSearchEnabled
        && m_ft8ApEnabled
        && !m_lowCpuModeEnabled
        && !cpuPressureActive()
        && effectiveFt4ThreadLimit() >= 16;
    if (depth > 3 && !canUseDepth4) {
        depth = 3;
    }
    if (ft4LatencyGuardActive() || cpuPressureSevereActive()) {
        depth = qMin(depth, 2);
    } else if (ft4AdaptiveCpuLimitActive() || cpuPressureActive()) {
        depth = qMin(depth, 3);
    }
    return depth;
}

int DecodiumBridge::effectiveDecodeDepth() const
{
    QString const normalizedMode = m_mode.trimmed().toUpper();
    int depth = qBound(1, m_ndepth, 4);
    if (normalizedMode == QStringLiteral("FT4")) {
        if (m_deepSearchEnabled && m_ft8ApEnabled && depth < 4) {
            depth = 4;
        } else if (!(m_deepSearchEnabled && m_ft8ApEnabled)) {
            depth = qMin(depth, 3);
        }
    } else if ((m_avgDecodeEnabled || m_deepSearchEnabled) && depth < 4) {
        depth = 4;
    }

    if (m_lowCpuModeEnabled) {
        if (normalizedMode == QStringLiteral("FT8")
            || normalizedMode == QStringLiteral("FT4")
            || normalizedMode == QStringLiteral("FT2")) {
            depth = qMin(depth, 2);
        }
    }
    return depth;
}

int DecodiumBridge::legacyCompatibleDecodeDepthBits() const
{
    int const eff = effectiveDecodeDepth();
    bool const pressure = m_lowCpuModeEnabled;
    int const bits = modernDepthToLegacyBits(eff,
                                             pressure ? false : m_avgDecodeEnabled,
                                             pressure ? false : m_deepSearchEnabled);
    return bits;
}

int DecodiumBridge::legacyDecodeQsoProgress() const
{
    return qBound(0, m_qsoProgress, 6);
}

int DecodiumBridge::legacyDecodeCqHint() const
{
    // CQ Only is a presentation/user filter in Decodium4.  Passing it down as
    // lapcqonly makes the FT decoder run a narrower AP/CQ plan and can reduce
    // raw decode yield compared with JTDX/WSJT-X, especially after downgrades
    // where the persisted CQ-only flag remains enabled.
    return 0;
}

