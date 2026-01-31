#include "ExporterCsv.h"
#include <QFile>
#include <QTextStream>

bool ExporterCsv::writeGridFrame(const QString& path,
                                 const FrameMeta& meta,
                                 const std::vector<float>& gridValues,
                                 QString& errOut)
{
    errOut.clear();
    const std::size_t expected = static_cast<std::size_t>(meta.Nx) * meta.Ny;
    if (gridValues.size() != expected) {
        errOut = "Grid values size mismatch";
        return false;
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        errOut = "Cannot write: " + path;
        return false;
    }

    QTextStream out(&f);
    out.setRealNumberNotation(QTextStream::ScientificNotation);
    out.setRealNumberPrecision(6);

    out << "# crs=EPSG:" << meta.epsg << "\n";
    out << "# origin_x=" << meta.origin_x << " origin_y=" << meta.origin_y
        << " dx=" << meta.dx << " dy=" << meta.dy << " z=" << meta.z << "\n";
    out << "# Nx=" << meta.Nx << " Ny=" << meta.Ny << " t=" << meta.t << "\n";

    for (int j = 0; j < meta.Ny; ++j) {
        for (int i = 0; i < meta.Nx; ++i) {
            const float v = gridValues[static_cast<std::size_t>(i) + static_cast<std::size_t>(j) * meta.Nx];
            out << v;
            if (i + 1 < meta.Nx) out << ",";
        }
        out << "\n";
    }

    return true;
}
