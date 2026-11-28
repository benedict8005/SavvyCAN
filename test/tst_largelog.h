#ifndef TST_LARGELOG_H
#define TST_LARGELOG_H

#include <QtTest>
#include "ingest/logreader.h"

// Issue #2: a two-million-frame log has to load in a few seconds.
class TestLargeLog : public QObject
{
    Q_OBJECT

    // Writes `count` candump lines with ids and bytes worked out from the line number,
    // so the expected first, last and checksum values are known without reading the file.
    static void writeCandump(const QString &path, int count)
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QByteArray chunk;
        for (int i = 0; i < count; i++)
        {
            const int id = 0x100 + (i % 16);
            const int bus = i % 3;
            chunk += QByteArray("(") + QByteArray::number(i / 1000) + '.'
                + QByteArray::number(i % 1000).rightJustified(3, '0') + "000) can"
                + QByteArray::number(bus) + ' ' + QByteArray::number(id, 16).toUpper()
                + '#' + QByteArray::number(i & 0xFF, 16).rightJustified(2, '0').toUpper() + "AA55\n";
            if (chunk.size() > (1 << 20)) { f.write(chunk); chunk.clear(); }
        }
        f.write(chunk);
    }

    static void writeAsc(const QString &path, int count)
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("date Fri Nov 06 09:50:07.648 2026\nbase hex  timestamps absolute\n"
                "internal events logged\nBegin Triggerblock Thu Jan 01 00:00:00.0 1970\n"
                " 0.000000 Start of measurement\n");
        QByteArray chunk;
        for (int i = 0; i < count; i++)
        {
            chunk += QByteArray(" ") + QByteArray::number(i / 1000) + '.'
                + QByteArray::number(i % 1000).rightJustified(3, '0') + "000 "
                + QByteArray::number(i % 3 + 1) + "  "
                + QByteArray::number(0x100 + (i % 16), 16).toUpper()
                + "             Rx   d 4 " + QByteArray::number(i & 0xFF, 16).rightJustified(2, '0').toUpper()
                + " AA 55 00\n";
            if (chunk.size() > (1 << 20)) { f.write(chunk); chunk.clear(); }
        }
        f.write(chunk);
    }

    static void writeTrc(const QString &path, int count)
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(";$FILEVERSION=2.1\n;$STARTTIME=25569.0\n;$COLUMNS=N,O,T,B,I,d,R,L,D\n;\n");
        QByteArray chunk;
        for (int i = 0; i < count; i++)
        {
            chunk += QByteArray::number(i).rightJustified(7) + QByteArray("  ")
                + QByteArray::number(i / 1000.0, 'f', 3).rightJustified(10) + " DT  "
                + QByteArray::number(i % 3 + 1) + "     0"
                + QByteArray::number(0x100 + (i % 16), 16).toUpper()
                + " Rx -  4    " + QByteArray::number(i & 0xFF, 16).rightJustified(2, '0').toUpper()
                + " AA 55 00\n";
            if (chunk.size() > (1 << 20)) { f.write(chunk); chunk.clear(); }
        }
        f.write(chunk);
    }

    static void writeCsv(const QString &path, int count)
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("time,id,extended,bus,data\n");
        QByteArray chunk;
        for (int i = 0; i < count; i++)
        {
            chunk += QByteArray::number(i / 1000.0, 'f', 3) + ','
                + QByteArray::number(0x100 + (i % 16), 16).toUpper() + ",0,"
                + QByteArray::number(i % 3) + ','
                + QByteArray::number(i & 0xFF, 16).rightJustified(2, '0').toUpper() + "AA55\n";
            if (chunk.size() > (1 << 20)) { f.write(chunk); chunk.clear(); }
        }
        f.write(chunk);
    }

    static void writeGvret(const QString &path, int count)
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("Time Stamp,ID,Extended,Dir,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8\n");
        QByteArray chunk;
        for (int i = 0; i < count; i++)
        {
            chunk += QByteArray::number(qint64(i) * 1000) + ','
                + QByteArray::number(0x100 + (i % 16), 16).toUpper().rightJustified(8, '0') + ",false,Rx,"
                + QByteArray::number(i % 3) + ",4,"
                + QByteArray::number(i & 0xFF, 16).rightJustified(2, '0').toUpper() + ",AA,55,00,\n";
            if (chunk.size() > (1 << 20)) { f.write(chunk); chunk.clear(); }
        }
        f.write(chunk);
    }

    static void checkBig(const LogReader::Result &r, LogFormat::Format format, int count, qint64 ms, int tailLen)
    {
        qInfo() << LogFormat::name(format) << count << "frames read in" << ms << "ms";
        QCOMPARE(r.format, format);
        QVERIFY(r.problems.isEmpty());
        QCOMPARE(r.frames.size(), count);
        QCOMPARE(int(r.frames.first().frameId()), 0x100);
        QCOMPARE(int(r.frames.last().frameId()), 0x100 + ((count - 1) % 16));
        QCOMPARE(r.frames.last().payload().size(), tailLen);
        QVERIFY2(ms < 5000, qPrintable(QString("took %1 ms").arg(ms)));
    }

