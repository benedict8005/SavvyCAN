#ifndef CANDUMPPARSER_H
#define CANDUMPPARSER_H

#include <QByteArray>
#include <QList>
#include <QString>
#include <QVector>
#include "can_structs.h"

// Reads SocketCAN "candump -L" logs and says which lines it could not read.
//
//   (0.005000) can0 123#AABB           classic frame
//   (0.005000) can0 123#R              remote frame
//   (0.005000) can0 18DA0000##3AABB    CAN FD frame; first digit holds the BRS (1) and ESI (2) flags
//
// An optional trailing "R" or "T" token (direction) is accepted and ignored.
// A line with 8 id digits is an extended id, a line with 3 digits a standard id.
// Blank lines are skipped. Every other line that is not a frame is reported with its
// 1-based line number instead of being dropped silently.
namespace CandumpParser
{
    struct LineError
    {
        int line;        // 1-based line number in the file
        QString message; // short reason, for example "missing '#'"
    };

    struct Result
    {
        QVector<CANFrame> frames;
        QList<LineError> errors;
        bool opened = false; // false when the file could not be opened
    };

    // Parses one line. On failure returns false and sets *error.
    bool parseLine(const QByteArray &line, CANFrame *frame, QString *error);

    Result parseData(const QByteArray &content);
    Result parseFile(const QString &path);
}

#endif // CANDUMPPARSER_H
