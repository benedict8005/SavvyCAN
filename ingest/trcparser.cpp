#include "trcparser.h"

#include <QFile>

namespace TrcParser
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

static int lengthFromDlc(int dlc)
{
    static const int table[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};
    return table[dlc & 15];
}

Result parseData(const QByteArray &content)
{
    Result result;
    result.opened = true;
    result.frames.reserve(content.count('\n')); // one frame per line at most, avoids regrowing

    QByteArray version;
    int versionLine = 0;
    QList<QByteArray> columns;
    int columnIndex[256]; // position of each column letter, -1 when the file has none
    bool headerChecked = false;
    int lineNumber = 0;
    int pos = 0;
    for (int &index : columnIndex) index = -1;

    while (pos < content.size())
    {
        int end = content.indexOf('\n', pos);
        if (end < 0) end = content.size();
        const QByteArray line = QByteArray::fromRawData(content.constData() + pos, end - pos).trimmed();
        pos = end + 1;
        lineNumber++;
        if (line.isEmpty()) continue;

        if (line.startsWith(';'))
        {
            if (line.startsWith(";$FILEVERSION="))
            {
                version = line.mid(14).trimmed();
                versionLine = lineNumber;
            }
            else if (line.startsWith(";$COLUMNS="))
            {
                columns = line.mid(10).trimmed().split(',');
                for (int &index : columnIndex) index = -1;
                for (int i = 0; i < columns.size(); i++)
                    if (columns[i].size() == 1 && columnIndex[uchar(columns[i][0])] < 0)
                        columnIndex[uchar(columns[i][0])] = i;
            }
            continue;
        }

        if (!headerChecked)
        {
            // first row: the header is complete, so decide whether the rows can be read
            headerChecked = true;
            if (version != "2.1")
            {
                const bool known = version == "1.0" || version == "1.1" || version == "1.2" ||
                                   version == "1.3" || version == "2.0";
                result.errors.append({versionLine ? versionLine : lineNumber,
                                      known ? "unsupported file version" : "unknown file version"});
                return result;
            }
            if (columns.isEmpty())
            {
                result.errors.append({lineNumber, "no column definition"});
                return result;
            }
        }

        const QList<QByteArray> tokens = line.simplified().split(' ');
        auto column = [&](char name) -> QByteArray {
            const int i = columnIndex[uchar(name)];
            return (i >= 0 && i < tokens.size()) ? tokens[i] : QByteArray();
        };
        // the data column is last and runs to the end of the line
        const int dataIndex = columnIndex[uchar('D')];

        if (tokens.size() < columns.size() - 1)
        {
            result.errors.append({lineNumber, "missing fields"});
            continue;
        }

        const QByteArray type = column('T');
        if (type == "ER" || type == "EC" || type == "ST") continue;
        const bool remote = type == "RR";
        const bool fd = type == "FD" || type == "FB" || type == "FE" || type == "BI";
        if (!remote && !fd && type != "DT")
        {
            result.errors.append({lineNumber, "unknown row type"});
            continue;
        }

        bool ok = false;
        const double millis = column('O').toDouble(&ok);
        if (!ok || millis < 0)
        {
            result.errors.append({lineNumber, "bad time"});
            continue;
        }
        const int bus = column('B').toInt(&ok);
        if (!ok || bus < 0)
        {
            result.errors.append({lineNumber, "bad bus"});
            continue;
        }
        const QByteArray idText = column('I');
        if (!isHex(idText) || idText.size() > 8)
        {
            result.errors.append({lineNumber, "bad hex ID"});
            continue;
        }
        const quint32 id = idText.toUInt(nullptr, 16);
        if (id > 0x1FFFFFFF)
        {
            result.errors.append({lineNumber, "ID too large"});
            continue;
        }
        const QByteArray direction = column('d');
        if (direction != "Rx" && direction != "Tx")
        {
            result.errors.append({lineNumber, "bad direction"});
            continue;
        }
        const int dlc = column('L').toInt(&ok);
        if (!ok || dlc < 0 || dlc > (fd ? 15 : 8))
        {
            result.errors.append({lineNumber, "bad length"});
            continue;
        }
        const int length = fd ? lengthFromDlc(dlc) : dlc;

        QByteArray payload;
        if (!remote)
        {
            const int first = dataIndex;
            if (first < 0 || tokens.size() - first < length)
            {
                bool allHex = true;
                for (int i = qMax(first, 0); i < tokens.size(); i++)
                    if (!isHex(tokens[i]) || tokens[i].size() > 2) allHex = false;
                result.errors.append({lineNumber, allHex ? "fewer data bytes than the length"
                                                         : "non-hex data"});
                continue;
            }
            bool bytesOk = true;
            for (int i = 0; i < length; i++)
            {
                const QByteArray &byte = tokens[first + i];
                if (!isHex(byte) || byte.size() > 2) { bytesOk = false; break; }
                payload.append(char(byte.toInt(nullptr, 16)));
            }
            if (!bytesOk)
            {
                result.errors.append({lineNumber, "non-hex data"});
                continue;
            }
        }

        CANFrame frame;
        frame.bus = bus;
        frame.setFrameId(id);
        frame.setExtendedFrameFormat(idText.size() > 4);
        frame.setFrameType(remote ? QCanBusFrame::RemoteRequestFrame : QCanBusFrame::DataFrame);
        frame.setFlexibleDataRateFormat(fd);
        frame.setBitrateSwitch(type == "FB" || type == "BI");
        frame.setPayload(payload);
        frame.setTimeStamp(QCanBusFrame::TimeStamp::fromMicroSeconds(qRound64(millis * 1000.0)));
        frame.isReceived = direction == "Rx";
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