private slots:
    void twoMillionAscFramesLoadInAFewSeconds()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("big.asc");
        writeAsc(path, 2000000);
        QElapsedTimer timer; timer.start();
        const LogReader::Result r = LogReader::readFile(path);
        checkBig(r, LogFormat::Asc, 2000000, timer.elapsed(), 4);
        QCOMPARE(r.frames.last().bus, (2000000 - 1) % 3 + 1);
    }

    void twoMillionTrcFramesLoadInAFewSeconds()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("big.trc");
        writeTrc(path, 2000000);
        QElapsedTimer timer; timer.start();
        const LogReader::Result r = LogReader::readFile(path);
        checkBig(r, LogFormat::Trc, 2000000, timer.elapsed(), 4);
    }

    void twoMillionCandumpFramesLoadInAFewSeconds()
    {
        const int count = 2000000;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("big.log");
        writeCandump(path, count);

        QElapsedTimer timer;
        timer.start();
        const LogReader::Result r = LogReader::readFile(path);
        const qint64 ms = timer.elapsed();
        qInfo() << "two million candump frames read in" << ms << "ms";

        QCOMPARE(r.format, LogFormat::Candump);
        QVERIFY(r.problems.isEmpty());
        QCOMPARE(r.frames.size(), count);
        QCOMPARE(int(r.frames.first().frameId()), 0x100);
        QCOMPARE(r.frames.first().bus, 0);
        QCOMPARE(int(r.frames.last().frameId()), 0x100 + ((count - 1) % 16));
        QCOMPARE(r.frames.last().bus, (count - 1) % 3);
        QCOMPARE(r.frames.last().payload().toHex().toUpper(), QByteArray("7FAA55"));
        QVERIFY2(ms < 5000, qPrintable(QString("took %1 ms").arg(ms)));
    }

    void twoMillionCsvFramesLoadInAFewSeconds()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("big.csv");
        writeCsv(path, 2000000);
        QElapsedTimer timer; timer.start();
        const LogReader::Result r = LogReader::readFile(path);
        checkBig(r, LogFormat::GenericCsv, 2000000, timer.elapsed(), 3);
        QCOMPARE(r.frames.last().bus, (2000000 - 1) % 3);
    }

    void twoMillionGvretFramesLoadInAFewSeconds()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("big_gvret.csv");
        writeGvret(path, 2000000);
        QElapsedTimer timer; timer.start();
        const LogReader::Result r = LogReader::readFile(path);
        checkBig(r, LogFormat::GvretCsv, 2000000, timer.elapsed(), 4);
        QCOMPARE(r.frames.last().bus, (2000000 - 1) % 3);
    }
};

#endif // TST_LARGELOG_H
