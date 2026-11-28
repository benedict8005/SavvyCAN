#ifndef TST_LEGACYBUS_H
#define TST_LEGACYBUS_H

#include <QtTest>
#include "framefileio.h"
#include "ingest/ascparser.h"
#include "ingest/candumpparser.h"
#include "ingest/trcparser.h"
#include "ingest/gvretparser.h"

// The new parsers must give every frame the same bus number as the readers that
// SavvyCAN already uses, so that databases, filters and saved projects that refer to
// a bus keep working. Each test loads the same file both ways and compares frame by frame.
class TestLegacyBus : public QObject
{
    Q_OBJECT

    static QString data(const QString &name)
    {
        return QFileInfo(QString::fromUtf8(__FILE__)).absolutePath() + "/data/synthetic/" + name;
    }

    static void compareBuses(const QVector<CANFrame> &legacy, const QVector<CANFrame> &parsed)
    {
        QVERIFY(!legacy.isEmpty());
        QCOMPARE(parsed.size(), legacy.size());
        for (int i = 0; i < legacy.size(); i++)
        {
            if (parsed[i].bus != legacy[i].bus || parsed[i].frameId() != legacy[i].frameId())
                QFAIL(qPrintable(QString("frame %1: bus %2 id %3, legacy bus %4 id %5")
                    .arg(i).arg(parsed[i].bus).arg(parsed[i].frameId())
                    .arg(legacy[i].bus).arg(legacy[i].frameId())));
        }
    }

private slots:
    void ascKeepsChannelNumberAsBus()
    {
        QVector<CANFrame> legacy;
        QVERIFY(FrameFileIO::loadCanalyzerASC(data("classic/sample.asc"), &legacy));
        const AscParser::Result parsed = AscParser::parseFile(data("classic/sample.asc"));
        QVERIFY(parsed.errors.isEmpty());
        compareBuses(legacy, parsed.frames);
        // channels 1 to 3 in the file are buses 1 to 3, not 0 to 2
        QCOMPARE(parsed.frames[0].bus, 1);
    }

    void candumpKeepsInterfaceNumberAsBus()
    {
        QVector<CANFrame> legacy;
        QVERIFY(FrameFileIO::loadCanDumpFile(data("classic/sample_candump.log"), &legacy));
        const CandumpParser::Result parsed = CandumpParser::parseFile(data("classic/sample_candump.log"));
        QVERIFY(parsed.errors.isEmpty());
        compareBuses(legacy, parsed.frames);
        // can0, can1, can2 are buses 0, 1, 2
        QCOMPARE(parsed.frames[0].bus, 0);
    }

    void trcKeepsBusColumn()
    {
        QVector<CANFrame> legacy;
        QVERIFY(FrameFileIO::loadPCANFile(data("classic/sample_channel1.trc"), &legacy));
        const TrcParser::Result parsed = TrcParser::parseFile(data("classic/sample_channel1.trc"));
        QVERIFY(parsed.errors.isEmpty());
        compareBuses(legacy, parsed.frames);
        QCOMPARE(parsed.frames[0].bus, 1);
    }

    void gvretCsvKeepsBusColumn()
    {
        const QByteArray csv =
            "Time Stamp,ID,Extended,Dir,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8\n"
            "1000,00000100,false,Rx,0,2,01,02,\n"
            "2000,18FEF100,true,Rx,1,1,AA,\n"
            "3000,00000200,false,Tx,2,1,BB,\n"
            "4000,00000100,false,Rx,5,0,\n";
        QTemporaryFile file;
        QVERIFY(file.open());
        file.write(csv);
        file.flush();

        QVector<CANFrame> legacy;
        QVERIFY(FrameFileIO::loadNativeCSVFile(file.fileName(), &legacy));
        const GvretParser::Result parsed = GvretParser::parseData(csv);
        QVERIFY(parsed.errors.isEmpty());
        compareBuses(legacy, parsed.frames);
        QCOMPARE(parsed.frames[3].bus, 5);
    }
};

#endif // TST_LEGACYBUS_H
