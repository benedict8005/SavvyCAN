#ifndef ASCPARSER_H
#define ASCPARSER_H

#include <QByteArray>
#include <QList>
#include <QString>
#include <QVector>
#include "can_structs.h"

// Reads Vector ASC logs and says which lines it could not read.
//
//   0.003000 2  100   Rx   d 8 C0 2B 32 7E 00 00 00 00        classic data frame
//   0.010000 1  18DA0000x Rx d 8 ...                          "x" after the id: extended id
//   0.020000 1  100   Rx   r                                  remote frame
//   0.005000 CANFD 1 Rx 301  0 0 9 12 5A ...                  CAN FD frame (BRS, ESI, DLC code, length, data)
//
// Header lines (date, base, internal events, comments, trigger blocks) and event lines such
// as "Start of measurement" are skipped. Blank lines are skipped. Every other line that is
// not a frame is reported with its 1-based line number instead of being dropped silently.
// The channel number in the file is kept as the bus number.
namespace AscParser
{
    struct LineError
    {
        int line;        // 1-based line number in the file
        QString message; // short reason, for example "non-hex data"
    };

    struct Result
    {
        QVector<CANFrame> frames;
        QList<LineError> errors;
        bool opened = false; // false when the file could not be opened
    };

    enum LineKind { Frame, Skipped, Bad };

    // Parses one line. Returns Frame (and fills *frame), Skipped for header/event lines,
    // or Bad (and sets *error).
    LineKind parseLine(const QByteArray &line, CANFrame *frame, QString *error);

    Result parseData(const QByteArray &content);
    Result parseFile(const QString &path);
}

#endif // ASCPARSER_H
