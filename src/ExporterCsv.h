#pragma once
#include <QString>
#include <vector>

class ExporterCsv {
public:
    struct FrameMeta {
        int epsg = 0;
        double origin_x = 0.0;
        double origin_y = 0.0;
        double dx = 1.0;
        double dy = 1.0;
        double z  = 0.0;
        int Nx = 0;
        int Ny = 0;
        double t = 0.0;
    };

    static bool writeGridFrame(const QString& path,
                               const FrameMeta& meta,
                               const std::vector<float>& gridValues,
                               QString& errOut);
};
