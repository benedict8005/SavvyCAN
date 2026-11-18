#ifndef TST_CANDUMPPARSER_H
#define TST_CANDUMPPARSER_H

#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "ingest/candumpparser.h"

class TestCandumpParser : public QObject
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
    void garbageLinesAreNumbered()
    {
        const QJsonObject truth = json("malformed/truth.json").value("garbage.log").toObject();
        const CandumpParser::Result result = CandumpParser::parseFile(data("malformed/garbage.log"));
        QVERIFY(result.opened);
        QCOMPARE(result.frames.size(), truth.value("valid_frames").toInt());

        const QJsonArray lines = truth.value("bad_line_numbers").toArray();
        const QJsonArray problems = truth.value("problems").toArray();
        QCOMPARE(result.errors.size(), lines.size());
        for (int i = 0; i < lines.size(); i++)
        {
            QCOMPARE(result.errors[i].line, lines[i].toInt());
            // the reason is the one the generator recorded, for example "missing '#'"
            QVERIFY2(result.errors[i].message == problems[i].toString(),
                     qPrintable(result.errors[i].message + " vs " + problems[i].toString()));
        }
    }

    void classicFramesReadBack()
    {
        const CandumpParser::Result result = CandumpParser::parseFile(data("classic/sample_candump.log"));
        QVERIFY(result.errors.isEmpty());
        const QJsonObject perChannel = json("classic/truth.json").value("frames_per_channel").toObject();
        int expected = 0;
        for (const QString &key : perChannel.keys()) expected += perChannel.value(key).toInt();
        QCOMPARE(result.frames.size(), expected);
    }

    void canFdFlagsReadBack()
    {
        const QJsonObject truth = json("canfd/truth.json");
        const CandumpParser::Result result = CandumpParser::parseFile(data("canfd/canfd_candump.log"));
        QVERIFY(result.errors.isEmpty());
        QCOMPARE(result.frames.size(), truth.value("frames").toInt());

        int fd = 0, brs = 0, esi = 0, extended = 0;
        for (const CANFrame &frame : result.frames)
        {
            if (frame.hasExtendedFrameFormat()) extended++; // counted over every frame, as in truth.json
            if (!frame.hasFlexibleDataRateFormat()) continue;
            fd++;
            if (frame.hasBitrateSwitch()) brs++;
            if (frame.hasErrorStateIndicator()) esi++;
        }
        QCOMPARE(fd, truth.value("fd_frames").toInt());
        QCOMPARE(brs, truth.value("brs_frames").toInt());
        QCOMPARE(esi, truth.value("esi_frames").toInt());
        QCOMPARE(extended, truth.value("extended_id_frames").toInt());

        // first frames against frame_list: [time, bus, id, extended, fd, ?, data]
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

    void remoteAndDirection()
    {
        const CandumpParser::Result result = CandumpParser::parseData(
            "(1.500000) vcan1 123#R\n\n(2.000000) can0 18DA0000#AABB T\n");
        QVERIFY(result.errors.isEmpty());
        QCOMPARE(result.frames.size(), 2);
        QCOMPARE(result.frames[0].bus, 1);
        QCOMPARE(result.frames[0].frameType(), QCanBusFrame::RemoteRequestFrame);
        QVERIFY(!result.frames[1].isReceived);
        QVERIFY(result.frames[1].hasExtendedFrameFormat());
        QCOMPARE(qint64(result.frames[0].timeStamp().seconds()), qint64(1));
        QCOMPARE(qint64(result.frames[0].timeStamp().microSeconds()), qint64(500000));
    }

    void missingFileIsNotOpened()
    {
        QVERIFY(!CandumpParser::parseFile("/no/such/file").opened);
    }
};

#endif // TST_CANDUMPPARSER_H
