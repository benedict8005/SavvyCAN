#include "logreader.h"
#include <QFile>
#include "ascparser.h"
#include "blfhandler.h"
#include "candumpparser.h"
#include "csvparser.h"
#include "gvretparser.h"
#include "trcparser.h"

namespace LogReader
{
    QString Result::formatMessage() const
    {
        if (!recognised) return QStringLiteral("Unknown log format");
        return QStringLiteral("Found format: ") + LogFormat::name(format);
    }

    template <class R>
    static void take(Result &out, R &parsed)
    {
        out.opened = parsed.opened;
        out.frames = std::move(parsed.frames);
        for (const auto &e : parsed.errors)
            out.problems.append({e.line, e.message});
    }

    Result readFile(const QString &path)
    {
        Result out;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return out;
        const QByteArray head = file.read(LogFormat::headBytes);
        file.close();
        out.opened = true;
        out.format = LogFormat::detect(head);
        out.recognised = out.format != LogFormat::Unknown;

        switch (out.format)
        {
        case LogFormat::Asc:        { auto r = AscParser::parseFile(path);     take(out, r); break; }
        case LogFormat::Trc:        { auto r = TrcParser::parseFile(path);     take(out, r); break; }
        case LogFormat::Candump:    { auto r = CandumpParser::parseFile(path); take(out, r); break; }
        case LogFormat::GenericCsv: { auto r = CsvParser::parseFile(path);     take(out, r); break; }
        case LogFormat::GvretCsv:   { auto r = GvretParser::parseFile(path);   take(out, r); break; }
        case LogFormat::Blf:
        {
            BLFHandler blf;
            if (!blf.loadBLF(path, &out.frames))
                out.problems.append({0, QStringLiteral("BLF file is damaged or ends early")});
            break;
        }
        case LogFormat::Unknown:
            break;
        }
        out.opened = true; // the file was opened above for detection
        return out;
    }
}
