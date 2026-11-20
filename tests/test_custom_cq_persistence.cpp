#include <QFile>
#include <QtTest>

namespace {
QString readBridge()
{
    QFile file(QStringLiteral(DECODIUM_SOURCE_DIR "/src/bridge/DecodiumBridge.cpp"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QString::fromUtf8(file.readAll());
}

QString readStation()
{
    QFile file(QStringLiteral(DECODIUM_SOURCE_DIR "/src/bridge/DecodiumBridgeStation.cpp"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QString::fromUtf8(file.readAll());
}

QString readTxMessages()
{
    QFile file(QStringLiteral(DECODIUM_SOURCE_DIR "/src/bridge/DecodiumBridgeTxMessages.cpp"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QString::fromUtf8(file.readAll());
}

QString readPart(const char *name)
{
    QFile file(QString::fromUtf8(DECODIUM_SOURCE_DIR "/src/bridge/") + QString::fromUtf8(name));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QString::fromUtf8(file.readAll());
}

QString functionBody(QString const& source, QString const& signature, QString const& next)
{
    int const start = source.indexOf(signature);
    if (start < 0) return {};
    int const finish = source.indexOf(next, start + signature.size());
    return source.mid(start, finish > start ? finish - start : -1);
}
}

class TestCustomCqPersistence final : public QObject
{
    Q_OBJECT

private slots:
    void editedTx6SurvivesRegeneration();
    void editedTx6IsPersistedAndMigrated();
    void standardResetClearsTheOverride();
};

void TestCustomCqPersistence::editedTx6SurvivesRegeneration()
{
    QString const source = readBridge();
    QVERIFY(!source.isEmpty());
    QString const regenerate = functionBody(readStation(),
        QStringLiteral("void DecodiumBridge::regenerateTxMessages()"),
        QStringLiteral("double DecodiumBridge::frequency() const"));
    QString const qso = functionBody(readPart("DecodiumBridgeDecodeClick.cpp"),
        QStringLiteral("void DecodiumBridge::genStdMsgs("),
        QStringLiteral("bool DecodiumBridge::maybeStartCourtesySignoff("));
    QVERIFY(regenerate.contains(QStringLiteral("m_customCqMessage.isEmpty()")));
    QVERIFY(regenerate.contains(QStringLiteral(": m_customCqMessage")));
    QVERIFY(qso.contains(QStringLiteral("m_customCqMessage.isEmpty()")));
    QVERIFY(qso.contains(QStringLiteral(": m_customCqMessage")));

    QString const setMessage = functionBody(readTxMessages(),
        QStringLiteral("void DecodiumBridge::setTxMessage("),
        QStringLiteral("QString DecodiumBridge::validateTxMessage"));
    QVERIFY(setMessage.contains(QStringLiteral("m_customCqMessage = normalized")));
    QVERIFY(setMessage.contains(QStringLiteral("decodiumBridgeStandardFtxCq")));
    QFile helpers(QStringLiteral(DECODIUM_SOURCE_DIR "/src/bridge/FtxMessageHelpers.cpp"));
    QVERIFY(helpers.open(QIODevice::ReadOnly | QIODevice::Text));
    QVERIFY(QString::fromUtf8(helpers.readAll()).contains(QStringLiteral("return standardFtxCqMessage(call, grid, mode)")));
}

void TestCustomCqPersistence::editedTx6IsPersistedAndMigrated()
{
    QString const source = readPart("DecodiumBridgeSaveSettings.cpp") + readPart("DecodiumBridgeLoadSettings.cpp");
    QVERIFY(source.contains(QStringLiteral("s.setValue(\"customTx6Message\", m_customCqMessage)")));
    QVERIFY(source.contains(QStringLiteral("m_customCqMessage = s.value(\"customTx6Message\"")));
    QVERIFY(source.contains(QStringLiteral("!s.contains(\"customTx6Message\")")));
    QVERIFY(source.contains(QStringLiteral("m_customCqMessage = m_tx6.trimmed().toUpper()")));
}

void TestCustomCqPersistence::standardResetClearsTheOverride()
{
    QString const reset = functionBody(readTxMessages(),
        QStringLiteral("void DecodiumBridge::resetStandardTxMessages()"),
        QStringLiteral("void DecodiumBridge::stopTx()"));
    QVERIFY(reset.contains(QStringLiteral("m_customCqMessage.clear()")));
    QVERIFY(reset.contains(QStringLiteral("regenerateTxMessages()")));
}

QTEST_APPLESS_MAIN(TestCustomCqPersistence)
#include "test_custom_cq_persistence.moc"
