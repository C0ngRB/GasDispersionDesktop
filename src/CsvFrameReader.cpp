#include "CsvFrameReader.h"

#include <QFile>
#include <QTextStream>
#include <QStringList>

static bool parseHeader(const QString& line, int& Nx, int& Ny, double& t) {
    // line: "# Nx=300 Ny=150 t=0.50"
    if (!line.trimmed().startsWith("#")) return false;
    const auto parts = line.mid(1).trimmed().split(QRegExp("\\s+"), Qt::SkipEmptyParts);
    bool okAny = false;
    for (const auto& p : parts) {
        if (p.startsWith("Nx=")) { Nx = p.mid(3).toInt(&okAny); }
        else if (p.startsWith("Ny=")) { Ny = p.mid(3).toInt(&okAny); }
        else if (p.startsWith("t=")) { t = p.mid(2).toDouble(&okAny); }
    }
    return true;
}


bool CsvFrameReader::read(const QString& path, Frame& out, QString& err) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        err = "Cannot open file: " + path;
        return false;
    }

    QTextStream in(&f);
    in.setCodec("UTF-8");

    QString first = in.readLine();
    int Nx = 0, Ny = 0;
    double t = 0.0;

    QStringList dataLines;
    if (first.trimmed().startsWith("#")) {
        parseHeader(first, Nx, Ny, t);
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            if (!line.isEmpty()) dataLines << line;
        }
    } else {
        dataLines << first.trimmed();
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            if (!line.isEmpty()) dataLines << line;
        }
    }

    if (dataLines.isEmpty()) {
        err = "Empty CSV data: " + path;
        return false;
    }

    // Deduce Nx if not provided
    if (Nx <= 0) {
        const auto cols = dataLines[0].split(",", Qt::KeepEmptyParts);
        Nx = cols.size();
    }
    if (Ny <= 0) Ny = dataLines.size();

    if (dataLines.size() != Ny) {
        // Allow mismatch but clamp to min
        Ny = std::min(Ny, dataLines.size());
    }

    out.Nx = Nx;
    out.Ny = Ny;
    out.t = t;
    out.data.assign((size_t)Nx * (size_t)Ny, 0.0f);
    for (int j = 0; j < Ny; ++j) {
        const auto cols = dataLines[j].split(",", Qt::KeepEmptyParts);
        if (cols.size() < Nx) {
            err = "Row has fewer columns than Nx in: " + path;
            return false;
        }
        for (int i = 0; i < Nx; ++i) {
            bool ok = false;
            float v = cols[i].toFloat(&ok);
            if (!ok) v = 0.0f;
            out.data[(size_t)i + (size_t)j * (size_t)Nx] = v;
        }
    }
    return true;
}
