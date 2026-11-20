static QThreadPool& decodeTextLogThreadPool()
{
    static QThreadPool* pool = [] {
        auto* configured = new QThreadPool;
        configured->setMaxThreadCount(1);
        configured->setExpiryTimeout(30000);
        return configured;
    }();
    return *pool;
}

static QThreadPool& settingsWriteThreadPool()
{
    static QThreadPool* pool = [] {
        auto* configured = new QThreadPool;
        configured->setMaxThreadCount(1);
        configured->setExpiryTimeout(30000);
        return configured;
    }();
    return *pool;
}

QThreadPool& decodiumBridgeSettingsWriteThreadPool()
{
    return settingsWriteThreadPool();
}

static void appendDecodeTextBatchesAsync(QString primaryPath,
                                         QString primaryText,
                                         QString legacyPath,
                                         QString legacyText)
{
    if (primaryText.isEmpty() && legacyText.isEmpty()) {
        return;
    }
    decodeTextLogThreadPool().start(
        [primaryPath = std::move(primaryPath),
         primaryText = std::move(primaryText),
         legacyPath = std::move(legacyPath),
         legacyText = std::move(legacyText)]() {
        auto appendText = [](QString const& path, QString const& text) {
            if (path.isEmpty() || text.isEmpty()) {
                return;
            }
            QDir().mkpath(QFileInfo(path).absolutePath());
            QFile file(path);
            if (!file.open(QIODevice::Append | QIODevice::Text)) {
                return;
            }
            QTextStream out(&file);
            out << text;
        };

        // logAllTxtPath commonly aliases legacyAllTxtPath. One write keeps
        // the previous batch order while avoiding two file opens per decode.
        if (!primaryPath.isEmpty() && primaryPath == legacyPath) {
            appendText(primaryPath, primaryText + legacyText);
            return;
        }
        appendText(primaryPath, primaryText);
        appendText(legacyPath, legacyText);
    });
}

void decodiumBridgeAppendDecodeTextBatchesAsync(QString a, QString b, QString c, QString d)
{
    appendDecodeTextBatchesAsync(std::move(a), std::move(b), std::move(c), std::move(d));
}

