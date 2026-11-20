#include <QtTest>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <cstring>
#include <utility>

#include "src/app/DecodiumUpdater.hpp"

class FakeReply final : public QNetworkReply
{
public:
    FakeReply(const QNetworkRequest& request, QByteArray body, bool failure, QObject* parent)
        : QNetworkReply(parent), body_(std::move(body))
    {
        setRequest(request);
        setUrl(request.url());
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        if (failure) setError(QNetworkReply::ContentAccessDenied, QStringLiteral("HTTP 403"));
        QTimer::singleShot(0, this, [this] { setFinished(true); emit finished(); });
    }
    void abort() override {}
    qint64 bytesAvailable() const override { return body_.size()-offset_+QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char* data, qint64 maxSize) override
    {
        const qint64 count=qMin(maxSize,body_.size()-offset_);
        if (count<=0) return -1;
        std::memcpy(data,body_.constData()+offset_,static_cast<std::size_t>(count));
        offset_+=count;
        return count;
    }
private:
    QByteArray body_;
    qint64 offset_ {0};
};

class FakeNetwork final : public QNetworkAccessManager
{
public:
    struct Response { QByteArray body; bool failure; };
    QList<Response> responses;
    QList<QUrl> requests;
    QList<int> timeouts;
protected:
    QNetworkReply* createRequest(Operation, const QNetworkRequest& request, QIODevice*) override
    {
        requests.append(request.url());
        timeouts.append(request.transferTimeout());
        if (responses.isEmpty()) return new FakeReply(request,{},true,this);
        const auto response=responses.takeFirst();
        return new FakeReply(request,response.body,response.failure,this);
    }
};

class TestDecodiumUpdater final : public QObject
{
    Q_OBJECT
private:
    QTemporaryDir settingsRoot_;
    QSettings settings() const {
        return QSettings(QSettings::IniFormat,QSettings::UserScope,
                         QStringLiteral("Decodium"),QStringLiteral("Decodium3"));
    }
    static QByteArray release(const QString& version) {
        return QByteArray("{\"tag_name\":\"v")+version.toUtf8()+"\",\"assets\":[]}";
    }
private slots:
    void initTestCase() {
        QVERIFY(settingsRoot_.isValid());
        QStandardPaths::setTestModeEnabled(true);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settingsRoot_.path());
        qunsetenv("DECODIUM_UPDATE_FAKE_CURRENT");
    }
    void init() { auto s=settings(); s.clear(); s.sync(); }
    void failedChecksDoNotSuppressRetry_data() {
        QTest::addColumn<int>("scenario");
        QTest::newRow("both 403") << 0;
        QTest::newRow("primary current, fallback 403") << 1;
        QTest::newRow("primary 403, fallback current") << 2;
        QTest::newRow("malformed primary, fallback current") << 3;
    }
    void failedChecksDoNotSuppressRetry() {
        QFETCH(int,scenario);
        FakeNetwork network;
        DecodiumUpdater updater(nullptr,&network);
        const auto current=release(updater.currentVersion());
        network.responses = scenario==0 ? QList<FakeNetwork::Response>{{{},true},{{},true}}
            : scenario==1 ? QList<FakeNetwork::Response>{{current,false},{{},true}}
            : scenario==2 ? QList<FakeNetwork::Response>{{{},true},{current,false}}
            : QList<FakeNetwork::Response>{{QByteArrayLiteral("{\"tag_name\":\"not-a-version\"}"),false},{current,false}};
        QSignalSpy errors(&updater,&DecodiumUpdater::errorOccurred);
        QSignalSpy currentSignal(&updater,&DecodiumUpdater::upToDate);
        updater.check(false);
        QTRY_VERIFY_WITH_TIMEOUT(!updater.busy(),3000);
        QCOMPARE(network.requests.size(),2);
        QVERIFY(network.requests[0].path().contains(QStringLiteral("/elisir80/")));
        QVERIFY(network.requests[1].path().contains(QStringLiteral("/iu8lmc/")));
        QCOMPARE(network.timeouts,QList<int>({15000,15000}));
        QCOMPARE(errors.size(),1);
        QCOMPARE(currentSignal.size(),0);
        QVERIFY(!settings().contains(QStringLiteral("Update/LastCheckUtc")));
    }
    void newestReleaseSelectionAndRetry_data() {
        QTest::addColumn<bool>("primaryFails");
        QTest::newRow("primary current") << false;
        QTest::newRow("primary unavailable") << true;
    }
    void newestReleaseSelectionAndRetry() {
        QFETCH(bool,primaryFails);
        FakeNetwork network;
        DecodiumUpdater updater(nullptr,&network);
        network.responses={{primaryFails ? QByteArray() : release(updater.currentVersion()),primaryFails},
                           {release(QStringLiteral("999.0.0")),false}};
        QSignalSpy found(&updater,&DecodiumUpdater::updateFound);
        updater.check(true);
        QTRY_VERIFY_WITH_TIMEOUT(!updater.busy(),3000);
        QCOMPARE(found.size(),1);
        QCOMPARE(updater.latestVersion(),QStringLiteral("999.0.0"));
        QVERIFY(updater.releaseRepository().startsWith(QStringLiteral("iu8lmc/")));
        QVERIFY(settings().contains(QStringLiteral("Update/LastCheckUtc")));
    }
    void primaryNewerStopsBeforeFallback() {
        FakeNetwork network;
        network.responses={{release(QStringLiteral("999.0.0")),false}};
        DecodiumUpdater updater(nullptr,&network);
        QSignalSpy found(&updater,&DecodiumUpdater::updateFound);
        updater.check(true);
        QTRY_VERIFY_WITH_TIMEOUT(!updater.busy(),3000);
        QCOMPARE(network.requests.size(),1);
        QCOMPARE(found.size(),1);
        QVERIFY(updater.releaseRepository().startsWith(QStringLiteral("elisir80/")));
    }
};
QTEST_GUILESS_MAIN(TestDecodiumUpdater)
#include "test_decodium_updater.moc"
