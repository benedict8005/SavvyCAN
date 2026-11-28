#ifndef GVRETPARSER_H
#define GVRETPARSER_H

#include <QByteArray>
#include <QList>
#include <QString>
#include <QVector>
#include "can_structs.h"

// Reads the GVRET / SavvyCAN native CSV log and says which lines it could not read.
//
//   Time Stamp,ID,Extended,Dir,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8      (version 2)
//   Time Stamp,ID,Extended,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8          (version 1, no Dir column)
//
// Time stamps are whole microseconds, ids are hex, Extended is true or false, Dir is Rx or Tx.
// The Bus value is kept exactly as written: bus 0 stays bus 0. A line with fewer data bytes than
// LEN, a length above 8, a bad number or a missing field is reported with its 1-based line number
// and is not turned into a frame. A file whose first line is not a GVRET header is reported once
// on line 1 and no frames are read.
namespace GvretParser
{
    struct LineError
    {
        int line;
        QString message;
    };

    struct Result
    {
        QVector<CANFrame> frames;
        QList<LineError> errors;
        int version = 0;     // 1 or 2, 0 when no valid header was found
        bool opened = false; // false when the file could not be opened
    };

    Result parseData(const QByteArray &content);
    Result parseFile(const QString &path);
}

#endif // GVRETPARSER_H
