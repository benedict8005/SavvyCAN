#ifndef TST_LOGFORMAT_H
#define TST_LOGFORMAT_H

#include <QtTest>
#include "ingest/logformat.h"

class TestLogFormat : public QObject
{
    Q_OBJECT

    static QString data(const QString &relative)
    {
        return QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()
            + "/data/synthetic/" + relative;
    }

private slots:
    void synthetic_data()
    {
        QTest::addColumn<QString>("file");
        QTest::addColumn<int>("format");
        using namespace LogFormat;
        QTest::newRow("classic asc") << "classic/sample.asc" << int(Asc);
        QTest::newRow("classic blf") << "classic/sample.blf" << int(Blf);
        QTest::newRow("uncompressed blf") << "classic/sample_uncompressed.blf" << int(Blf);
        QTest::newRow("csv") << "classic/sample.csv" << int(GenericCsv);
        QTest::newRow("candump") << "classic/sample_candump.log" << int(Candump);
        QTest::newRow("trc") << "classic/sample_channel1.trc" << int(Trc);
        QTest::newRow("fd asc") << "canfd/canfd.asc" << int(Asc);
        QTest::newRow("fd blf") << "canfd/canfd.blf" << int(Blf);
        QTest::newRow("fd candump") << "canfd/canfd_candump.log" << int(Candump);
        QTest::newRow("jitter asc") << "jitter/jitter.asc" << int(Asc);
        QTest::newRow("jitter candump") << "jitter/jitter_candump.log" << int(Candump);
        // the content decides, not the extension
        QTest::newRow("candump named .dat") << "malformed/wrong_extension.dat" << int(Candump);
        // damaged files are still recognised so the reader can report the damage
        QTest::newRow("truncated blf") << "malformed/truncated.blf" << int(Blf);
        QTest::newRow("bad lines asc") << "malformed/bad_lines.asc" << int(Asc);
        QTest::newRow("header only asc") << "malformed/header_only.asc" << int(Asc);
        QTest::newRow("garbage candump") << "malformed/garbage.log" << int(Candump);
        QTest::newRow("short csv") << "malformed/missing_columns.csv" << int(GenericCsv);
        QTest::newRow("bad version trc") << "malformed/bad_header.trc" << int(Trc);
        QTest::newRow("empty") << "malformed/empty.log" << int(Unknown);
    }

    void synthetic()
    {
        QFETCH(QString, file);
        QFETCH(int, format);
        QCOMPARE(int(LogFormat::detectFile(data(file))), format);
    }

    void gvretCsv()
    {
        QCOMPARE(LogFormat::detect("Time Stamp,ID,Extended,Dir,Bus,LEN,D1,D2\n1000,100,false,Rx,0,1,5\n"),
                 LogFormat::GvretCsv);
    }

    void unknownContent()
    {
        QCOMPARE(LogFormat::detect("hello world\nthis is not a log\n"), LogFormat::Unknown);
        QCOMPARE(LogFormat::detect(QByteArray()), LogFormat::Unknown);
        QCOMPARE(LogFormat::detectFile("/no/such/file"), LogFormat::Unknown);
    }

    void names()
    {
        QCOMPARE(LogFormat::name(LogFormat::Asc), QString("Vector ASC"));
        QCOMPARE(LogFormat::name(LogFormat::Unknown), QString("unknown"));
    }
};

#endif // TST_LOGFORMAT_H
