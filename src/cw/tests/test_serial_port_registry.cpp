#include "../../net/DecodiumSerialPortRegistry.h"

#include <QSemaphore>
#include <QTest>
#include <QThread>

#include <atomic>

using decodium::ports::SerialPortLease;
using decodium::ports::SerialPortRegistry;

class TestSerialPortRegistry : public QObject
{
    Q_OBJECT

private slots:
    void aliasesShareOneLease()
    {
        QVERIFY(SerialPortRegistry::normalize(QStringLiteral("COM17"))
                == SerialPortRegistry::normalize(QStringLiteral("\\\\.\\COM17")));
        auto first = SerialPortRegistry::acquire({QStringLiteral("COM17")},
                                                  QStringLiteral("CAT Hamlib"));
        QVERIFY(first.isValid());
        QCOMPARE(SerialPortRegistry::ownerForPort(QStringLiteral("\\\\.\\COM17")),
                 QStringLiteral("CAT Hamlib"));

        QString conflictPort;
        QString conflictOwner;
        auto second = SerialPortRegistry::acquire({QStringLiteral("\\\\.\\COM17")},
                                                   QStringLiteral("CW keyer"),
                                                   &conflictPort, &conflictOwner);
        QVERIFY(!second.isValid());
        QCOMPARE(conflictOwner, QStringLiteral("CAT Hamlib"));

        first.release();
        QVERIFY(SerialPortRegistry::ownerForPort(QStringLiteral("COM17")).isEmpty());
        auto third = SerialPortRegistry::acquire({QStringLiteral("COM17")},
                                                  QStringLiteral("CW keyer"));
        QVERIFY(third.isValid());
    }

    void multiplePortsAreAllOrNothing()
    {
        auto first = SerialPortRegistry::acquire({QStringLiteral("COM18")},
                                                  QStringLiteral("PTT"));
        QVERIFY(first.isValid());
        QString conflictPort;
        QString conflictOwner;
        auto second = SerialPortRegistry::acquire({QStringLiteral("COM19"), QStringLiteral("COM18")},
                                                   QStringLiteral("CAT"),
                                                   &conflictPort, &conflictOwner);
        QVERIFY(!second.isValid());
        QVERIFY(!conflictPort.isEmpty());
        QCOMPARE(conflictOwner, QStringLiteral("PTT"));
        QVERIFY(SerialPortRegistry::ownerForPort(QStringLiteral("COM19")).isEmpty());
    }

    void concurrentClaimsHaveExactlyOneWinner()
    {
        QSemaphore ready;
        QSemaphore go;
        QSemaphore attempted;
        QSemaphore releaseWinner;
        std::atomic<int> acquired {0};
        QString ownerA;
        QString ownerB;

        QThread* a = QThread::create([&] {
            ready.release();
            go.acquire();
            auto lease = SerialPortRegistry::acquire({QStringLiteral("COM20")}, QStringLiteral("CAT"));
            attempted.release();
            if (lease.isValid()) {
                ownerA = lease.owner();
                ++acquired;
                releaseWinner.acquire();
            }
        });
        QThread* b = QThread::create([&] {
            ready.release();
            go.acquire();
            auto lease = SerialPortRegistry::acquire({QStringLiteral("COM20")}, QStringLiteral("CW"));
            attempted.release();
            if (lease.isValid()) {
                ownerB = lease.owner();
                ++acquired;
                releaseWinner.acquire();
            }
        });
        a->start();
        b->start();
        QVERIFY(ready.tryAcquire(2, 1000));
        go.release(2);
        QVERIFY(attempted.tryAcquire(2, 1000));
        releaseWinner.release();
        QVERIFY(a->wait(2000));
        QVERIFY(b->wait(2000));
        delete a;
        delete b;

        QCOMPARE(acquired.load(), 1);
        QVERIFY(ownerA.isEmpty() != ownerB.isEmpty());
        QVERIFY(SerialPortRegistry::ownerForPort(QStringLiteral("COM20")).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestSerialPortRegistry)
#include "test_serial_port_registry.moc"
