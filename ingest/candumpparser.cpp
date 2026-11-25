#include "candumpparser.h"

#include <QFile>

namespace CandumpParser
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

bool parseLine(const QByteArray &rawLine, CANFrame *frame, QString *error)
{
    const QList<QByteArray> tokens = rawLine.simplified().split(' ');
    if (tokens.size() < 3)
    {
        *error = "not a frame";
        return false;
    }

    // timestamp: "(seconds.fraction)"
    const QByteArray &stamp = tokens[0];
    if (!stamp.startsWith('('))
    {
        *error = "not a frame";
        return false;
    }
    if (!stamp.endsWith(')'))
    {
        *error = "missing ')'";
        return false;
    }
    bool ok = false;
    const double seconds = stamp.mid(1, stamp.size() - 2).toDouble(&ok);
    if (!ok || seconds < 0)
    {
        *error = "bad timestamp";
        return false;
    }

    // bus: the number at the end of the interface name, 0 if there is none
    int bus = 0;
    const QByteArray &iface = tokens[1];
    int digits = 0;
    while (digits < iface.size() && iface[iface.size() - 1 - digits] >= '0' && iface[iface.size() - 1 - digits] <= '9')
        digits++;
    if (digits > 0) bus = iface.right(digits).toInt();

    // id and data
    const QByteArray &body = tokens[2];
    const int hash = body.indexOf('#');
    if (hash < 0)
    {
        *error = "missing '#'";
        return false;
    }
    const bool fd = body.mid(hash, 2) == "##";
    const QByteArray idText = body.left(hash);
    if (!isHex(idText))
    {
        *error = "bad hex ID";
        return false;
    }
    if (idText.size() != 3 && idText.size() != 8)
    {
        *error = "bad ID length";
        return false;
    }
    const quint32 id = idText.toUInt(nullptr, 16);
    if (id > 0x1FFFFFFF)
    {
        *error = "ID too large";
        return false;
    }

    QByteArray dataText = body.mid(hash + (fd ? 2 : 1));
    bool remote = false;
    bool brs = false;
    bool esi = false;
    if (fd)
    {
        if (dataText.isEmpty() || !isHex(dataText.left(1)))
        {
            *error = "bad FD flags";
            return false;
        }
        const int flags = dataText.left(1).toInt(nullptr, 16);
        brs = flags & 1;
        esi = flags & 2;
        dataText = dataText.mid(1);
    }
    else if (dataText.startsWith('R'))
    {
        remote = true;
        dataText.clear();
    }

    if (!dataText.isEmpty() && !isHex(dataText))
    {
        *error = "non-hex data";
        return false;
    }
    if (dataText.size() % 2)
    {
        *error = "odd number of data digits";
        return false;
    }
    const QByteArray payload = QByteArray::fromHex(dataText);
    if (!fd && payload.size() > 8)
    {
        *error = "classic frame with more than 8 data bytes";
        return false;
    }
    if (fd && payload.size() > 64)
    {
        *error = "FD frame with more than 64 data bytes";
        return false;
    }

    CANFrame result;
    result.bus = bus;
    result.setFrameId(id);
    result.setExtendedFrameFormat(idText.size() == 8);
    result.setFrameType(remote ? QCanBusFrame::RemoteRequestFrame : QCanBusFrame::DataFrame);
    result.setFlexibleDataRateFormat(fd);
    result.setBitrateSwitch(brs);
    result.setErrorStateIndicator(esi);
    result.setPayload(payload);
    result.setTimeStamp(QCanBusFrame::TimeStamp::fromMicroSeconds(qRound64(seconds * 1000000.0)));
    result.isReceived = !(tokens.size() > 3 && tokens[3] == "T");
    *frame = result;
    return true;
}

Result parseData(const QByteArray &content)
{
    Result result;
    result.opened = true;
    result.frames.reserve(content.count('\n')); // one frame per line at most, avoids regrowing
    int lineNumber = 0;
    int pos = 0;
    while (pos < content.size())
    {
        int end = content.indexOf('\n', pos);
        if (end < 0) end = content.size();
        const QByteArray line = QByteArray::fromRawData(content.constData() + pos, end - pos);
        pos = end + 1;
        lineNumber++;

        if (line.trimmed().isEmpty()) continue;
        CANFrame frame;
        QString message;
        if (parseLine(line, &frame, &message))
            result.frames.append(frame);
        else
            result.errors.append({lineNumber, message});
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
