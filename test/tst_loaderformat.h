#ifndef TST_LOADERFORMAT_H
#define TST_LOADERFORMAT_H

#include <QtTest>
#include "framefileio.h"
#include "ingest/logformat.h"

// The file loader tells the user which format it found, from the content of the file.
class TestLoaderFormat : public QObject
{
    Q_OBJECT

    static QString data(const QString &relative)
    {
        return QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()
            + "/data/synthetic/" + relative;
    }

private slots:
    void messageNamesTheFormat_data()
    {
        QTest::addColumn<QString>("file");
        QTest::addColumn<QString>("message");
        QTest::newRow("asc") << "classic/sample.asc" << "Found format: Vector ASC";
        QTest::newRow("blf") << "classic/sample.blf" << "Found format: Vector BLF";
    }
    void messageNamesTheFormat()
    {
        QFETCH(QString, file);
        QFETCH(QString, message);
        QCOMPARE(FrameFileIO::detectedFormatMessage(data(file)), message);
    }

    void extensionIsIgnored()
    {
        // candump content stored under a .dat name
        QCOMPARE(FrameFileIO::detectedFormatMessage(data("malformed/wrong_extension.dat")),
                 QString("Found format: ") + LogFormat::name(LogFormat::Candump));
    }

    void unknownOrMissingGivesNoMessage()
    {
        QVERIFY(FrameFileIO::detectedFormatMessage(data("does/not/exist.log")).isEmpty());
        QTemporaryFile junk;
        QVERIFY(junk.open());
        junk.write("this is not a can log\n");
        junk.close();
        QVERIFY(FrameFileIO::detectedFormatMessage(junk.fileName()).isEmpty());
    }
};

#endif // TST_LOADERFORMAT_H
