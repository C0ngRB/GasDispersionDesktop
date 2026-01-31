#pragma once
#include <vector>
#include <cstdint>
#include <QString>

#include "Grid3D.h"
#include "TerrainDem.h"

class Simulator3D {
public:
    struct Params {
        double totalTime_s = 60.0;
        double dt_s = 0.05;
        bool autoClampDt = true;
        double windSpeed_mps = 2.0;
        double windDir_deg = 0.0;
        double K_m2ps = 1.0;
        double decay_1ps = 0.0;
        double srcX_m = 0.0;
        double srcY_m = 0.0;
        double srcZ_m = 0.0;
        double srcRadius_m = 2.0;
        double leakRate = 1.0;
    };

    Simulator3D() = default;

    bool initialize(const Grid3D& grid, const TerrainDem& dem, const Params& p, QString& errOut);
    void reset();
    double step();
    double stableDt() const;

    const Grid3D& grid() const { return grid_; }
    const Params& params() const { return p_; }
    double time() const { return t_; }
    float maxC() const { return maxC_; }

    void extractSliceXY(int zIndex, std::vector<float>& outSlice, float& outMax) const;
    float groundZ(double x, double y) const { return dem_ ? dem_->sampleBilinear(x, y) : 0.0f; }

private:
    Grid3D grid_;
    Params p_;
    const TerrainDem* dem_{nullptr};
    double t_{0.0};
    double u_{0.0}, v_{0.0}, w_{0.0};
    std::vector<float> C_;
    std::vector<float> Cnew_;
    std::vector<std::uint8_t> solid_;
    float maxC_{0.0f};

    inline bool isSolid(int i, int j, int k) const {
        return solid_[grid_.idx(i,j,k)] != 0;
    }

    float sampleC(int i, int j, int k) const;
    void buildSolidMask(QString& errOut);
};
