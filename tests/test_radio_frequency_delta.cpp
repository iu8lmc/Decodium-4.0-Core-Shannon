#include <QtTest>

#include <QSettings>
#include <QTemporaryDir>

#include "models/StationList.hpp"
#include "src/radio/Radio.hpp"

class TestRadioFrequencyDelta final : public QObject
{
    Q_OBJECT

private slots:
    void negativeMHzOffsetIsAccepted()
    {
        bool ok = false;
        Radio::FrequencyDelta const offset =
            Radio::frequency_delta(QVariant(QStringLiteral("-2556")),
                                   6, &ok, QLocale::c());

        QVERIFY(ok);
        QCOMPARE(offset, Radio::FrequencyDelta(-2'556'000'000LL));
    }

    void negativeStationOffsetSurvivesIniRoundTrip()
    {
        qRegisterMetaType<StationList::Station>("Station");
        qRegisterMetaType<StationList::Stations>("Stations");
        QMetaType::registerConverter<StationList::Station, QString>(
            &StationList::Station::toString);

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QString const path = directory.filePath(QStringLiteral("Decodium.ini"));
        StationList::Stations const expected {
            StationList::Station {QStringLiteral("13cm"),
                                  Radio::FrequencyDelta(-2'556'000'000LL),
                                  QStringLiteral("QO-100 uplink")}
        };

        {
            QSettings settings(path, QSettings::IniFormat);
            settings.setValue(QStringLiteral("stations"), QVariant::fromValue(expected));
            settings.sync();
            QCOMPARE(settings.status(), QSettings::NoError);
        }

        QSettings reloaded(path, QSettings::IniFormat);
        StationList::Stations const actual =
            reloaded.value(QStringLiteral("stations")).value<StationList::Stations>();
        QCOMPARE(actual, expected);
    }
};

QTEST_GUILESS_MAIN(TestRadioFrequencyDelta)
#include "test_radio_frequency_delta.moc"
