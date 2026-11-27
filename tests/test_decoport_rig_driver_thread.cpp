// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/net/DecoPortRigDriver.h"

#include <QElapsedTimer>
#include <QThread>
#include <QtTest>

class TestDecoPortRigDriverThread final : public QObject
{
    Q_OBJECT

private slots:
    void ownsDedicatedWorkerThread()
    {
        DecoPortRigDriver driver;

        QVERIFY(driver.parent() == nullptr);
        QVERIFY(driver.thread() != nullptr);
        QVERIFY(driver.thread() != QThread::currentThread());
        QVERIFY(driver.thread()->isRunning());
    }

    void rejectsInvalidOpenWithoutBlockingCaller()
    {
        DecoPortRigDriver driver;
        QElapsedTimer elapsed;
        elapsed.start();

        QVERIFY(!driver.open(QString(), 38400, 0, 1035));
        QVERIFY2(elapsed.elapsed() < 100,
                 "An invalid DecoPort open request blocked the caller");
    }
};

QTEST_GUILESS_MAIN(TestDecoPortRigDriverThread)
#include "test_decoport_rig_driver_thread.moc"
