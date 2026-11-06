// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include <QImage>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickImageProvider>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QVariantMap>

namespace {

QString qmlErrors(const QList<QQmlError>& errors)
{
    QStringList lines;
    lines.reserve(errors.size());
    for (const QQmlError& error : errors) {
        lines.push_back(error.toString());
    }
    return lines.join(QLatin1Char('\n'));
}

class PreviewProvider final : public QQuickImageProvider
{
public:
    PreviewProvider()
        : QQuickImageProvider(QQuickImageProvider::Image)
    {
    }

    QImage requestImage(const QString& id,
                        QSize* size,
                        const QSize& requestedSize) override
    {
        Q_UNUSED(requestedSize)
        lastId = id;
        QImage image(320, 256, QImage::Format_RGB32);
        image.fill(QColor(QStringLiteral("#196c86")));
        if (size) {
            *size = image.size();
        }
        return image;
    }

    QString lastId;
};

class QsoEngineFixture final : public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE QVariantList sstvExistingQsoChoices(
        const QString& search, int maximumRows)
    {
        lastSearch = search;
        lastMaximumRows = maximumRows;
        return existingChoices;
    }

    Q_INVOKABLE QVariantMap logSstvQso(const QVariantMap& request)
    {
        lastRequest = request;
        ++requestSerial;
        lastRequestToken = QString::number(requestSerial);
        return {
            {QStringLiteral("accepted"), acceptRequests},
            {QStringLiteral("requestId"),
             acceptRequests ? lastRequestToken : QString {}},
            {QStringLiteral("error"),
             acceptRequests ? QString {}
                            : QStringLiteral("fixture rejection")}
        };
    }

    void complete(bool qsoCreated,
                  bool associationStored,
                  const QString& qsoId,
                  const QString& error = {})
    {
        emit sstvQsoLogFinished(
            lastRequestToken,
            lastRequest.value(QStringLiteral("imageRecordId")).toString(),
            qsoCreated, associationStored, qsoId, error);
    }

    QVariantList existingChoices;
    QVariantMap lastRequest;
    QString lastSearch;
    QString lastRequestToken;
    int lastMaximumRows {0};
    quint64 requestSerial {9'007'199'254'740'990ULL};
    bool acceptRequests {true};

signals:
    void sstvQsoLogFinished(QString requestToken,
                            QString imageRecordId,
                            bool qsoCreated,
                            bool associationStored,
                            QString qsoId,
                            QString error);
    void qsoLogCacheChanged();
};

QVariant invokeVariant(QObject* object,
                       const char* method,
                       const QVariant& argument = {})
{
    QVariant result;
    bool invoked = false;
    if (argument.isValid()) {
        invoked = QMetaObject::invokeMethod(
            object, method, Q_RETURN_ARG(QVariant, result),
            Q_ARG(QVariant, argument));
    } else {
        invoked = QMetaObject::invokeMethod(
            object, method, Q_RETURN_ARG(QVariant, result));
    }
    if (!invoked) {
        return {};
    }
    return result;
}

} // namespace

