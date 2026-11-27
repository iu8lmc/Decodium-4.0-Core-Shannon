#include "DecodiumBridge.h"
#include <QFontDatabase>
#include <QGuiApplication>

static QString resolveBridgeFontFamily(QString const& requestedFamily,
                                       QString const& fallbackFamily = QString {})
{
    static QStringList const availableFamilies = [] {
        QStringList families = QFontDatabase::families();
        families.removeDuplicates();
        return families;
    }();

    auto findInstalledFamily = [] (QString const& family) -> QString {
        QString const clean = family.trimmed();
        if (clean.isEmpty()) {
            return {};
        }
        for (QString const& available : availableFamilies) {
            if (available.compare(clean, Qt::CaseInsensitive) == 0) {
                return available;
            }
        }
        return {};
    };

    QStringList candidates;
    auto addCandidate = [&candidates] (QString const& candidate) {
        QString const clean = candidate.trimmed();
        if (!clean.isEmpty()) {
            candidates << clean;
        }
    };
    auto addPlatformSansCandidates = [&addCandidate] {
#if defined(Q_OS_MAC)
        addCandidate(QStringLiteral("Helvetica Neue"));
        addCandidate(QStringLiteral("Arial"));
#elif defined(Q_OS_WIN)
        addCandidate(QStringLiteral("Segoe UI"));
        addCandidate(QStringLiteral("Arial"));
#else
        addCandidate(QStringLiteral("Noto Sans"));
        addCandidate(QStringLiteral("DejaVu Sans"));
        addCandidate(QStringLiteral("Liberation Sans"));
        addCandidate(QStringLiteral("Sans Serif"));
#endif
    };

    for (QString const& token : requestedFamily.split(',', Qt::SkipEmptyParts)) {
        QString const clean = token.trimmed();
        if (clean.compare(QStringLiteral("monospace"), Qt::CaseInsensitive) == 0) {
#if defined(Q_OS_MAC)
            addCandidate(QStringLiteral("Menlo"));
            addCandidate(QStringLiteral("Monaco"));
#elif defined(Q_OS_WIN)
            addCandidate(QStringLiteral("Consolas"));
            addCandidate(QStringLiteral("Cascadia Mono"));
#else
            addCandidate(QStringLiteral("DejaVu Sans Mono"));
            addCandidate(QStringLiteral("Noto Sans Mono"));
            addCandidate(QStringLiteral("Liberation Mono"));
#endif
            addCandidate(QStringLiteral("Monospace"));
        } else if (clean.compare(QStringLiteral("MS Shell Dlg 2"), Qt::CaseInsensitive) == 0
                   || clean.compare(QStringLiteral("MS Shell Dlg"), Qt::CaseInsensitive) == 0
#if defined(Q_OS_WIN)
                   || clean.compare(QStringLiteral("MS Sans Serif"), Qt::CaseInsensitive) == 0
                   || clean.compare(QStringLiteral("MS Serif"), Qt::CaseInsensitive) == 0
                   || clean.compare(QStringLiteral("System"), Qt::CaseInsensitive) == 0
                   || clean.compare(QStringLiteral("Small Fonts"), Qt::CaseInsensitive) == 0
#endif
#if !defined(Q_OS_WIN)
                   || clean.compare(QStringLiteral("Segoe UI"), Qt::CaseInsensitive) == 0
#endif
                   ) {
            addPlatformSansCandidates();
        } else {
            addCandidate(clean);
        }
    }

    addCandidate(fallbackFamily);
    addCandidate(QGuiApplication::font().family());
    addPlatformSansCandidates();

    for (QString const& candidate : candidates) {
        QString const installed = findInstalledFamily(candidate);
        if (!installed.isEmpty()) {
            return installed;
        }
    }

    return availableFamilies.isEmpty() ? fallbackFamily.trimmed() : availableFamilies.first();
}

static QFont fallbackBridgeFont(QString const& fallbackFamily, int fallbackPointSize)
{
    static QFont const defaultApplicationFont = QGuiApplication::font();
    QFont font = defaultApplicationFont;
    if (!fallbackFamily.trimmed().isEmpty()) {
        font.setFamily(resolveBridgeFontFamily(fallbackFamily.trimmed(), defaultApplicationFont.family()));
    }
    if (fallbackPointSize > 0) {
        font.setPointSize(fallbackPointSize);
    } else if (font.pointSize() > 0 && font.pointSize() < 10) {
        font.setPointSize(10);
    }
    return font;
}

QFont DecodiumBridge::fontSettingFont(const QString& key,
                                      const QString& fallbackFamily,
                                      int fallbackPointSize) const
{
    QFont fallback = fallbackBridgeFont(fallbackFamily, fallbackPointSize);
    QString const raw = getSetting(key, fallback.toString()).toString().trimmed();
    if (raw.isEmpty()) {
        return fallback;
    }

    QFont parsed;
    if (parsed.fromString(raw)) {
        parsed.setFamily(resolveBridgeFontFamily(parsed.family(), fallback.family()));
        return parsed;
    }

    // Be tolerant of older/manual values that only contain a family name.
    fallback.setFamily(resolveBridgeFontFamily(raw, fallback.family()));
    return fallback;
}

