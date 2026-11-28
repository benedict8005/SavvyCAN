#ifndef TST_LOGREADER_H
#define TST_LOGREADER_H

#include <QtTest>
#include "ingest/logreader.h"

class TestLogReader : public QObject
{
    Q_OBJECT

    static QString data(const QString &relative)
    {
        return QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()
            + "/data/synthetic/" + relative;
    }

private slots:
    void messageNamesTheFormat()
    {
        QCOMPARE(LogReader::readFile(data("classic/sample.asc")).formatMessage(),
                 QString("Found format: Vector ASC"));
        QCOMPARE(LogReader::readFile(data("classic/sample.blf")).formatMessage(),
                 QString("Found format: Vector BLF"));
        QCOMPARE(LogReader::readFile(data("classic/sample_channel1.trc")).formatMessage(),
                 QString("Found format: ") + LogFormat::name(LogFormat::Trc));
    }

    void classicFramesPerChannelMatchTruth_data()
    {
        QTest::addColumn<QString>("file");
        QTest::newRow("asc") << "classic/sample.asc";
        QTest::newRow("blf") << "classic/sample.blf";
        QTest::newRow("uncompressed blf") << "classic/sample_uncompressed.blf";
    }
    void classicFramesPerChannelMatchTruth()
    {
        QFETCH(QString, file);
        const LogReader::Result r = LogReader::readFile(data(file));
        QVERIFY(r.opened && r.recognised);
        QVERIFY(r.problems.isEmpty());
        QMap<int, int> perBus;
        for (const CANFrame &f : r.frames) perBus[f.bus]++;
        // truth.json: frames_per_channel 1:1600, 2:1100, 3:500
        QCOMPARE(r.frames.size(), 3200);
        QCOMPARE(perBus.value(1), 1600);
        QCOMPARE(perBus.value(2), 1100);
        QCOMPARE(perBus.value(3), 500);
    }

    void contentDecidesNotExtension()
    {
        const LogReader::Result r = LogReader::readFile(data("malformed/wrong_extension.dat"));
        QCOMPARE(r.format, LogFormat::Candump);
        QCOMPARE(r.frames.size(), 100);
        QVERIFY(r.problems.isEmpty());
    }

    void badLinesComeBackWithLineNumbers()
    {
        const LogReader::Result r = LogReader::readFile(data("malformed/bad_lines.asc"));
        QCOMPARE(r.frames.size(), 200);
        QList<int> lines;
        for (const auto &p : r.problems) lines << p.line;
        QCOMPARE(lines, QList<int>({15, 46, 77, 108, 139}));

        const LogReader::Result g = LogReader::readFile(data("malformed/garbage.log"));
        QCOMPARE(g.frames.size(), 60);
        lines.clear();
        for (const auto &p : g.problems) lines << p.line;
        QCOMPARE(lines, QList<int>({11, 26, 41, 56}));
    }

    void truncatedBlfKeepsGoodFramesAndReportsDamage()
    {
        const LogReader::Result r = LogReader::readFile(data("malformed/truncated.blf"));
        QCOMPARE(r.format, LogFormat::Blf);
        QCOMPARE(r.problems.size(), 1);
        QCOMPARE(r.problems[0].line, 0);
        QVERIFY(r.frames.size() < 200); // truth.json: fewer than 200
    }

    void problemSummaryListsLinesAndReasons()
    {
        const LogReader::Result r = LogReader::readFile(data("malformed/bad_lines.asc"));
        // reasons come from the malformed/truth.json entry for this file
        QCOMPARE(r.problemSummary(),
                 QString("5 problems: line 15: non-hex data; line 46: no structure; "
                         "line 77: classic frame with DLC 9; line 108: missing fields; "
                         "line 139: fewer data bytes than the length"));
        const QString shortened = r.problemSummary(2);
        QVERIFY2(shortened.endsWith("line 46: no structure; and 3 more"), qPrintable(shortened));
        QVERIFY(!shortened.contains("line 77"));
    }

    void problemSummaryForDamagedBlfAndCleanFile()
    {
        const LogReader::Result b = LogReader::readFile(data("malformed/truncated.blf"));
        QVERIFY(b.problemSummary().startsWith("1 problem: "));
        QVERIFY(!b.problemSummary().contains("line"));
        QVERIFY(LogReader::readFile(data("classic/sample.asc")).problemSummary().isEmpty());
    }

    void emptyAndMissingFiles()
    {
        const LogReader::Result e = LogReader::readFile(data("malformed/empty.log"));
        QVERIFY(e.opened);
        QVERIFY(!e.recognised);
        QCOMPARE(e.formatMessage(), QString("Unknown log format"));
        QCOMPARE(e.frames.size(), 0);

        const LogReader::Result m = LogReader::readFile(data("malformed/no_such_file.log"));
        QVERIFY(!m.opened);
        QVERIFY(!m.recognised);
    }
};

#endif // TST_LOGREADER_H
