#include "logformat.h"

#include <QFile>
#include <QRegularExpression>
#include <QList>

namespace LogFormat
{

static bool looksLikeAsc(const QString &line)
{
    return line.startsWith("date ") || line.startsWith("base ") ||
           line.startsWith("Begin Triggerblock") || line.startsWith("no internal events") ||
           line.startsWith("internal events logged");
}

static bool looksLikeCandump(const QString &line)
{
    static const QRegularExpression re(R"(^\(\d+(\.\d+)?\)\s+\S+\s+[0-9A-Za-z]+#)");
    return re.match(line).hasMatch();
}

static Format csvFormat(const QString &line)
{
    const QStringList cols = line.toLower().split(',');
    if (cols.size() < 3) return Unknown;
    QStringList trimmed;
    for (const QString &c : cols) trimmed.append(c.trimmed());
    if (trimmed.contains("time stamp") && trimmed.contains("id")) return GvretCsv;
    bool hasTime = false, hasId = false;
    for (const QString &c : trimmed)
    {
        if (c.contains("time")) hasTime = true;
        if (c == "id" || c.contains("arbitration_id") || c == "can id" || c == "identifier") hasId = true;
    }
    return (hasTime && hasId) ? GenericCsv : Unknown;
}

Format detect(const QByteArray &head)
{
    if (head.startsWith("LOGG")) return Blf;

    const QList<QByteArray> rawLines = head.left(headBytes).split('\n');
    bool first = true;
    for (const QByteArray &raw : rawLines)
    {
        QString line = QString::fromLatin1(raw).trimmed();
        if (line.isEmpty()) continue;
        if (first && line.startsWith(QChar(0xFEFF))) line.remove(0, 1);
        first = false;

        if (line.startsWith(";$FILEVERSION") || line.startsWith(";$COLUMNS")) return Trc;
        if (line.startsWith(';')) continue; // other PEAK comment lines; keep looking
        if (looksLikeAsc(line)) return Asc;
        if (looksLikeCandump(line)) return Candump;
        Format csv = csvFormat(line);
        if (csv != Unknown) return csv;
        // an ASC frame or event line, e.g. "0.000000 1 100 Rx d 8 ..."
        static const QRegularExpression ascFrame(R"(^\d+\.\d+\s+(\d+|CANFD)\s+\S+\s+(Rx|Tx))");
        if (ascFrame.match(line).hasMatch()) return Asc;
        return Unknown;
    }
    return Unknown;
}

Format detectFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return Unknown;
    return detect(file.read(headBytes));
}

QString name(Format format)
{
    switch (format)
    {
    case Asc: return QStringLiteral("Vector ASC");
    case Blf: return QStringLiteral("Vector BLF");
    case Trc: return QStringLiteral("PEAK TRC");
    case GvretCsv: return QStringLiteral("GVRET CSV");
    case GenericCsv: return QStringLiteral("CSV");
    case Candump: return QStringLiteral("candump");
    default: return QStringLiteral("unknown");
    }
}

}
