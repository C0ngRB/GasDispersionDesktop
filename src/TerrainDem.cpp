#include "TerrainDem.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

static bool readAllBytes(const QString& path, QByteArray& out, QString& err) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        err = "Cannot open: " + path;
        return false;
    }
    out = f.readAll();
    return true;
}

bool TerrainDem::load(const QString& metaPath, const QString& binPath, QString& errOut) {
    errOut.clear();

    QByteArray metaBytes;
    if (!readAllBytes(metaPath, metaBytes, errOut)) return false;

    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(metaBytes, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        errOut = "Invalid meta json: " + pe.errorString();
        return false;
    }
    const QJsonObject o = doc.object();

    auto getInt = [&](const char* k, int& v)->bool{
        if (!o.contains(k) || !o.value(k).isDouble()) return false;
        v = o.value(k).toInt();
        return true;
    };
    auto getDouble = [&](const char* k, double& v)->bool{
        if (!o.contains(k) || !o.value(k).isDouble()) return false;
        v = o.value(k).toDouble();
        return true;
    };

    Meta m;
    if (!getInt("width", m.width) || !getInt("height", m.height)) { errOut = "meta missing width/height"; return false; }
    if (!getDouble("origin_x", m.origin_x) || !getDouble("origin_y", m.origin_y)) { errOut = "meta missing origin"; return false; }
    if (!getDouble("dx", m.dx) || !getDouble("dy", m.dy)) { errOut = "meta missing dx/dy"; return false; }
    getInt("epsg", m.epsg);
    if (o.contains("nodata") && o.value("nodata").isDouble()) m.nodata = static_cast<float>(o.value("nodata").toDouble());
    getDouble("z_min", m.z_min);
    getDouble("z_max", m.z_max);

    if (m.epsg == 0) {
        errOut = "EPSG is missing (epsg=0). DEM must be projected (meters).";
        return false;
    }

    QByteArray binBytes;
    if (!readAllBytes(binPath, binBytes, errOut)) return false;

    const std::size_t n = static_cast<std::size_t>(m.width) * static_cast<std::size_t>(m.height);
    if (binBytes.size() != static_cast<int>(n * sizeof(float))) {
        errOut = "dem_data.bin size mismatch";
        return false;
    }

    data_.resize(n);
    std::memcpy(data_.data(), binBytes.constData(), n * sizeof(float));
    meta_ = m;

    return true;
}

float TerrainDem::at(int i, int j) const {
    i = std::clamp(i, 0, meta_.width - 1);
    j = std::clamp(j, 0, meta_.height - 1);
    return data_[static_cast<std::size_t>(i) + static_cast<std::size_t>(j) * static_cast<std::size_t>(meta_.width)];
}

float TerrainDem::sampleBilinear(double x, double y) const {
    const double fx = (x - meta_.origin_x) / meta_.dx;
    const double fy = (y - meta_.origin_y) / meta_.dy;

    const double fxC = std::clamp(fx, 0.0, static_cast<double>(meta_.width - 1));
    const double fyC = std::clamp(fy, 0.0, static_cast<double>(meta_.height - 1));

    const int x0 = static_cast<int>(std::floor(fxC));
    const int y0 = static_cast<int>(std::floor(fyC));
    const int x1 = std::min(x0 + 1, meta_.width - 1);
    const int y1 = std::min(y0 + 1, meta_.height - 1);

    const double tx = fxC - x0;
    const double ty = fyC - y0;

    const double v00 = at(x0, y0);
    const double v10 = at(x1, y0);
    const double v01 = at(x0, y1);
    const double v11 = at(x1, y1);

    const double v0 = v00 * (1.0 - tx) + v10 * tx;
    const double v1 = v01 * (1.0 - tx) + v11 * tx;
    return static_cast<float>(v0 * (1.0 - ty) + v1 * ty);
}
