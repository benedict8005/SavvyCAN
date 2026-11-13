#ifndef TST_VIRTUALBUS_H
#define TST_VIRTUALBUS_H

#include <QtTest>
#include <QCanBus>
#include <QCanBusDevice>
#include <memory>
#include "can_structs.h"

// Checks frames travel between two devices on Qt's virtual CAN bus. This needs
// no hardware or SocketCAN interface; the tests are skipped when the plugin is
// not installed.
class TestVirtualBus : public QObject
{
    Q_OBJECT

    std::unique_ptr<QCanBusDevice> open(const QString &channel)
    {
        QString error;
        std::unique_ptr<QCanBusDevice> dev(
            QCanBus::instance()->createDevice("virtualcan", channel, &error));
        if (!dev) return nullptr;
        if (!dev->connectDevice()) return nullptr;
        // Writing before ConnectedState is reached loses frames.
        for (int i = 0; i < 50 && dev->state() != QCanBusDevice::ConnectedState; i++)
            QTest::qWait(20);
        if (dev->state() != QCanBusDevice::ConnectedState) return nullptr;
        return dev;
    }

private slots:
    void initTestCase()
    {
        if (!QCanBus::instance()->plugins().contains("virtualcan"))
            QSKIP("Qt virtualcan plugin is not installed");
    }

    void frameCrossesBus_data()
    {
        QTest::addColumn<quint32>("id");
        QTest::addColumn<bool>("extended");
        QTest::addColumn<QByteArray>("payload");
        QTest::newRow("standard") << 0x123u << false << QByteArray("\x01\x02\x03", 3);
        QTest::newRow("extended") << 0x18FEF100u << true << QByteArray("\xAA\xBB\xCC\xDD\xEE\xFF\x00\x11", 8);
        QTest::newRow("empty") << 0x7FFu << false << QByteArray();
    }

    void frameCrossesBus()
    {
        QFETCH(quint32, id);
        QFETCH(bool, extended);
        QFETCH(QByteArray, payload);

        auto tx = open("can0");
        auto rx = open("can0");
        QVERIFY(tx);
        QVERIFY(rx);

        CANFrame out;
        out.setFrameId(id);
        out.setExtendedFrameFormat(extended);
        out.setPayload(payload);
        QVERIFY(tx->writeFrame(out));

        QTRY_VERIFY_WITH_TIMEOUT(rx->framesAvailable() > 0, 2000);
        QCanBusFrame in = rx->readFrame();
        QCOMPARE(in.frameId(), id);
        QCOMPARE(in.hasExtendedFrameFormat(), extended);
        QCOMPARE(in.payload(), payload);
    }
};

#endif // TST_VIRTUALBUS_H
