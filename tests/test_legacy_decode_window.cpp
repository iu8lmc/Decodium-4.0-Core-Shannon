#include "LegacyDecodeWindow.hpp"

#include <QtTest>

class TestLegacyDecodeWindow final : public QObject
{
    Q_OBJECT

private slots:
    void keepsShortSnapshotsIntact();
    void boundsWarmRunSnapshots();
    void supportsExplicitEmptyWindow();
};

void TestLegacyDecodeWindow::keepsShortSnapshotsIntact()
{
    QCOMPARE(decodium::legacy::recent_decode_window_start(250), 0);
    QCOMPARE(decodium::legacy::recent_decode_window_start(384), 0);
}

void TestLegacyDecodeWindow::boundsWarmRunSnapshots()
{
    int const start = decodium::legacy::recent_decode_window_start(1800);
    QCOMPARE(start, 1800 - decodium::legacy::kLiveDecodeSnapshotRows);
    QCOMPARE(1800 - start, decodium::legacy::kLiveDecodeSnapshotRows);
}

void TestLegacyDecodeWindow::supportsExplicitEmptyWindow()
{
    QCOMPARE(decodium::legacy::recent_decode_window_start(1800, 0), 1800);
    QCOMPARE(decodium::legacy::recent_decode_window_start(0), 0);
}

QTEST_MAIN(TestLegacyDecodeWindow)
#include "test_legacy_decode_window.moc"
