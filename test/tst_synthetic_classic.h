#ifndef TST_SYNTHETIC_CLASSIC_H
#define TST_SYNTHETIC_CLASSIC_H

#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include "framefileio.h"

// Reads the generated classic logs and compares what the readers return with
// the numbers in classic/truth.json.
class TestSyntheticClassic : public QObject
{
    Q_OBJECT

    static QString dataDir()
    {
        return QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()
            + "/data/synthetic/classic/";
    }

    static QJsonObject truth()
    {
        QFile file(dataDir() + "truth.json");
        if (!file.open(QIODevice::ReadOnly)) return QJsonObject();
        return QJsonDocument::fromJson(file.readAll()).object();
    }

private slots:
    void truthFileIsReadable()
    {
        QVERIFY(truth().contains("frames_per_channel"));
    }

    void ascFrameCountPerChannel()
    {
        QVector<CANFrame> frames;
        QVERIFY(FrameFileIO::loadCanalyzerASC(dataDir() + "sample.asc", &frames));

        const QJsonObject expected = truth().value("frames_per_channel").toObject();
        QMap<int, int> perBus;
        int total = 0;
        for (const CANFrame &frame : frames) {
            perBus[frame.bus]++;
            total++;
        }
        int expectedTotal = 0;
        for (const QString &channel : expected.keys()) {
            // The ASC reader keeps the channel number from the file as the bus.
            int count = expected.value(channel).toInt();
            expectedTotal += count;
            QVERIFY2(perBus.value(channel.toInt()) == count,
                     qPrintable("channel " + channel));
        }
        QCOMPARE(total, expectedTotal);
    }
};

#endif
