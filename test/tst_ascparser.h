#ifndef TST_ASCPARSER_H
#define TST_ASCPARSER_H

#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "ingest/ascparser.h"

class TestAscParser : public QObject
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
    void badLinesAreNumbered()
    {
        const QJsonObject truth = json("malformed/truth.json").value("bad_lines.asc").toObject();
        const AscParser::Result result = AscParser::parseFile(data("malformed/bad_lines.asc"));
        QVERIFY(result.opened);
        QCOMPARE(result.frames.size(), truth.value("valid_frames").toInt());

        const QJsonArray lines = truth.value("bad_line_numbers").toArray();
        const QJsonArray problems = truth.value("problems").toArray();
        QCOMPARE(result.errors.size(), lines.size());
        for (int i = 0; i < lines.size(); i++)
        {
            QCOMPARE(result.errors[i].line, lines[i].toInt());
            QVERIFY2(result.errors[i].message == problems[i].toString(),
                     qPrintable(result.errors[i].message + " vs " + problems[i].toString()));
        }
    }

    void headerOnlyHasNoFramesAndNoErrors()
    {
        const AscParser::Result result = AscParser::parseFile(data("malformed/header_only.asc"));
        QVERIFY(result.opened);
        QCOMPARE(result.frames.size(), 0);
        QVERIFY(result.errors.isEmpty());
    }

    void classicFramesReadBackPerChannel()
    {
        const QJsonObject perChannel = json("classic/truth.json").value("frames_per_channel").toObject();
        const AscParser::Result result = AscParser::parseFile(data("classic/sample.asc"));
        QVERIFY(result.errors.isEmpty());
        QMap<int, int> counts;
        for (const CANFrame &frame : result.frames) counts[frame.bus]++;
        int expected = 0;
        for (const QString &key : perChannel.keys())
        {
            QCOMPARE(counts.value(key.toInt()), perChannel.value(key).toInt());
            expected += perChannel.value(key).toInt();
        }
        QCOMPARE(result.frames.size(), expected);
    }

    void canFdFlagsReadBack()
    {
        const QJsonObject truth = json("canfd/truth.json");
        const AscParser::Result result = AscParser::parseFile(data("canfd/canfd.asc"));
        QVERIFY(result.errors.isEmpty());
        QCOMPARE(result.frames.size(), truth.value("frames").toInt());

        int fd = 0, brs = 0, esi = 0, extended = 0;
        for (const CANFrame &frame : result.frames)
        {
            if (frame.hasExtendedFrameFormat()) extended++;
            if (!frame.hasFlexibleDataRateFormat()) continue;
            fd++;
            if (frame.hasBitrateSwitch()) brs++;
            if (frame.hasErrorStateIndicator()) esi++;
        }
        QCOMPARE(fd, truth.value("fd_frames").toInt());
        QCOMPARE(brs, truth.value("brs_frames").toInt());
        QCOMPARE(esi, truth.value("esi_frames").toInt());
        QCOMPARE(extended, truth.value("extended_id_frames").toInt());

        const QJsonArray list = truth.value("frame_list").toArray();
        for (int i = 0; i < list.size(); i++)
        {
            const QJsonArray row = list[i].toArray();
            const CANFrame &frame = result.frames[i];
            QCOMPARE(qint64(frame.timeStamp().seconds()) * 1000000 + frame.timeStamp().microSeconds(),
                     qRound64(row[0].toDouble() * 1e6));
            QCOMPARE(frame.frameId(), quint32(row[2].toDouble()));
            QCOMPARE(frame.hasExtendedFrameFormat(), row[3].toBool());
            QCOMPARE(frame.payload().toHex(), row[6].toString().toLatin1());
        }
    }

    void remoteExtendedAndDirection()
    {
        const AscParser::Result result = AscParser::parseData(
            "date Mon Jan 01 00:00:00.000 am 2026\n"
            "   1.500000 3 18DA0000x Tx r\n\n"
            "   2.000000 1 123 Rx d 2 AA BB\n");
        QVERIFY(result.errors.isEmpty());
        QCOMPARE(result.frames.size(), 2);
        QCOMPARE(result.frames[0].bus, 3);
        QCOMPARE(result.frames[0].frameId(), quint32(0x18DA0000));
        QVERIFY(result.frames[0].hasExtendedFrameFormat());
        QVERIFY(!result.frames[0].isReceived);
        QCOMPARE(result.frames[0].frameType(), QCanBusFrame::RemoteRequestFrame);
        QCOMPARE(result.frames[1].payload().toHex(), QByteArray("aabb"));
    }

    void missingFileIsNotOpened()
    {
        QVERIFY(!AscParser::parseFile("/no/such/file").opened);
    }
};

#endif // TST_ASCPARSER_H
