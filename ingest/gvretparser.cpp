#include "gvretparser.h"

#include <QFile>

namespace GvretParser
{

Result parseData(const QByteArray &content)
{
    Result result;
    result.opened = true;
    int lineNumber = 0;
    int pos = 0;
    while (pos < content.size())
    {
        int end = content.indexOf('\n', pos);
        if (end < 0) end = content.size();
        QByteArray line = content.mid(pos, end - pos).trimmed();
        pos = end + 1;
        lineNumber++;
        if (lineNumber == 1 && line.startsWith("\xEF\xBB\xBF")) line.remove(0, 3);
        if (line.isEmpty()) continue;

        const QList<QByteArray> fields = line.split(',');
        if (result.version == 0)
        {
            const QByteArray upper = line.toUpper();
            if (!upper.startsWith("TIME STAMP,ID,EXTENDED,") || !upper.contains(",D1"))
            {
                result.errors.append({lineNumber, "not a GVRET header"});
                return result;
            }
            result.version = (fields.size() > 3 && fields[3].trimmed().toUpper() == "DIR") ? 2 : 1;
            continue;
        }

        const int lenField = result.version == 2 ? 5 : 4;
        const int busField = lenField - 1;
        if (fields.size() <= lenField)
        {
            result.errors.append({lineNumber, "missing fields"});
            continue;
        }

        bool ok = false;
        const qulonglong micros = fields[0].trimmed().toULongLong(&ok);
        if (!ok)
        {
            result.errors.append({lineNumber, "bad timestamp"});
            continue;
        }
        const quint64 id = fields[1].trimmed().toULongLong(&ok, 16);
        if (!ok || id > 0x1FFFFFFF)
        {
            result.errors.append({lineNumber, "bad ID"});
            continue;
        }
        const QByteArray ext = fields[2].trimmed().toLower();
        if (ext != "true" && ext != "false")
        {
            result.errors.append({lineNumber, "bad extended flag"});
            continue;
        }
        bool received = true;
        if (result.version == 2)
        {
            const QByteArray dir = fields[3].trimmed().toLower();
            if (dir != "rx" && dir != "tx")
            {
                result.errors.append({lineNumber, "bad direction"});
                continue;
            }
            received = dir == "rx";
        }
        const int bus = fields[busField].trimmed().toInt(&ok);
        if (!ok || bus < 0)
        {
            result.errors.append({lineNumber, "bad bus"});
            continue;
        }
        const int len = fields[lenField].trimmed().toInt(&ok);
        if (!ok || len < 0 || len > 8)
        {
            result.errors.append({lineNumber, "bad length"});
            continue;
        }
        if (fields.size() < lenField + 1 + len)
        {
            result.errors.append({lineNumber, "fewer data bytes than length"});
            continue;
        }
        QByteArray payload(len, 0);
        bool dataOk = true;
        for (int i = 0; i < len; i++)
        {
            const int value = fields[lenField + 1 + i].trimmed().toInt(&ok, 16);
            if (!ok || value < 0 || value > 255) { dataOk = false; break; }
            payload[i] = char(value);
        }
        if (!dataOk)
        {
            result.errors.append({lineNumber, "non-hex data"});
            continue;
        }

        CANFrame frame;
        frame.bus = bus;
        frame.setFrameId(quint32(id));
        frame.setExtendedFrameFormat(ext == "true" || id > 0x7FF);
        frame.setFrameType(QCanBusFrame::DataFrame);
        frame.setPayload(payload);
        frame.setTimeStamp(QCanBusFrame::TimeStamp(0, qint64(micros)));
        frame.isReceived = received;
        result.frames.append(frame);
    }
    return result;
}

Result parseFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return Result();
    return parseData(file.readAll());
}

}
