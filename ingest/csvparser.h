#ifndef CSVPARSER_H
#define CSVPARSER_H

#include <QByteArray>
#include <QList>
#include <QString>
#include <QVector>
#include "can_structs.h"

// Reads generic CSV logs whose first line names the columns, and says which rows it could not read.
//
//   timestamp,arbitration_id,data
//   0.003000,100,c02b327e00000000
//
// Required columns (matched by name, any order, case ignored): a time column (its name contains
// "time", value in seconds), an id column (id, can id, identifier or arbitration_id, value in hex
// with an optional 0x) and a data column ("data", value as hex digits). Optional columns:
// extended, remote, bus (or channel). The data length is the number of hex bytes, so no length
// column is needed. An id above 0x7FF is extended even when there is no extended column.
// Blank lines are skipped. Every other row that is not a frame is reported with its 1-based line
// number. A header that lacks a required column is reported once, on the header line, and no
// frames are read from the file.
namespace CsvParser
{
    struct LineError
    {
        int line;        // 1-based line number in the file
        QString message; // short reason, for example "missing fields"
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

#endif // CSVPARSER_H
