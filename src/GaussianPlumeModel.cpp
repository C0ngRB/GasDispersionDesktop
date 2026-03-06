#include "GaussianPlumeModel.h"

#include <algorithm>
#include <cmath>

static inline double deg2rad(double d) { return d * M_PI / 180.0; }

bool GaussianPlumeModel::initialize(const Grid3D &g, const ITerrain *terr, const Params &p, QString &err)
{
    (void)err;
    grid_ = g;
    terr_ = terr;
    params_ = p;
    t_ = 0.0;

    const double th = deg2rad(params_.windDir_deg);
    u_ = params_.windSpeed_mps * std::cos(th);
    v_ = params_.windSpeed_mps * std::sin(th);

    if (params_.K_m2ps <= 0.0)
        params_.K_m2ps = 1e-6;

    return true;
}

void GaussianPlumeModel::reset()
{
    t_ = 0.0;
}

void GaussianPlumeModel::step()
{
    t_ += params_.dt_s;
}

double GaussianPlumeModel::time() const { return t_; }

const Grid3D &GaussianPlumeModel::grid() const { return grid_; }
GaussianPlumeModel::Params GaussianPlumeModel::params() const { return params_; }

double GaussianPlumeModel::terrainHeight(double x, double y) const
{
    if (!terr_)
        return 0.0;
    return terr_->height(x, y);
}

double GaussianPlumeModel::concentrationAt(double x, double y, double z) const
{
    const double gx = params_.srcX_m;
    const double gy = params_.srcY_m;
    const double gz = params_.srcZ_m;

    const double h = terrainHeight(x, y);
    if (z < h)
        return 0.0;

    const double h0 = terrainHeight(gx, gy);

    const double U = std::hypot(u_, v_);
    if (U < 1e-9)
        return 0.0;

    const double dx = x - gx;
    const double dy = y - gy;

    const double ex = u_ / U;
    const double ey = v_ / U;

    const double xw = dx * ex + dy * ey;
    const double yw = -dx * ey + dy * ex;
    const double zw = z - gz;

    if (xw <= 1e-6)
        return 0.0;

    const double K = params_.K_m2ps;
    const double Q = params_.leakRate_kgps;
    const double lam = params_.decay_1ps;

    const double decay = (lam > 0.0) ? std::exp(-lam * xw / U) : 1.0;

    const double denom = 4.0 * M_PI * K * xw;
    const double a = U / (4.0 * K * xw);

    auto core = [&](double zrel)
    {
        const double e = std::exp(-a * (yw * yw + zrel * zrel));
        return (Q / denom) * e * decay;
    };

    const double gz_mirror = 2.0 * h0 - gz;
    const double zrel_m = z - gz_mirror;

    const double c = core(zw) + core(zrel_m);

    return std::max(0.0, c);
}

void GaussianPlumeModel::extractSliceXY(int k, std::vector<float> &out, float &maxC) const
{
    const int Nx = grid_.Nx;
    const int Ny = grid_.Ny;
    out.assign(Nx * Ny, 0.0f);
    maxC = 0.0f;

    const double z = grid_.z0 + k * grid_.dz;

    for (int j = 0; j < Ny; ++j)
    {
        const double y = grid_.y0 + j * grid_.dy;
        for (int i = 0; i < Nx; ++i)
        {
            const double x = grid_.x0 + i * grid_.dx;

            const double c = concentrationAt(x, y, z);
            const float cf = (float)c;

            out[j * Nx + i] = cf;
            maxC = std::max(maxC, cf);
        }
    }
}
