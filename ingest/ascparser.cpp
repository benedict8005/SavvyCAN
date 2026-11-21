#include "ascparser.h"

#include <QFile>

namespace AscParser
{

static bool isHex(const QByteArray &text)
{
    if (text.isEmpty()) return false;
    for (char c : text)
    {
        const bool ok = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
        if (!ok) return false;
    }
    return true;
}

static bool isNumber(const QByteArray &text, double *value = nullptr)
{
    bool ok = false;
    const double v = text.toDouble(&ok);
    if (ok && value) *value = v;
    return ok;
}

static bool isHeaderOrEvent(const QByteArray &line)
{
    static const char *prefixes[] = {"date ", "base ", "internal events", "no internal events", "//",
                                     "Begin ", "End ", "begin ", "end "};
    for (const char *prefix : prefixes)
        if (line.startsWith(prefix)) return true;
    static const char *events[] = {"Start of measurement", "ErrorFrame", "Statistic:", "TriggerBlock"};
    for (const char *event : events)
        if (line.contains(event)) return true;
    return false;
}

LineKind parseLine(const QByteArray &rawLine, CANFrame *frame, QString *error)
{
    const QByteArray trimmed = rawLine.trimmed();
    if (trimmed.isEmpty() || isHeaderOrEvent(trimmed)) return Skipped;

    const QList<QByteArray> tokens = trimmed.simplified().split(' ');
    double seconds = 0;
    if (tokens.size() < 2 || !isNumber(tokens[0], &seconds) || seconds < 0)
    {
        *error = "no structure";
        return Bad;
    }

    const bool fd = tokens[1] == "CANFD";
    // classic: time chan id dir type [len data...]
    // fd:      time CANFD chan dir id brs esi dlc len data... (trailing timing columns)
    const int minimum = fd ? 9 : 5;
    if (tokens.size() < minimum)
    {
        *error = "missing fields";
        return Bad;
    }

    bool ok = false;
    const int channel = tokens[fd ? 2 : 1].toInt(&ok);
    if (!ok || channel < 0)
    {
        *error = "bad channel";
        return Bad;
    }
    const QByteArray &direction = tokens[fd ? 3 : 3];
    QByteArray idText = tokens[fd ? 4 : 2];
    if (!fd && (direction != "Rx" && direction != "Tx"))
    {
        // some writers put the direction before the id; this reader needs the standard order
        *error = "no structure";
        return Bad;
    }
    const bool extended = idText.endsWith('x') || idText.endsWith('X');
    if (extended) idText.chop(1);
    if (!isHex(idText) || idText.size() > 8)
    {
        *error = "bad hex ID";
        return Bad;
    }
    const quint32 id = idText.toUInt(nullptr, 16);
    if (id > 0x1FFFFFFF)
    {
        *error = "ID too large";
        return Bad;
    }

    bool remote = false;
    bool brs = false;
    bool esi = false;
    int length = 0;
    int firstData = 0;
    if (fd)
    {
        brs = tokens[5] == "1";
        esi = tokens[6] == "1";
        length = tokens[8].toInt(&ok);
        if (!ok || length < 0 || length > 64)
        {
            *error = "bad FD length";
            return Bad;
        }
        firstData = 9;
    }
    else if (tokens[4] == "r" || tokens[4] == "R")
    {
        remote = true;
    }
    else if (tokens[4] != "d" && tokens[4] != "D")
    {
        *error = "no structure";
        return Bad;
    }
    else
    {
        if (tokens.size() < 6)
        {
            *error = "missing fields";
            return Bad;
        }
        length = tokens[5].toInt(&ok);
        if (!ok || length < 0)
        {
            *error = "bad length";
            return Bad;
        }
        if (length > 8)
        {
            *error = QString("classic frame with DLC %1").arg(length);
            return Bad;
        }
        firstData = 6;
    }

    QByteArray payload;
    if (!remote)
    {
        if (tokens.size() - firstData < length)
        {
            // check the bytes that are there before complaining about their number
            for (int i = firstData; i < tokens.size(); i++)
                if (!isHex(tokens[i]) || tokens[i].size() > 2)
                {
                    *error = "non-hex data";
                    return Bad;
                }
            *error = "fewer data bytes than the length";
            return Bad;
        }
        for (int i = 0; i < length; i++)
        {
            const QByteArray &byte = tokens[firstData + i];
            if (!isHex(byte) || byte.size() > 2)
            {
                *error = "non-hex data";
                return Bad;
            }
            payload.append(char(byte.toInt(nullptr, 16)));
        }
    }

    CANFrame result;
    result.bus = channel;
    result.setFrameId(id);
    result.setExtendedFrameFormat(extended);
    result.setFrameType(remote ? QCanBusFrame::RemoteRequestFrame : QCanBusFrame::DataFrame);
    result.setFlexibleDataRateFormat(fd);
    result.setBitrateSwitch(brs);
    result.setErrorStateIndicator(esi);
    result.setPayload(payload);
    result.setTimeStamp(QCanBusFrame::TimeStamp::fromMicroSeconds(qRound64(seconds * 1000000.0)));
    result.isReceived = direction != "Tx";
    *frame = result;
    return Frame;
}

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
        const QByteArray line = content.mid(pos, end - pos);
        pos = end + 1;
        lineNumber++;

        CANFrame frame;
        QString message;
        switch (parseLine(line, &frame, &message))
        {
        case Frame: result.frames.append(frame); break;
        case Skipped: break;
        case Bad: result.errors.append({lineNumber, message}); break;
        }
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
