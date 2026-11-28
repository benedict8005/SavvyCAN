#ifndef TST_TRCPARSER_H
#define TST_TRCPARSER_H

#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "ingest/trcparser.h"

class TestTrcParser : public QObject
{
    Q_OBJECT

    static QString data(const QString &relative)
    {
        return QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()
            + "/data/synthetic/" + relative;
    }

    static QJsonObject json(const QString &relative)
    {
        QFile file(data(relative));
        if (!file.open(QIODevice::ReadOnly)) return QJsonObject();
        return QJsonDocument::fromJson(file.readAll()).object();
    }

private slots:
    void channelOneCountsMatchTruth()
    {
        // the truth file lists each message's cycle time and channels; the TRC holds channel 1
        const QJsonObject truth = json("classic/truth.json");
        const double duration = truth.value("duration_s").toDouble();
        QMap<quint32, int> expected;
        for (const QJsonValue &value : truth.value("messages").toArray())
        {
            const QJsonObject message = value.toObject();
            if (!message.value("channels").toArray().contains(1)) continue;
            expected[quint32(message.value("id").toInt())] =
                int(duration * 1000.0 / message.value("cycle_ms").toDouble());
        }

        const TrcParser::Result result = TrcParser::parseFile(data("classic/sample_channel1.trc"));
        QVERIFY(result.opened);
        QVERIFY(result.errors.isEmpty());
        QCOMPARE(result.frames.size(), truth.value("frames_per_channel").toObject().value("1").toInt());

        QMap<quint32, int> counts;
        for (const CANFrame &frame : result.frames)
        {
            QCOMPARE(frame.bus, 1);
            QVERIFY(!frame.hasExtendedFrameFormat());
            counts[frame.frameId()]++;
        }
        QCOMPARE(counts, expected);
    }

    void firstAndLastFramesReadBack()
    {
        const TrcParser::Result result = TrcParser::parseFile(data("classic/sample_channel1.trc"));
        QVERIFY(result.frames.size() > 2);
        const CANFrame &first = result.frames.first();
        QCOMPARE(first.frameId(), quint32(0x100));
        QCOMPARE(first.payload().toHex(), QByteArray("401f327d00000000"));
        QCOMPARE(first.timeStamp().microSeconds(), qint64(0));
        const CANFrame &last = result.frames.last();
        QCOMPARE(last.frameId(), quint32(0x100));
        QCOMPARE(last.payload().toHex(), QByteArray("2c1f328100000000"));
        QCOMPARE(qint64(last.timeStamp().seconds()), qint64(9));
        QCOMPARE(last.timeStamp().microSeconds(), qint64(990000));
    }

    void badHeaderIsReportedOnce()
    {
        const TrcParser::Result result = TrcParser::parseFile(data("malformed/bad_header.trc"));
        QVERIFY(result.opened);
        QCOMPARE(result.frames.size(), 0);
        QCOMPARE(result.errors.size(), 1);
        QCOMPARE(result.errors[0].line, 1);
        QCOMPARE(result.errors[0].message, QString("unknown file version"));
    }

    void missingColumnsAreReported()
    {
        const TrcParser::Result result = TrcParser::parseData(
            ";$FILEVERSION=2.1\n"
            "      0         0.000 DT  1     0100 Rx -  1    AA\n");
        QCOMPARE(result.frames.size(), 0);
        QCOMPARE(result.errors.size(), 1);
        QCOMPARE(result.errors[0].line, 2);
        QCOMPARE(result.errors[0].message, QString("no column definition"));
    }

    void badRowsAreNumberedAndOthersStillRead()
    {
        const TrcParser::Result result = TrcParser::parseData(
            ";$FILEVERSION=2.1\n"                                   // 1
            ";$COLUMNS=N,O,T,B,I,d,R,L,D\n"                         // 2
            "      0         1.500 DT  2     18DA0000 Tx -  2    AA BB\n"   // 3 extended
            "      1         2.000 RR  1     0123 Rx -  0\n"                // 4 remote
            "      2         3.000 DT  1     01ZZ Rx -  1    AA\n"          // 5 bad id
            "      3         4.000 DT  1     0100 Rx -  4    AA BB\n"       // 6 short data
            "      4         5.000 DT  1     0100 Rx -  1    GG\n"          // 7 non-hex
            "      5         6.000 ER  1     0000 Rx -  0\n"                // 8 skipped
            "      6         7.000 DT  1\n"                                 // 9 missing fields
            "      7         8.000 DT  1     0100 Rx -  9    AA\n"          // 10 length
            "      8         9.000 DT  1     0101 Rx -  1    CC\n");        // 11 good
        QCOMPARE(result.frames.size(), 3);
        QList<int> lines;
        QStringList messages;
        for (const TrcParser::LineError &e : result.errors) { lines << e.line; messages << e.message; }
        QCOMPARE(lines, (QList<int>{5, 6, 7, 9, 10}));
        QCOMPARE(messages, (QStringList{"bad hex ID", "fewer data bytes than the length", "non-hex data",
                                        "missing fields", "bad length"}));

        QCOMPARE(result.frames[0].bus, 2);
        QCOMPARE(result.frames[0].frameId(), quint32(0x18DA0000));
        QVERIFY(result.frames[0].hasExtendedFrameFormat());
        QVERIFY(!result.frames[0].isReceived);
        QCOMPARE(result.frames[1].frameType(), QCanBusFrame::RemoteRequestFrame);
        QCOMPARE(result.frames[2].payload().toHex(), QByteArray("cc"));
    }

    void canFdRowsKeepFlagsAndLength()
    {
        const TrcParser::Result result = TrcParser::parseData(
            ";$FILEVERSION=2.1\n"
            ";$COLUMNS=N,O,T,B,I,d,R,L,D\n"
            "      0         0.000 FB  1     0300 Rx -  9    00 01 02 03 04 05 06 07 08 09 0A 0B\n");
        QVERIFY(result.errors.isEmpty());
        QCOMPARE(result.frames.size(), 1);
        QVERIFY(result.frames[0].hasFlexibleDataRateFormat());
        QVERIFY(result.frames[0].hasBitrateSwitch());
        QCOMPARE(result.frames[0].payload().size(), 12);
    }

    void missingFileIsNotOpened()
    {
        QVERIFY(!TrcParser::parseFile("/no/such/file").opened);
    }
};

#endif // TST_TRCPARSER_H
