#include "csvparser.h"

#include <QFile>

namespace CsvParser
{

namespace
{
struct Columns
{
    int time = -1, id = -1, data = -1, extended = -1, remote = -1, bus = -1;
    int needed = 0; // highest column index we read
    bool complete() const { return time >= 0 && id >= 0 && data >= 0; }
};

Columns readHeader(const QList<QByteArray> &names)
{
    Columns c;
    for (int i = 0; i < names.size(); i++)
    {
        const QByteArray name = names[i].trimmed().toLower();
        if (name.contains("time")) { if (c.time < 0) c.time = i; }
        else if (name == "id" || name == "can id" || name == "identifier" || name.contains("arbitration_id")) { if (c.id < 0) c.id = i; }
        else if (name == "data") c.data = i;
        else if (name == "extended") c.extended = i;
        else if (name == "remote") c.remote = i;
        else if (name == "bus" || name == "channel") c.bus = i;
    }
    c.needed = qMax(qMax(c.time, c.id), qMax(c.data, qMax(c.extended, qMax(c.remote, c.bus))));
    return c;
}

bool isFlagOn(const QByteArray &text)
{
    const QByteArray t = text.trimmed().toLower();
    return t == "1" || t == "true" || t == "yes";
}
}

Result parseData(const QByteArray &content)
{
    Result result;
    result.opened = true;
    Columns columns;
    bool haveHeader = false;
    int lineNumber = 0;
    int pos = 0;
    while (pos < content.size())
    {
        int end = content.indexOf('\n', pos);
        if (end < 0) end = content.size();
        QByteArray line = content.mid(pos, end - pos);
        pos = end + 1;
        lineNumber++;
        line = line.trimmed();
        if (lineNumber == 1 && line.startsWith("\xEF\xBB\xBF")) line.remove(0, 3);
        if (line.isEmpty()) continue;

        const QList<QByteArray> fields = line.split(',');
        if (!haveHeader)
        {
            haveHeader = true;
            columns = readHeader(fields);
            if (!columns.complete())
            {
                result.errors.append({lineNumber, "missing required columns"});
                return result;
            }
            continue;
        }

        if (fields.size() <= columns.needed)
        {
            result.errors.append({lineNumber, "missing fields"});
            continue;
        }

        bool ok = false;
        const double seconds = fields[columns.time].trimmed().toDouble(&ok);
        if (!ok || seconds < 0)
        {
            result.errors.append({lineNumber, "bad timestamp"});
            continue;
        }

        QByteArray idText = fields[columns.id].trimmed();
        if (idText.startsWith("0x") || idText.startsWith("0X")) idText.remove(0, 2);
        const quint64 id = idText.toULongLong(&ok, 16);
        if (idText.isEmpty() || !ok || id > 0x1FFFFFFF)
        {
            result.errors.append({lineNumber, "bad ID"});
            continue;
        }

        const QByteArray dataText = fields[columns.data].trimmed();
        if (dataText.size() % 2 != 0)
        {
            result.errors.append({lineNumber, "odd number of hex digits"});
            continue;
        }
        bool hexOnly = true;
        for (char c : dataText)
            if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) hexOnly = false;
        if (!hexOnly)
        {
            result.errors.append({lineNumber, "non-hex data"});
            continue;
        }
        const QByteArray payload = QByteArray::fromHex(dataText);
        if (payload.size() > 64)
        {
            result.errors.append({lineNumber, "too many data bytes"});
            continue;
        }

        int bus = 0;
        if (columns.bus >= 0)
        {
            bus = fields[columns.bus].trimmed().toInt(&ok);
            if (!ok || bus < 0)
            {
                result.errors.append({lineNumber, "bad bus"});
                continue;
            }
        }

        const bool remote = columns.remote >= 0 && isFlagOn(fields[columns.remote]);
        const bool extended = (columns.extended >= 0 && isFlagOn(fields[columns.extended])) || id > 0x7FF;

        CANFrame frame;
        frame.bus = bus;
        frame.setFrameId(quint32(id));
        frame.setExtendedFrameFormat(extended);
        frame.setFrameType(remote ? QCanBusFrame::RemoteRequestFrame : QCanBusFrame::DataFrame);
        frame.setFlexibleDataRateFormat(payload.size() > 8);
        frame.setPayload(remote ? QByteArray() : payload);
        frame.setTimeStamp(QCanBusFrame::TimeStamp::fromMicroSeconds(qRound64(seconds * 1000000.0)));
        frame.isReceived = true;
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
