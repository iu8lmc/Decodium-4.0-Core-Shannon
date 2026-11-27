static QString expandedLocalFilePath(QString path)
{
    path = QDir::fromNativeSeparators(path.trimmed());
    if (path == QStringLiteral("~")) {
        return QDir::homePath();
    }
    if (path.startsWith(QStringLiteral("~/"))) {
        return QDir::home().absoluteFilePath(path.mid(2));
    }
    return path;
}

static bool ensureParentDirectoryForFile(QString const& filePath, QString* errorMessage = nullptr)
{
    QFileInfo const info(filePath);
    QDir dir = info.dir();
    if (dir.exists() || dir.mkpath(QStringLiteral("."))) {
        return true;
    }
    if (errorMessage) {
        *errorMessage = QStringLiteral("Impossibile creare la cartella: %1")
                            .arg(QDir::toNativeSeparators(dir.absolutePath()));
    }
    return false;
}

QString decodiumBridgeExpandedLocalFilePath(QString path) { return expandedLocalFilePath(path); }
bool decodiumBridgeEnsureParentDirectoryForFile(QString const& path, QString* error) { return ensureParentDirectoryForFile(path, error); }

static QString buildTxRecordingPath(QString const& mode, bool tune)
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (dir.isEmpty()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    dir += QStringLiteral("/Decodium/recordings");
    QString const stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    QString const tag = tune ? QStringLiteral("tune") : QStringLiteral("tx");
    QString const safeMode = mode.trimmed().isEmpty() ? QStringLiteral("FT8") : mode.trimmed().toUpper();
    return QDir(dir).absoluteFilePath(QStringLiteral("decodium_%1_%2_%3.wav").arg(tag, safeMode, stamp));
}

QString decodiumBridgeBuildTxRecordingPath(QString const& mode, bool tune) { return buildTxRecordingPath(mode, tune); }

static QString buildWsprDecodeWavPath(quint64 serial, QString const& utcToken)
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (dir.isEmpty()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }
    dir += QStringLiteral("/wspr/%1").arg(serial);
    QDateTime slotUtc = approxUtcDateTimeForDisplayToken(utcToken);
    if (!slotUtc.isValid()) {
        slotUtc = QDateTime::currentDateTimeUtc();
    }
    QString const date = slotUtc.toString(QStringLiteral("yyMMdd"));
    QString const time = slotUtc.toString(QStringLiteral("hhmm"));
    return QDir(dir).absoluteFilePath(QStringLiteral("%1_%2.wav").arg(date, time));
}

QString decodiumBridgeBuildWsprDecodeWavPath(quint64 serial, const QString& utc) { return buildWsprDecodeWavPath(serial, utc); }


