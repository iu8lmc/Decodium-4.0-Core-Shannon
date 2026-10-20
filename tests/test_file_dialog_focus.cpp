#include <QFile>
#include <QtTest>

namespace {

QString readSource(const QString& relativePath)
{
    QFile file(QStringLiteral(DECODIUM_SOURCE_DIR "/") + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

} // namespace

class TestFileDialogFocus final : public QObject
{
    Q_OBJECT

private slots:
    void openDialogFollowsQmlWindowAndActivates();
};

void TestFileDialogFocus::openDialogFollowsQmlWindowAndActivates()
{
    const QString source = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!source.isEmpty());

    const int openStart = source.indexOf(
        QStringLiteral("QString DecodiumBridge::openFileDialog"));
    const int saveStart = source.indexOf(
        QStringLiteral("QString DecodiumBridge::saveFileDialog"), openStart);
    QVERIFY(openStart >= 0);
    QVERIFY(saveStart > openStart);

    const QString implementation = source.mid(openStart, saveStart - openStart);
    QVERIFY(implementation.contains(QStringLiteral("dialogOwnerWindow(widgetParent)")));
    QVERIFY(implementation.contains(
        QStringLiteral("prepareFileDialogForActivation(dialog, ownerWindow)")));
    QVERIFY(implementation.contains(QStringLiteral("dialog.exec()")));

    const int helperStart = source.indexOf(
        QStringLiteral("static void prepareFileDialogForActivation"));
    QVERIFY(helperStart >= 0);
    const QString activation = source.mid(helperStart, openStart - helperStart);
    QVERIFY(activation.contains(QStringLiteral("setTransientParent(ownerWindow)")));
    QVERIFY(activation.contains(QStringLiteral("dialog.show()")));
    QVERIFY(activation.contains(QStringLiteral("dialog.raise()")));
    QVERIFY(activation.contains(QStringLiteral("dialog.activateWindow()")));
    QVERIFY(activation.contains(QStringLiteral("requestActivate()")));
}

QTEST_APPLESS_MAIN(TestFileDialogFocus)
#include "test_file_dialog_focus.moc"