QString DecodiumBridge::fontSettingLabel(const QString& key,
                                         const QString& fallbackFamily,
                                         int fallbackPointSize) const
{
    QFont const font = fontSettingFont(key, fallbackFamily, fallbackPointSize);
    int const pointSize = font.pointSize() > 0 ? font.pointSize() : fallbackBridgeFont(fallbackFamily, fallbackPointSize).pointSize();
    QStringList traits;
    if (font.bold()) traits << QStringLiteral("bold");
    if (font.italic()) traits << QStringLiteral("italic");
    QString label = QStringLiteral("%1, %2 pt").arg(font.family()).arg(pointSize);
    if (!traits.isEmpty()) {
        label += QStringLiteral(" (%1)").arg(traits.join(QStringLiteral(", ")));
    }
    return label;
}

QString DecodiumBridge::fontSettingFamily(const QString& key,
                                          const QString& fallbackFamily,
                                          int fallbackPointSize) const
{
    return fontSettingFont(key, fallbackFamily, fallbackPointSize).family();
}

int DecodiumBridge::fontSettingPixelSize(const QString& key,
                                         const QString& fallbackFamily,
                                         int fallbackPointSize) const
{
    QFont const font = fontSettingFont(key, fallbackFamily, fallbackPointSize);
    if (font.pixelSize() > 0) {
        return qBound(8, font.pixelSize(), 36);
    }
    int pointSize = font.pointSize();
    if (pointSize <= 0) {
        pointSize = fallbackBridgeFont(fallbackFamily, fallbackPointSize).pointSize();
    }
    return qBound(8, qRound(pointSize * 1.25), 36);
}

int DecodiumBridge::fontSettingPointSize(const QString& key,
                                         const QString& fallbackFamily,
                                         int fallbackPointSize) const
{
    QFont const font = fontSettingFont(key, fallbackFamily, fallbackPointSize);
    int pointSize = font.pointSize();
    if (pointSize <= 0) {
        pointSize = fallbackBridgeFont(fallbackFamily, fallbackPointSize).pointSize();
    }
    return qBound(6, pointSize, 48);
}

bool DecodiumBridge::fontSettingBold(const QString& key,
                                     const QString& fallbackFamily,
                                     int fallbackPointSize) const
{
    return fontSettingFont(key, fallbackFamily, fallbackPointSize).bold();
}

bool DecodiumBridge::fontSettingItalic(const QString& key,
                                       const QString& fallbackFamily,
                                       int fallbackPointSize) const
{
    return fontSettingFont(key, fallbackFamily, fallbackPointSize).italic();
}

QStringList DecodiumBridge::availableFontFamilies(bool fixedPitchOnly) const
{
    QStringList families = QFontDatabase::families();
    families.removeDuplicates();
    families.sort(Qt::CaseInsensitive);
    if (!fixedPitchOnly) {
        return families;
    }

    QStringList fixedFamilies;
    fixedFamilies.reserve(families.size());
    for (QString const& family : families) {
        if (QFontDatabase::isFixedPitch(family)) {
            fixedFamilies.append(family);
        }
    }
    return fixedFamilies.isEmpty() ? families : fixedFamilies;
}

void DecodiumBridge::setFontSetting(const QString& key,
                                    const QString& family,
                                    int pointSize,
                                    bool bold,
                                    bool italic,
                                    const QString& fallbackFamily,
                                    int fallbackPointSize)
{
    QFont font = fallbackBridgeFont(fallbackFamily, fallbackPointSize);
    QString const cleanFamily = family.trimmed();
    if (!cleanFamily.isEmpty()) {
        font.setFamily(cleanFamily);
    }
    font.setPointSize(qBound(6, pointSize > 0 ? pointSize : font.pointSize(), 48));
    font.setBold(bold);
    font.setItalic(italic);

    setSetting(key, font.toString());
    if (key == QStringLiteral("Font")) {
        QGuiApplication::setFont(font);
    }
    emit statusMessage(QStringLiteral("%1: %2").arg(key, fontSettingLabel(key, fallbackFamily, fallbackPointSize)));
}

void DecodiumBridge::chooseFontSetting(const QString& key,
                                       const QString& fallbackFamily,
                                       int fallbackPointSize)
{
    Q_UNUSED(fallbackFamily);
    Q_UNUSED(fallbackPointSize);
    emit statusMessage(QStringLiteral("%1: selettore font integrato disponibile nella UI").arg(key));
}

void DecodiumBridge::resetFontSetting(const QString& key,
                                      const QString& fallbackFamily,
                                      int fallbackPointSize)
{
    QFont const font = fallbackBridgeFont(fallbackFamily, fallbackPointSize);
    setSetting(key, font.toString());
    if (key == QStringLiteral("Font")) {
        QGuiApplication::setFont(font);
    }
    emit statusMessage(QStringLiteral("%1 ripristinato: %2").arg(key, fontSettingLabel(key, fallbackFamily, fallbackPointSize)));
}


void DecodiumBridge::increaseFontScale()
{
    setFontScale(m_fontScale + 0.1);
}

void DecodiumBridge::decreaseFontScale()
{
    setFontScale(m_fontScale - 0.1);
}

void DecodiumBridge::setFontScale(double s)
{
    double clamped = qBound(0.5, s, 2.0);
    if (!qFuzzyCompare(m_fontScale, clamped)) {
        m_fontScale = clamped;
        emit fontScaleChanged();
    }
}

