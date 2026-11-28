#ifndef TST_CSVPARSER_H
#define TST_CSVPARSER_H

#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "ingest/csvparser.h"

class TestCsvParser : public QObject
{
    Q_OBJECT

    static QString data(const QString &relative)
    {
        return QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()
            + "/data/synthetic/" + relative;
    }

private slots:
    void shortRowsAreNumbered()
    {
        QFile file(data("malformed/truth.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonObject truth = QJsonDocument::fromJson(file.readAll()).object()
                                      .value("missing_columns.csv").toObject();
        const CsvParser::Result result = CsvParser::parseFile(data("malformed/missing_columns.csv"));
        QVERIFY(result.opened);
        QCOMPARE(result.frames.size(), truth.value("valid_frames").toInt());
        const QJsonArray lines = truth.value("bad_line_numbers").toArray();
        QCOMPARE(result.errors.size(), lines.size());
        for (int i = 0; i < lines.size(); i++)
        {
            QCOMPARE(result.errors[i].line, lines[i].toInt());
            QCOMPARE(result.errors[i].message, QString("missing fields"));
        }
    }

    void firstFramesAreReadWithoutLengthOrChannel()
    {
        const CsvParser::Result result = CsvParser::parseFile(data("malformed/missing_columns.csv"));
        QVERIFY(result.frames.size() >= 2);
        QCOMPARE(result.frames[0].frameId(), quint32(0x100));
        QCOMPARE(result.frames[0].payload(), QByteArray::fromHex("401f327d00000000"));
        QCOMPARE(result.frames[0].bus, 0);
        QCOMPARE(result.frames[3].timeStamp().microSeconds(), qint64(3000));
    }

    void badRowsGetTheirOwnReason()
    {
        const QByteArray csv =
            "Time,ID,Data,Extended,Bus\n"
            "0.1,0x1ABCDE00,0102,1,2\n"   // 2
            "x,100,00\n"                   // 3: not enough columns, checked first
            "x,100,00,0,0\n"               // 4
            "0.2,xyz,00,0,0\n"             // 5
            "0.3,100,0g,0,0\n"             // 6
            "0.4,100,012,0,0\n"            // 7
            "0.5,100,00,0,-1\n"            // 8
            "\n"
            "0.6,7FF,\n";                  // 10: empty data is fine only with enough columns
        const CsvParser::Result result = CsvParser::parseData(csv);
        QCOMPARE(result.frames.size(), 1);
        QCOMPARE(result.frames[0].bus, 2);
        QVERIFY(result.frames[0].hasExtendedFrameFormat());
        QCOMPARE(result.frames[0].frameId(), quint32(0x1ABCDE00));
        QStringList got;
        for (const auto &e : result.errors) got << QString("%1:%2").arg(e.line).arg(e.message);
        QCOMPARE(got, QStringList({"3:missing fields", "4:bad timestamp", "5:bad ID", "6:non-hex data",
                                   "7:odd number of hex digits", "8:bad bus", "10:missing fields"}));
    }

    void headerWithoutDataColumnIsReportedOnce()
    {
        const CsvParser::Result result = CsvParser::parseData("timestamp,arbitration_id,dlc\n0.1,100,8\n0.2,100,8\n");
        QCOMPARE(result.frames.size(), 0);
        QCOMPARE(result.errors.size(), 1);
        QCOMPARE(result.errors[0].line, 1);
    }
};

#endif // TST_CSVPARSER_H
