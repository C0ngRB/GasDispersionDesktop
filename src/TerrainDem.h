#pragma once
#include <QString>
#include <vector>

class TerrainDem {
public:
    struct Meta {
        int width = 0;
        int height = 0;
        double origin_x = 0.0;
        double origin_y = 0.0;
        double dx = 1.0;
        double dy = -1.0;
        int epsg = 0;
        float nodata = -3.4e38f;
        double z_min = 0.0;
        double z_max = 0.0;
    };

    bool load(const QString& metaPath, const QString& binPath, QString& errOut);

    const Meta& meta() const { return meta_; }
    const std::vector<float>& data() const { return data_; }

    double minX() const { return meta_.origin_x; }
    double maxX() const { return meta_.origin_x + (meta_.width - 1) * meta_.dx; }
    double maxY() const { return meta_.origin_y; }
    double minY() const { return meta_.origin_y + (meta_.height - 1) * meta_.dy; }

    float sampleBilinear(double x, double y) const;

private:
    Meta meta_;
    std::vector<float> data_;

    float at(int i, int j) const;
};
