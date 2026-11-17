#ifndef LOGFORMAT_H
#define LOGFORMAT_H

#include <QByteArray>
#include <QString>

// Finds out which log format a file holds by looking at its content.
// The file name and extension are never used.
namespace LogFormat
{
    enum Format
    {
        Unknown,
        Asc,        // Vector ASCII trace
        Blf,        // Vector binary log
        Trc,        // PEAK trace
        GvretCsv,   // SavvyCAN / GVRET CSV ("Time Stamp,ID,Extended,Dir,Bus,LEN,...")
        GenericCsv, // other CSV with a timestamp and an id column
        Candump     // SocketCAN candump -L
    };

    // How many bytes of the start of a file are enough to decide.
    const int headBytes = 8192;

    Format detect(const QByteArray &head);
    Format detectFile(const QString &path);

    // Short name for messages such as "Found format: Vector ASC".
    QString name(Format format);
}

#endif // LOGFORMAT_H
