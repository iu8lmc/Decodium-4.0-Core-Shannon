#include <QtTest>
#include <QQuickWindow>
#include <atomic>
#include "src/ui/WorldMapGpuItem.hpp"

class CountingMap : public WorldMapGpuItem
{
public:
    using WorldMapGpuItem::WorldMapGpuItem;
    std::atomic<int> frames {0};
protected:
    QSGNode* updatePaintNode(QSGNode* node, UpdatePaintNodeData* data) override
    {
        ++frames;
        return WorldMapGpuItem::updatePaintNode(node, data);
    }
};

class MapFrameCadenceTest : public QObject
{
    Q_OBJECT
private slots:
    void idleAndAnimatedContent()
    {
        QQuickWindow window;
        window.resize(640, 360);
        auto* map = new CountingMap(window.contentItem());
        map->setSize(QSizeF(640, 360));
        map->setHomeGrid(QStringLiteral("JM75"));
        // Keep this phase static; automatic home-grid zoom is intentionally
        // animated and must not be mistaken for redundant idle rendering.
        map->setViewportState({{"centerLongitude", 0.0}, {"centerLatitude", 0.0},
                               {"spanLongitude", 360.0}, {"spanLatitude", 180.0},
                               {"locked", true}});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTRY_VERIFY(map->frames.load() > 0);
        QTest::qWait(1000);
        auto framesDuring = [map](int ms) {
            int const before = map->frames.load();
            QTest::qWait(ms);
            return map->frames.load() - before;
        };
        int const idle = framesDuring(2500);
        qInfo() << "idle frames in 2500ms:" << idle;
        QVERIFY2(idle >= 2 && idle <= 4, "Idle map must refresh UTC seconds without animation-rate redraws");

        map->addContact(QStringLiteral("K1ABC"), QStringLiteral("JM75"), QStringLiteral("FN42"));
        int const animated = framesDuring(1000);
        qInfo() << "contact frames in 1000ms:" << animated;
        QVERIFY(animated >= 6);
        map->clearContacts();
        // Let the auto-fit viewport settle after removing the contact.
        QTest::qWait(3000);
        QVERIFY(framesDuring(2500) <= 4);

        QVariantMap quake {{"type", "earthquake"}, {"longitude", 14.0}, {"latitude", 36.0}, {"magnitude", 4.0}};
        map->setGeographicFeatures({quake});
        QVERIFY(framesDuring(1000) >= 6);
        map->setGeographicFeatures({});
        QTest::qWait(250);
        QVERIFY(framesDuring(2500) <= 4);

        map->setTransmitState(true, QStringLiteral("K1ABC"), QStringLiteral("FN42"), QStringLiteral("FT8"));
        QVERIFY(framesDuring(1000) >= 6);
        map->setTransmitState(false, {}, {}, QStringLiteral("FT8"));

        // Direct user changes must still repaint immediately between clock ticks.
        int before = map->frames.load();
        map->setHomeGrid(QStringLiteral("FN42"));
        QTRY_VERIFY_WITH_TIMEOUT(map->frames.load() > before, 250);
        map->setViewportState({{"centerLongitude", 20.0}, {"centerLatitude", 10.0},
                               {"spanLongitude", 180.0}, {"spanLatitude", 90.0}});
        QVERIFY(framesDuring(1000) >= 6);
        map->setActive(false);
        QTest::qWait(250);
        QCOMPARE(framesDuring(1000), 0);
        before = map->frames.load();
        map->setActive(true);
        QTRY_VERIFY(map->frames.load() > before);
    }
};
QTEST_MAIN(MapFrameCadenceTest)
#include "test_world_map_frame_cadence.moc"
