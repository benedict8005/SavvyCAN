#ifndef LOGREADER_H
#define LOGREADER_H

#include <QList>
#include <QString>
#include <QVector>
#include "can_structs.h"
#include "logformat.h"

// Reads any supported log file: finds the format from the content, hands the file to the
// matching parser and gives back the frames, the problems found and the message to show
// the user ("Found format: Vector ASC").
//
// Line-based formats report bad lines with their 1-based line number. A BLF file has no
// lines, so damage is reported with line 0 and the frames read before the damage are kept.
namespace LogReader
{
    struct Problem
    {
        int line;        // 1-based line number, 0 when the problem is not tied to a line
        QString message;
    };

    struct Result
    {
        LogFormat::Format format = LogFormat::Unknown;
        QVector<CANFrame> frames;
        QList<Problem> problems;
        bool opened = false;   // false when the file could not be opened
        bool recognised = false; // false when no supported format was found

        // "Found format: Vector ASC", or "Unknown log format" when nothing matched
        QString formatMessage() const;

        // Text for the user about what could not be read, e.g.
        // "3 problems: line 15: non-hex data; line 46: no structure; line 77: missing fields".
        // At most maxShown problems are listed, then "and N more". Empty when there are none.
        QString problemSummary(int maxShown = 5) const;
    };

    Result readFile(const QString &path);
}

#endif // LOGREADER_H
