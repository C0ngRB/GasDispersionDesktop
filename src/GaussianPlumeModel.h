#pragma once
#include <string>
#include <vector>

#include <QString>

#include "Grid3D.h"
#include "ITerrain.h"

class GaussianPlumeModel
{
public:
    struct Params
    {
        double windSpeed_mps = 2.0;
        double windDir_deg = 0.0;

        double K_m2ps = 2.0;
        double decay_1ps = 0.0;

        double srcX_m = 0.0;
        double srcY_m = 0.0;
        double srcZ_m = 2.0;
        double srcRadius_m = 2.0;

        double leakRate_kgps = 1.0;

        double dt_s = 0.05;
        double totalTime_s = 60.0;
    };

    bool initialize(const Grid3D &g, const ITerrain *terr, const Params &p, QString &err);
    void reset();
    void step();

    double time() const;
    const Grid3D &grid() const;
    Params params() const;

    void extractSliceXY(int k, std::vector<float> &out, float &maxC) const;

private:
    double terrainHeight(double x, double y) const;
    double concentrationAt(double x, double y, double z) const;

    Grid3D grid_;
    const ITerrain *terr_ = nullptr;
    Params params_;
    double t_ = 0.0;

    double u_ = 0.0;
    double v_ = 0.0;
};
