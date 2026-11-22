#ifndef TST_GVRETPARSER_H
#define TST_GVRETPARSER_H

#include <QtTest>
#include "ingest/gvretparser.h"

class TestGvretParser : public QObject
{
    Q_OBJECT

private slots:
    void version2KeepsBusDirectionAndTime()
    {
        const QByteArray csv =
            "Time Stamp,ID,Extended,Dir,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8\n"
            "39747828,000005EB,false,Rx,0,8,E8,45,85,4B,4A,28,36,69,\n"
            "39748000,18FEF100,true,Tx,2,2,01,FF,\n"
            "5,100,false,Rx,1,0,\n";
        const GvretParser::Result r = GvretParser::parseData(csv);
        QCOMPARE(r.version, 2);
        QVERIFY(r.errors.isEmpty());
        QCOMPARE(r.frames.size(), 3);
        QCOMPARE(r.frames[0].bus, 0);
        QCOMPARE(r.frames[0].frameId(), quint32(0x5EB));
        QVERIFY(!r.frames[0].hasExtendedFrameFormat());
        QVERIFY(r.frames[0].isReceived);
        QCOMPARE(r.frames[0].timeStamp().microSeconds(), qint64(39747828));
        QCOMPARE(r.frames[0].payload(), QByteArray::fromHex("e845854b4a283669"));
        QCOMPARE(r.frames[1].bus, 2);
        QVERIFY(r.frames[1].hasExtendedFrameFormat());
        QVERIFY(!r.frames[1].isReceived);
        QCOMPARE(r.frames[1].payload(), QByteArray::fromHex("01ff"));
        QCOMPARE(r.frames[2].bus, 1);
        QCOMPARE(r.frames[2].timeStamp().microSeconds(), qint64(5)); // small values are not scaled
        QCOMPARE(r.frames[2].payload().size(), 0);
    }

    void version1HasNoDirectionColumn()
    {
        const GvretParser::Result r = GvretParser::parseData(
            "Time Stamp,ID,Extended,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8\n"
            "1000,200,false,3,1,AB,\n");
        QCOMPARE(r.version, 1);
        QCOMPARE(r.frames.size(), 1);
        QCOMPARE(r.frames[0].bus, 3);
        QCOMPARE(r.frames[0].payload(), QByteArray::fromHex("ab"));
        QVERIFY(r.frames[0].isReceived);
    }

    void badLinesAreNumbered()
    {
        const QByteArray csv =
            "Time Stamp,ID,Extended,Dir,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8\n"
            "1,100,false,Rx,0,1,00,\n"      // 2 ok
            "2,100,false,Rx\n"               // 3
            "x,100,false,Rx,0,1,00,\n"       // 4
            "3,zz,false,Rx,0,1,00,\n"        // 5
            "4,100,maybe,Rx,0,1,00,\n"       // 6
            "5,100,false,Up,0,1,00,\n"       // 7
            "6,100,false,Rx,-1,1,00,\n"      // 8
            "7,100,false,Rx,0,9,00,\n"       // 9
            "8,100,false,Rx,0,3,00,01\n"     // 10
            "9,100,false,Rx,0,1,GG,\n"       // 11
            "\n"
            "10,100,false,Rx,0,1,00,\n";     // 13 ok
        const GvretParser::Result r = GvretParser::parseData(csv);
        QCOMPARE(r.frames.size(), 2);
        QStringList got;
        for (const auto &e : r.errors) got << QString("%1:%2").arg(e.line).arg(e.message);
        QCOMPARE(got, QStringList({"3:missing fields", "4:bad timestamp", "5:bad ID",
                                   "6:bad extended flag", "7:bad direction", "8:bad bus",
                                   "9:bad length", "10:fewer data bytes than length", "11:non-hex data"}));
    }

    void otherHeaderIsReportedOnce()
    {
        const GvretParser::Result r = GvretParser::parseData("timestamp,arbitration_id,data\n0.1,100,00\n");
        QCOMPARE(r.version, 0);
        QCOMPARE(r.frames.size(), 0);
        QCOMPARE(r.errors.size(), 1);
        QCOMPARE(r.errors[0].line, 1);
    }

    void missingFileIsNotOpened()
    {
        QVERIFY(!GvretParser::parseFile("/nonexistent/file.csv").opened);
    }
};

#endif // TST_GVRETPARSER_H
