#ifndef TRCPARSER_H
#define TRCPARSER_H

#include <QByteArray>
#include <QList>
#include <QString>
#include <QVector>
#include "can_structs.h"

// Reads PEAK TRC logs of file version 2.1 and says which lines it could not read.
//
//   ;$FILEVERSION=2.1
//   ;$COLUMNS=N,O,T,B,I,d,R,L,D
//         0         0.000 DT  1     0100 Rx -  8    40 1F 32 7D 00 00 00 00
//
// The header must name the version and the columns. The columns decide where each value is
// read from, so files with a different column order still work. Lines that start with ';'
// are comments. Row types DT (data), RR (remote), FD and FB (CAN FD, FB with bit rate
// switch) become frames; error and status rows (ER, EC, ST) are skipped. An id of more than
// four hex digits is an extended id. The time column is in milliseconds. Every other line
// that is not a frame is reported with its 1-based line number. A file with an unknown
// version, an unsupported version or no column line is reported once, at the line that
// shows the problem, and no frames are read from it.
namespace TrcParser
{
    struct LineError
    {
        int line;        // 1-based line number in the file
        QString message; // short reason, for example "bad hex ID"
    };

    struct Result
    {
        QVector<CANFrame> frames;
        QList<LineError> errors;
        bool opened = false; // false when the file could not be opened
    };

    Result parseData(const QByteArray &content);
    Result parseFile(const QString &path);
}

#endif // TRCPARSER_H