class TestSstvQsoQml final : public QObject
{
    Q_OBJECT

private slots:
    void explicitNewAndExistingWorkflowsArePathFree()
    {
        static const QString imageId = QStringLiteral(
            "2acb07fa-f7f6-46da-9f1f-38d5ab75f3e1");
        static const QString existingQsoId = QStringLiteral(
            "adif-sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

        QsoEngineFixture fixture;
        fixture.existingChoices = {
            QVariantMap {
                {QStringLiteral("qsoId"), existingQsoId},
                {QStringLiteral("call"), QStringLiteral("9H1TEST")},
                {QStringLiteral("grid"), QStringLiteral("JM75FV")},
                {QStringLiteral("dateTime"),
                 QStringLiteral("2026-08-24T18:30:00Z")},
                {QStringLiteral("mode"), QStringLiteral("SSTV")},
                {QStringLiteral("band"), QStringLiteral("20M")}
            }
        };

        QQmlEngine engine;
        auto* provider = new PreviewProvider;
        engine.addImageProvider(QStringLiteral("decodium-sstv-gallery"),
                                provider);
        QStringList warnings;
        connect(&engine, &QQmlEngine::warnings, this,
                [&warnings](const QList<QQmlError>& values) {
                    for (const QQmlError& warning : values) {
                        warnings.push_back(warning.toString());
                    }
                });

        QQuickWindow window;
        window.resize(960, 760);
        window.show();
        const QString sourcePath = QString::fromUtf8(
            DECODIUM_SSTV_QSO_QML_SOURCE);
        QQmlComponent component(&engine, QUrl::fromLocalFile(sourcePath),
                                QQmlComponent::PreferSynchronous);
        QVERIFY2(component.isReady(),
                 qPrintable(qmlErrors(component.errors())));
        QVariantMap initial {
            {QStringLiteral("engine"),
             QVariant::fromValue(static_cast<QObject*>(&fixture))},
            {QStringLiteral("parent"),
             QVariant::fromValue(static_cast<QObject*>(window.contentItem()))}
        };
        QScopedPointer<QObject> dialog(
            component.createWithInitialProperties(initial));
        QVERIFY2(dialog, qPrintable(qmlErrors(component.errors())));

        QVariantMap imageRecord {
            {QStringLiteral("recordId"), imageId},
            {QStringLiteral("mode"), QStringLiteral("Martin M1")},
            {QStringLiteral("remoteCallsign"), QStringLiteral("9H1TEST")},
            {QStringLiteral("remoteGrid"), QStringLiteral("JM75FV")},
            {QStringLiteral("frequencyHz"), 14'230'000},
            {QStringLiteral("capturedAtUtc"),
             QStringLiteral("2026-08-24T18:30:00.000Z")},
            {QStringLiteral("relatedQsoId"), QString {}}
        };
        QVERIFY(invokeVariant(dialog.data(), "openForImage", imageRecord)
                    .toBool());
        QTRY_VERIFY_WITH_TIMEOUT(dialog->property("visible").toBool(), 2'000);
        QTRY_COMPARE_WITH_TIMEOUT(provider->lastId, imageId, 2'000);

        QObject* confirmation = dialog->findChild<QObject*>(
            QStringLiteral("sstvQsoExplicitConfirmation"));
        QObject* submit = dialog->findChild<QObject*>(
            QStringLiteral("sstvQsoSubmit"));
        QObject* comments = dialog->findChild<QObject*>(
            QStringLiteral("sstvQsoComments"));
        QVERIFY(confirmation);
        QVERIFY(submit);
        QVERIFY(comments);
        QVERIFY(!submit->property("enabled").toBool());
        comments->setProperty("text", QString(2'200, QLatin1Char('x')));
        QTRY_COMPARE_WITH_TIMEOUT(
            comments->property("text").toString().size(), 2'048, 2'000);
        comments->setProperty("text", QStringLiteral("Good analogue image"));
        confirmation->setProperty("checked", true);
        QTRY_VERIFY_WITH_TIMEOUT(submit->property("enabled").toBool(), 2'000);
        QVERIFY(invokeVariant(dialog.data(), "submit").toBool());
        QCOMPARE(fixture.lastRequest.value(
                     QStringLiteral("imageRecordId")).toString(), imageId);
        QCOMPARE(fixture.lastRequest.value(
                     QStringLiteral("createNewQso")).toBool(), true);
        QCOMPARE(fixture.lastRequest.value(
                     QStringLiteral("remoteCallsign")).toString(),
                 QStringLiteral("9H1TEST"));
        QCOMPARE(fixture.lastRequest.value(
                     QStringLiteral("frequencyHz")).toLongLong(),
                 14'230'000LL);
        QCOMPARE(fixture.lastRequest.value(
                     QStringLiteral("imageMode")).toString(),
                 QStringLiteral("Martin M1"));
        QVERIFY(!fixture.lastRequest.contains(QStringLiteral("imagePath")));
        QVERIFY(!fixture.lastRequest.contains(QStringLiteral("rawAudioPath")));
        QVERIFY(!fixture.lastRequest.contains(QStringLiteral("metadataPath")));

        QSignalSpy completed(dialog.data(), SIGNAL(completed(bool,QString)));
        fixture.complete(true, true,
                         QStringLiteral("adif-sha256:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"));
        QTRY_COMPARE_WITH_TIMEOUT(completed.count(), 1, 2'000);
        QTRY_VERIFY_WITH_TIMEOUT(!dialog->property("visible").toBool(), 2'000);

        imageRecord.insert(QStringLiteral("relatedQsoId"), existingQsoId);
        QVERIFY(invokeVariant(dialog.data(), "openForImage", imageRecord)
                    .toBool());
        QTRY_VERIFY_WITH_TIMEOUT(dialog->property("visible").toBool(), 2'000);
        QTRY_COMPARE_WITH_TIMEOUT(fixture.lastMaximumRows, 50, 2'000);
        QCOMPARE(dialog->property("selectedExistingQsoId").toString(),
                 existingQsoId);
        confirmation->setProperty("checked", true);
        QTRY_VERIFY_WITH_TIMEOUT(submit->property("enabled").toBool(), 2'000);
        QVERIFY(invokeVariant(dialog.data(), "submit").toBool());
        QCOMPARE(fixture.lastRequest.size(), 4);
        QCOMPARE(fixture.lastRequest.value(
                     QStringLiteral("createNewQso")).toBool(), false);
        QCOMPARE(fixture.lastRequest.value(
                     QStringLiteral("existingQsoId")).toString(),
                 existingQsoId);
        QVERIFY(!fixture.lastRequest.contains(QStringLiteral("frequencyHz")));
        QVERIFY(!fixture.lastRequest.contains(QStringLiteral("comments")));
        fixture.complete(false, true, existingQsoId);
        QTRY_COMPARE_WITH_TIMEOUT(completed.count(), 2, 2'000);
        QTRY_VERIFY_WITH_TIMEOUT(!dialog->property("visible").toBool(), 2'000);

        QFile source(sourcePath);
        QVERIFY(source.open(QIODevice::ReadOnly | QIODevice::Text));
        const QByteArray qml = source.readAll();
        QVERIFY(!qml.contains("imagePath"));
        QVERIFY(!qml.contains("rawAudioPath"));
        QVERIFY(qml.contains("image://decodium-sstv-gallery/"));
        QVERIFY2(warnings.isEmpty(),
                 qPrintable(warnings.join(QLatin1Char('\n'))));
    }
};

QTEST_MAIN(TestSstvQsoQml)
#include "test_sstv_qso_qml.moc"
